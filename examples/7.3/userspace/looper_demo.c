// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "lab.h"

static void serve(int ready, int finish)
{
    int fd = open("/dev/mini_binder", O_RDWR); char byte;
    struct mini_binder_command event, reply = {.cmd = MINI_BC_REPLY};
    assert(fd >= 0); unsigned char *mapping = map_buffer(fd);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0); enter_loop(fd);
    assert(write(ready, "R", 1) == 1); close(ready);
    read_request(fd, &event);
    const unsigned char *data = (const unsigned char *)(uintptr_t)event.transaction.data_address;
    struct parcel_object received = read_object(&event.transaction, data);
    assert(received.type == OBJECT_PARCEL_HANDLE);
    uint32_t callback = (uint32_t)received.value;

    reply.transaction.size = 4; put32(reply.transaction.data, 11);
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    /* The first transaction is answered before starting the reverse call. */
    transact(fd, callback, 2, &event);
    assert(get32(mapping + event.transaction.reply_offset) == 22); free_reply(fd);
    assert(read(finish, &byte, 1) == 1); close(finish);
    unmap_close(fd, mapping); _exit(0);
}
int main(void)
{
    int ready[2], finish[2]; char byte; pid_t child;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event;
    assert(pipe(ready) == 0 && pipe(finish) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(finish[1]); serve(ready[1], finish[0]); }
    close(ready[1]); close(finish[0]);
    assert(read(ready[0], &byte, 1) == 1); close(ready[0]);
    int fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    unsigned char *mapping = map_buffer(fd);
    call.transaction.code = 1;
    object(&call.transaction, 0, MINI_BINDER_OBJECT_LOCAL, 77);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    read_reply(fd, &event);
    assert(get32(mapping + event.transaction.reply_offset) == 11); free_reply(fd);
    puts("ordinary READ: BR_REPLY, result=11");
    enter_loop(fd);
    puts("BC_ENTER_LOOPER: current thread can receive process requests");
    read_request(fd, &event); assert(event.transaction.object_cookie == 77);
    puts("same READ: BR_TRANSACTION, cookie=77");
    memset(&call, 0, sizeof(call)); call.cmd = MINI_BC_REPLY;

    call.transaction.size = 4; put32(call.transaction.data, 22);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    call.cmd = MINI_BC_EXIT_LOOPER;
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(write(finish[1], "F", 1) == 1); close(finish[1]); lab_wait_ok(child);
    unmap_close(fd, mapping);
    puts("BC_REPLY: reverse caller received 22; BC_EXIT_LOOPER completed");
    return 0;
}
