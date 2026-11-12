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
        reply.transaction.id = event.transaction.id;
        if (event.transaction.code == GET_OBJECT) {
            object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, calc.cookie);
        } else if (event.transaction.code == SUBTRACT) {
            assert(event.transaction.object_cookie == calc.cookie && event.transaction.size == 8);
            struct object_parcel request, result;
            uint32_t a, b;
            assert(parcel_from_transaction(&request, &event.transaction, data) == 0);
            assert(parcel_read_u32(&request.bytes, &a) == 0);
            assert(parcel_read_u32(&request.bytes, &b) == 0);
            object_parcel_init(&result, reply.transaction.data, sizeof(reply.transaction.data));
            assert(parcel_write_u32(&result.bytes, (uint32_t)calc.invoke((int)a, (int)b)) == 0);
            parcel_to_transaction(&reply.transaction, &result);
        } else assert(event.transaction.code == STOP);
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
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
        struct parcel_object received = read_object(&event.transaction, data);
        assert(received.type == OBJECT_PARCEL_HANDLE);
        handles[i] = (uint32_t)received.value;
        printf("GET_OBJECT %u: LOCAL(cookie=77) -> client handle %u\n", i + 1, handles[i]);
        free_reply(fd);
    }
    assert(handles[0] == handles[1]);
    call.transaction.target_handle = handles[0]; call.transaction.code = SUBTRACT;
    struct object_parcel arguments;
    object_parcel_init(&arguments, call.transaction.data, sizeof(call.transaction.data));
    assert(parcel_write_u32(&arguments.bytes, 7) == 0);
    assert(parcel_write_u32(&arguments.bytes, 3) == 0);
    parcel_to_transaction(&call.transaction, &arguments);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0); read_reply(fd, &event);
    assert(get32(mapping + event.transaction.reply_offset) == 4);
    printf("handle %u: sub(7, 3) = %u\n", handles[0], get32(mapping + event.transaction.reply_offset));
    free_reply(fd);
    transact(fd, 0, STOP, &event); free_reply(fd);
    assert(write(finish[1], "F", 1) == 1); close(finish[1]);
    lab_wait_ok(child); unmap_close(fd, mapping);
    puts("same local object reused; handle call passed");
    return 0;
}
