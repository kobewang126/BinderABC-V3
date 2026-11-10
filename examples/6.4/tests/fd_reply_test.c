// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>

static void put32(unsigned char *p, uint32_t n)
{ for (int i = 0; i < 4; i++) p[i] = (unsigned char)(n >> (8 * i)); }
static uint32_t get32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void *map(int fd)
{
    void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(p != MAP_FAILED);
    return p;
}
static void free_reply(int fd)
{
    struct mini_binder_command cmd = {.cmd = MINI_BC_FREE_REPLY};
    assert(ioctl(fd, MINI_BINDER_WRITE, &cmd) == 0);
}
static int first_free(void)
{
    for (int i = 0; i < 512; i++) if (fcntl(i, F_GETFD) == -1 && errno == EBADF) return i;
    return -1;
}
static void receive_request(int fd, struct mini_binder_command *req)
{
    memset(req, 0, sizeof(*req)); req->cmd = MINI_READ_REQUEST;
    assert(ioctl(fd, MINI_BINDER_READ, req) == 0 && req->cmd == MINI_BR_TRANSACTION);
}
static void send_fd_reply(int fd, uint64_t id, int data_fd)
{
    struct mini_binder_command reply = {.cmd = MINI_BC_REPLY};
    reply.transaction.id = id; reply.transaction.size = 12; reply.transaction.object_count = 1;
    put32(reply.transaction.data, MINI_BINDER_OBJECT_FD);
    put32(reply.transaction.data + 4, (uint32_t)data_fd);
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
}
static void send_pipe_and_wait(int binder, uint64_t id, int notify)
{
    int p[2];
    char byte;
    assert(pipe(p) == 0);
    send_fd_reply(binder, id, p[1]);
    close(p[1]);
    assert(write(notify, "H", 1) == 1);
    assert(read(p[0], &byte, 1) == 0); close(p[0]);
    assert(write(notify, "F", 1) == 1);
}
static void server(int notify, int stop)
{
    int binder = open("/dev/mini_binder", O_RDWR), data_fd, duplicate, leak_pipe[2];
    char path[] = "/tmp/binderabc-fd-reply-XXXXXX", byte;
    void *mapping;
    struct mini_binder_node_create node = {.cookie = 123};
    struct mini_binder_command req = {0}, invalid = {.cmd = MINI_BC_REPLY}, reply = {.cmd = MINI_BC_REPLY};
    assert(binder >= 0 && ioctl(binder, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = map(binder);
    assert(ioctl(binder, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(notify, "R", 1) == 1);
    receive_request(binder, &req);
    data_fd = mkstemp(path); assert(data_fd >= 0 && unlink(path) == 0);
    assert(write(data_fd, "data", 4) == 4 && lseek(data_fd, 0, SEEK_SET) == 0);
    duplicate = dup(data_fd); assert(duplicate >= 0);
    invalid.transaction.id = req.transaction.id; invalid.transaction.size = 36; invalid.transaction.object_count = 3;
    invalid.transaction.object_offsets[1] = 12; invalid.transaction.object_offsets[2] = 24;
    put32(invalid.transaction.data, MINI_BINDER_OBJECT_LOCAL); put32(invalid.transaction.data + 4, 123);
    assert(pipe(leak_pipe) == 0);
    put32(invalid.transaction.data + 12, MINI_BINDER_OBJECT_FD); put32(invalid.transaction.data + 16, (uint32_t)leak_pipe[1]);
    put32(invalid.transaction.data + 24, MINI_BINDER_OBJECT_FD); put32(invalid.transaction.data + 28, INT_MAX);
    assert(ioctl(binder, MINI_BINDER_WRITE, &invalid) == -1 && errno == EBADF);
    assert(close(leak_pipe[1]) == 0 && read(leak_pipe[0], &byte, 1) == 0);
    assert(close(leak_pipe[0]) == 0);
    put32(invalid.transaction.data + 16, (uint32_t)data_fd);
    put32(invalid.transaction.data + 28, (uint32_t)duplicate);
    assert(ioctl(binder, MINI_BINDER_WRITE, &invalid) == 0);
    assert(close(data_fd) == 0 && close(duplicate) == 0 && write(notify, "C", 1) == 1);

    receive_request(binder, &req);
    send_pipe_and_wait(binder, req.transaction.id, notify);

    receive_request(binder, &req);
    send_pipe_and_wait(binder, req.transaction.id, notify);

    receive_request(binder, &req);
    reply.transaction.id = req.transaction.id; reply.transaction.size = 4;
    put32(reply.transaction.data, 55);
    assert(ioctl(binder, MINI_BINDER_WRITE, &reply) == 0);

    receive_request(binder, &req);
    send_pipe_and_wait(binder, req.transaction.id, notify);
    assert(read(stop, &byte, 1) == 1);
    close(notify); close(stop); munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(binder); _exit(0);
}
int main(void)
{
    int notify[2], stop[2], binder, status, free_before, saved[128], count = 0, received, received2, flags, spare, spare2;
    long page = sysconf(_SC_PAGESIZE);
    char byte, data[4];
    pid_t child;
    void *guard;
    unsigned char *mapping, *payload;
    struct rlimit original, limit;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, reply = {.cmd = MINI_READ_REPLY};
    assert(pipe(notify) == 0 && pipe(stop) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(notify[0]); close(stop[1]); server(notify[1], stop[0]); }
    close(notify[1]); close(stop[0]);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'R');
    binder = open("/dev/mini_binder", O_RDWR); assert(binder >= 0); mapping = map(binder);
    assert(ioctl(binder, MINI_BINDER_WRITE, &call) == 0);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'C');

    guard = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(guard != MAP_FAILED && mprotect((char *)guard + page, (size_t)page, PROT_NONE) == 0);
    free_before = first_free(); assert(free_before >= 0);
    {
        struct mini_binder_command *bad = (void *)((char *)guard + page - sizeof(uint32_t));
        bad->cmd = MINI_READ_REPLY;
        assert(ioctl(binder, MINI_BINDER_READ, bad) == -1 && errno == EFAULT);
    }
    assert(first_free() == free_before);
    munmap(guard, (size_t)page * 2);

    assert(getrlimit(RLIMIT_NOFILE, &original) == 0 && original.rlim_max >= 64);
    limit = original; limit.rlim_cur = 64; assert(setrlimit(RLIMIT_NOFILE, &limit) == 0);
    while (count < (int)(sizeof(saved) / sizeof(saved[0]))) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd < 0) { assert(errno == EMFILE); break; }
        saved[count++] = fd;
    }
    assert(count > 0 && count < (int)(sizeof(saved) / sizeof(saved[0])));
    spare = saved[--count]; close(spare);
    assert(ioctl(binder, MINI_BINDER_READ, &reply) == -1 && errno == EMFILE);
    assert(first_free() == spare);
    spare2 = saved[--count]; close(spare2);
    reply.cmd = MINI_READ_REPLY;
    assert(ioctl(binder, MINI_BINDER_READ, &reply) == 0 && reply.transaction.object_count == 3);
    assert(reply.transaction.object_offsets[0] == 0 && reply.transaction.object_offsets[1] == 12 && reply.transaction.object_offsets[2] == 24);
    payload = mapping + reply.transaction.reply_offset;
    assert(get32(payload) == MINI_BINDER_OBJECT_HANDLE && get32(payload + 4) == 1);
    assert(get32(payload + 12) == MINI_BINDER_OBJECT_FD);
    assert(get32(payload + 24) == MINI_BINDER_OBJECT_FD);
    received = (int)get32(payload + 16); received2 = (int)get32(payload + 28);
    assert((received == spare && received2 == spare2) || (received == spare2 && received2 == spare));
    flags = fcntl(received, F_GETFD); assert(flags >= 0 && (flags & FD_CLOEXEC));
    flags = fcntl(received2, F_GETFD); assert(flags >= 0 && (flags & FD_CLOEXEC));
    assert(read(received, data, 2) == 2 && read(received2, data + 2, 2) == 2 && memcmp(data, "data", 4) == 0);
    close(received); close(received2); free_reply(binder);
    while (count) close(saved[--count]);
    assert(setrlimit(RLIMIT_NOFILE, &original) == 0);

    assert(ioctl(binder, MINI_BINDER_WRITE, &call) == 0);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'H');
    assert(ioctl(binder, MINI_BINDER_THREAD_EXIT) == 0);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'F');

    assert(ioctl(binder, MINI_BINDER_WRITE, &call) == 0);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'H');
    free_reply(binder);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'F');

    assert(ioctl(binder, MINI_BINDER_WRITE, &call) == 0);
    reply.cmd = MINI_READ_REPLY;
    assert(ioctl(binder, MINI_BINDER_READ, &reply) == 0);
    assert(reply.transaction.size == 4 && get32(mapping + reply.transaction.reply_offset) == 55);
    free_reply(binder);

    assert(ioctl(binder, MINI_BINDER_WRITE, &call) == 0);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'H');
    munmap(mapping, (size_t)page); close(binder);
    assert(read(notify[0], &byte, 1) == 1 && byte == 'F');
    assert(write(stop[1], "S", 1) == 1); close(stop[1]); close(notify[0]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("reply fds %d and %d after EFAULT and EMFILE retries: %.*s\n", received, received2, 4, data);
    puts("unread fd replies released by thread exit, free and file close");
    puts("subsequent transaction returned 55");
    return 0;
}
