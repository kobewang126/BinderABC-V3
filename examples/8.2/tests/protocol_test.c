// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <limits.h>
#include <sys/resource.h>
#include "../userspace/lab.h"

enum { BAD_BATCH = 1, ALL_OBJECTS, NINTH_OBJECT, ECHO, FILES, STOP };
static void serve(int ready, int finish)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    unsigned char *mapping;
    assert(fd >= 0); mapping = map_buffer(fd);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0); enter_loop(fd);
    assert(write(ready, "R", 1) == 1); close(ready);
    for (;;) {
        struct mini_binder_command event = {0}, reply = {.cmd = MINI_BC_REPLY};
        const unsigned char *data;
        int pipefd[2] = {-1, -1};
        read_request(fd, &event);
        data = (const unsigned char *)(uintptr_t)event.transaction.data_address;

        switch (event.transaction.code) {
        case BAD_BATCH:
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, 900);
            object(&reply.transaction, 1, MINI_BINDER_OBJECT_FD, INT_MAX);
            assert(mini_write(fd, &reply) == -1 && errno == EBADF);
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, 0);
            reply.transaction.object_count = 1; reply.transaction.size = 12;
            assert(mini_write(fd, &reply) == -1 && errno == EINVAL);
            memset(&reply.transaction, 0, sizeof(reply.transaction));
             reply.transaction.status = -EBADF;
            break;
        case ALL_OBJECTS:
            for (unsigned int i = 0; i < 8; i++) object(&reply.transaction, i, MINI_BINDER_OBJECT_LOCAL, 100 + i);
            break;
        case NINTH_OBJECT:
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, 108);
            assert(mini_write(fd, &reply) == -1 && errno == ENOSPC);
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, 100);
            break;
        case ECHO:
            assert(event.transaction.object_count == 1 && get32(data) == MINI_BINDER_OBJECT_LOCAL && get32(data + 4) == 100);
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, get32(data + 4));
            break;
        case FILES: {
            int input = (int)get32(data + 4); char bytes[4];
            assert(get32(data) == MINI_BINDER_OBJECT_FD);
            assert(read(input, bytes, 4) == 4 && !memcmp(bytes, "ping", 4)); close(input);
            assert(pipe(pipefd) == 0 && write(pipefd[1], "pong", 4) == 4); close(pipefd[1]);
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_FD, (uint64_t)pipefd[0]);
            break;
        }
        default: assert(event.transaction.code == STOP); break;
        }
        assert(mini_write(fd, &reply) == 0);
        if (pipefd[0] >= 0) close(pipefd[0]);
        if (event.transaction.code == STOP) break;
    }
    { char byte; assert(read(finish, &byte, 1) == 1); } close(finish);
    unmap_close(fd, mapping); _exit(0);
}
int main(void)
{
    int ready[2], finish[2], fd, pipes[2], received;
    pid_t child;
    long page = sysconf(_SC_PAGESIZE);
    unsigned char *mapping;
    struct mini_binder_command event = {0}, call = {.cmd = MINI_BC_TRANSACTION};
    assert(pipe(ready) == 0 && pipe(finish) == 0); child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(finish[1]); serve(ready[1], finish[0]); }
    close(finish[0]); close(ready[1]); { char byte; assert(read(ready[0], &byte, 1) == 1); } close(ready[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0); mapping = map_buffer(fd);
    assert(ioctl(fd, _IO(MINI_BINDER_IOC_MAGIC, 255)) == -1 && errno == ENOTTY);
    assert(ioctl(fd, MINI_BINDER_GET_VERSION, (void *)1) == -1 && errno == EFAULT);
    call.cmd = 999; assert(mini_write(fd, &call) == -1 && errno == EINVAL);
    call.cmd = MINI_BC_TRANSACTION; call.transaction.target_handle = 8;
    assert(mini_write(fd, &call) == -1 && errno == ENOENT);
    call.transaction.target_handle = 0;
    for (int i = 0; i < 10; i++) {
        transact(fd, 0, BAD_BATCH, &event); assert(event.transaction.status == -EBADF); free_reply(fd);
    }
    transact(fd, 0, ALL_OBJECTS, &event);
    assert(event.transaction.object_count == 8);
    for (unsigned int i = 0; i < 8; i++) {
        const unsigned char *p = mapping + event.transaction.reply_offset + i * 12;
        assert(get32(p) == MINI_BINDER_OBJECT_HANDLE && get32(p + 4) == i + 1);
    }
    free_reply(fd);
    transact(fd, 0, ALL_OBJECTS, &event); free_reply(fd);
    transact(fd, 0, NINTH_OBJECT, &event);
    assert(get32(mapping + event.transaction.reply_offset + 4) == 1); free_reply(fd);
    puts("failed translations rolled back; eight Nodes fit; repeated objects reuse handles");
    call.transaction.code = ECHO; object(&call.transaction, 0, MINI_BINDER_OBJECT_HANDLE, 1);
    assert(mini_write(fd, &call) == 0); read_reply(fd, &event);
    assert(get32(mapping + event.transaction.reply_offset) == MINI_BINDER_OBJECT_HANDLE && get32(mapping + event.transaction.reply_offset + 4) == 1);
    free_reply(fd); puts("handle returned to owner as LOCAL; reply reused original handle");
    assert(pipe(pipes) == 0 && write(pipes[1], "ping", 4) == 4); close(pipes[1]);
    call.transaction.code = FILES; object(&call.transaction, 0, MINI_BINDER_OBJECT_FD, (uint64_t)pipes[0]);
    assert(mini_write(fd, &call) == 0); close(pipes[0]);
    {
        unsigned char *guard = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(guard != MAP_FAILED && mprotect(guard + page, (size_t)page, PROT_NONE) == 0);
        struct mini_binder_command *bad = (void *)(guard + page - sizeof(uint32_t));
        bad->cmd = 0;
        assert(mini_read(fd, bad) == -1 && errno == EFAULT);
        munmap(guard, (size_t)page * 2);
    }
    {
        struct rlimit old, small; int saved[128], count = 0;
        assert(getrlimit(RLIMIT_NOFILE, &old) == 0 && old.rlim_max >= 64);
        small = old; small.rlim_cur = 64; assert(setrlimit(RLIMIT_NOFILE, &small) == 0);
        while (count < 128) { int filler = open("/dev/null", O_RDONLY); if (filler < 0) { assert(errno == EMFILE); break; } saved[count++] = filler; }
        assert(count < 128);
        memset(&event, 0, sizeof(event));
        assert(mini_read(fd, &event) == -1 && errno == EMFILE);
        while (count) close(saved[--count]);
        assert(setrlimit(RLIMIT_NOFILE, &old) == 0);
    }
    read_reply(fd, &event);
    received = (int)get32(mapping + event.transaction.reply_offset + 4);
    assert(fcntl(received, F_GETFD) & FD_CLOEXEC);
    { char bytes[4]; assert(read(received, bytes, 4) == 4 && !memcmp(bytes, "pong", 4)); } close(received);
    free_reply(fd); puts("request/reply fds, EFAULT and EMFILE retry passed");
    transact(fd, 0, STOP, &event); free_reply(fd);
    assert(write(finish[1], "F", 1) == 1); close(finish[1]);
    lab_wait_ok(child); unmap_close(fd, mapping); return 0;
}
