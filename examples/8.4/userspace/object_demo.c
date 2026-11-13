// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "lab.h"

enum { GET_OBJECT = 1, SUBTRACT = 2, STOP = 3 };
struct calculator { uint64_t cookie; int (*invoke)(int, int); };
static int subtract(int a, int b) { return a - b; }

static void serve(int ready, int finish)
{
    const struct calculator calc = {.cookie = 77, .invoke = subtract};
    int fd = open("/dev/mini_binder", O_RDWR);
    unsigned char *mapping;
    assert(fd >= 0); mapping = map_buffer(fd);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    enter_loop(fd);
    assert(write(ready, "R", 1) == 1); close(ready);
    for (;;) {
        struct mini_binder_command event = {0}, reply = {.cmd = MINI_BC_REPLY};
        const unsigned char *data;
        read_request(fd, &event);
        data = (const unsigned char *)(uintptr_t)event.transaction.data_address;

        if (event.transaction.code == GET_OBJECT) {
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, calc.cookie);
        } else if (event.transaction.code == SUBTRACT) {
            assert(event.transaction.object_cookie == calc.cookie && event.transaction.size == 8);
            reply.transaction.size = 4;
            put32(reply.transaction.data, (uint32_t)calc.invoke((int)get32(data), (int)get32(data + 4)));
        } else assert(event.transaction.code == STOP);
        assert(mini_write(fd, &reply) == 0);
        if (event.transaction.code == STOP) break;
    }
    { char byte; assert(read(finish, &byte, 1) == 1); } close(finish);
    unmap_close(fd, mapping); _exit(0);
}
int main(void)
{
    int ready[2], finish[2], fd;
    pid_t child;
    unsigned char *mapping;
    uint32_t handles[2];
    struct mini_binder_command event = {0}, call = {.cmd = MINI_BC_TRANSACTION};
    assert(pipe(ready) == 0 && pipe(finish) == 0); child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(finish[1]); serve(ready[1], finish[0]); }
    close(finish[0]); close(ready[1]); { char byte; assert(read(ready[0], &byte, 1) == 1); } close(ready[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0); mapping = map_buffer(fd);
    for (unsigned int i = 0; i < 2; i++) {
        transact(fd, 0, GET_OBJECT, &event);
        const unsigned char *data = mapping + event.transaction.reply_offset;
        assert(get32(data) == MINI_BINDER_OBJECT_HANDLE);
        handles[i] = get32(data + 4);
        printf("GET_OBJECT %u: LOCAL(cookie=77) -> client handle %u\n", i + 1, handles[i]);
        free_reply(fd);
    }
    assert(handles[0] == handles[1]);
    call.transaction.target_handle = handles[0]; call.transaction.code = SUBTRACT;
    call.transaction.size = 8; put32(call.transaction.data, 7); put32(call.transaction.data + 4, 3);
    assert(mini_write(fd, &call) == 0); read_reply(fd, &event);
    assert(get32(mapping + event.transaction.reply_offset) == 4);
    printf("handle %u: sub(7, 3) = %u\n", handles[0], get32(mapping + event.transaction.reply_offset));
    free_reply(fd);
    transact(fd, 0, STOP, &event); free_reply(fd);
    assert(write(finish[1], "F", 1) == 1); close(finish[1]);
    lab_wait_ok(child); unmap_close(fd, mapping);
    puts("same local object reused; handle call passed");
    return 0;
}
