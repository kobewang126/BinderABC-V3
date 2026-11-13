// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include "../userspace/lab.h"

static void peer(int ready, int go, int finish)
{
    int fd = open("/dev/mini_binder", O_RDWR); char byte;
    unsigned char *mapping = map_buffer(fd);
    struct mini_binder_command event, reply = {.cmd = MINI_BC_REPLY};
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0); enter_loop(fd);
    assert(write(ready, "R", 1) == 1); close(ready);
    assert(read(go, &byte, 1) == 1); close(go);
    for (int i = 0; i < 2; i++) {
        read_request(fd, &event);
        reply.transaction.size = 4; put32(reply.transaction.data, 9);
        assert(mini_write(fd, &reply) == 0);
    }
    assert(read(finish, &byte, 1) == 1); close(finish);
    unmap_close(fd, mapping); _exit(0);
}
static void progress_test(void)
{
    int ready[2], go[2], finish[2]; char byte; pid_t child;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event;
    struct mini_binder_write_read bwr = {.write_size = sizeof(call), .write_buffer = (uintptr_t)&call};
    assert(pipe(ready) == 0 && pipe(go) == 0 && pipe(finish) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(go[1]); close(finish[1]); peer(ready[1], go[0], finish[0]); }
    close(ready[1]); close(go[0]); close(finish[0]); assert(read(ready[0], &byte, 1) == 1); close(ready[0]);
    int fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0); unsigned char *mapping = map_buffer(fd);
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, (void *)1) == -1 && errno == EFAULT);
    struct mini_binder_write_read invalid = {.write_size = 1};
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &invalid) == -1 && errno == EINVAL);
    invalid.write_size = sizeof(call); invalid.write_consumed = 1;
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &invalid) == -1 && errno == EINVAL);
    assert(ioctl(fd, _IOW(MINI_BINDER_IOC_MAGIC, 21, struct mini_binder_command), &call) == -1 && errno == ENOTTY);
    assert(ioctl(fd, _IOR(MINI_BINDER_IOC_MAGIC, 22, struct mini_binder_command), &event) == -1 && errno == ENOTTY);
    long page = sysconf(_SC_PAGESIZE);
    struct mini_binder_write_read *readonly = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE,
                                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(readonly != MAP_FAILED); *readonly = bwr;
    assert(mprotect(readonly, (size_t)page, PROT_READ) == 0);
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, readonly) == -1 && errno == EFAULT);
    assert(munmap(readonly, (size_t)page) == 0);
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &bwr) == 0 && bwr.write_consumed == sizeof(call));
    /* The server cannot even read the request before this pipe message. */
    puts("write-only returned before server received request");
    assert(write(go[1], "G", 1) == 1); close(go[1]);
    bwr.read_size = sizeof(event); bwr.read_buffer = 1;
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &bwr) == -1 && errno == EFAULT);
    assert(bwr.write_consumed == sizeof(call) && bwr.read_consumed == 0);
    bwr.read_buffer = (uintptr_t)&event;
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &bwr) == 0 && bwr.read_consumed == sizeof(event));
    assert(event.cmd == MINI_BR_REPLY && get32(mapping + event.transaction.reply_offset) == 9);
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &bwr) == 0); /* Both directions already consumed. */
    free_reply(fd);
    memset(&bwr, 0, sizeof(bwr));
    bwr.write_size = sizeof(call); bwr.write_buffer = (uintptr_t)&call;
    bwr.read_size = sizeof(event); bwr.read_buffer = 1;
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &bwr) == -1 && errno == EFAULT);
    assert(bwr.write_consumed == sizeof(call) && bwr.read_consumed == 0);
    bwr.read_buffer = (uintptr_t)&event;
    assert(ioctl(fd, MINI_BINDER_WRITE_READ, &bwr) == 0);
    assert(event.cmd == MINI_BR_REPLY && get32(mapping + event.transaction.reply_offset) == 9);
    free_reply(fd); puts("read retry preserved write_consumed: each request submitted exactly once");
    assert(write(finish[1], "F", 1) == 1); close(finish[1]); lab_wait_ok(child); unmap_close(fd, mapping);
}
static int client(unsigned char **mapping)
{
    int fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0); *mapping = map_buffer(fd); return fd;
}
static void lifecycle_test(void)
{
    unsigned char *server_map, *mapping;
    int server = client(&server_map), fd;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event, reply = {.cmd = MINI_BC_REPLY};
    assert(ioctl(server, MINI_BINDER_REGISTER_MANAGER) == 0); enter_loop(server);
    for (int i = 0; i < 8; i++) {
        fd = client(&mapping); assert(mini_write(fd, &call) == 0); unmap_close(fd, mapping);
    }
    puts("closing callers before receive reclaimed pending slots");
    for (int i = 0; i < 8; i++) {
        fd = client(&mapping); assert(mini_write(fd, &call) == 0); read_request(server, &event);
        unmap_close(fd, mapping);
        assert(mini_write(server, &reply) == -1 && errno == ECANCELED);
    }
    puts("closing callers after receive detached pointers; late replies rejected");
    fd = client(&mapping); assert(mini_write(fd, &call) == 0);
    assert(mini_write(fd, &call) == -1 && errno == EBUSY);
    assert(ioctl(fd, MINI_BINDER_THREAD_EXIT) == 0); /* Also cancels an unread request. */
    assert(mini_write(fd, &call) == 0); read_request(server, &event);

    assert(mini_write(server, &reply) == 0);
    unmap_close(server, server_map); /* Completed reply must survive server close. */
    read_reply(fd, &event); free_reply(fd); unmap_close(fd, mapping);
    puts("thread-exit cancellation and completed reply after server close passed");
}
int main(void)
{
    progress_test(); lifecycle_test(); return 0;
}
