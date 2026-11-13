// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include <test_io.h>
#include <parcel_binding.h>

static uint32_t get32(const unsigned char *data)
{
    struct parcel p; uint32_t value;
    parcel_init(&p, (void *)data, 4); p.size = 4;
    assert(parcel_read_u32(&p, &value) == 0); return value;
}
static void put32(unsigned char *data, uint32_t value)
{
    struct parcel p; parcel_init(&p, data, 4);
    assert(parcel_write_u32(&p, value) == 0);
}
static struct parcel_object read_object(const struct mini_binder_transaction_data *tx,
                                        const void *data)
{
    struct object_parcel p;
    struct parcel_object object;
    assert(parcel_from_transaction(&p, tx, data) == 0 && p.count == 1);
    assert(object_parcel_read_next(&p, &object) == 0);
    return object;
}
static void write_object(struct mini_binder_transaction_data *tx, uint32_t type, uint64_t value)
{
    struct object_parcel p;
    object_parcel_init(&p, tx->data, sizeof(tx->data));
    assert(object_parcel_write(&p, type, value) == 0);
    parcel_to_transaction(tx, &p);
}
static void *map(int fd)
{
    void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(p != MAP_FAILED);
    return p;
}
static void release_reply(int fd)
{
    struct mini_binder_command cmd = {.cmd = MINI_BC_FREE_REPLY};
    assert(ioctl(fd, MINI_BINDER_WRITE, &cmd) == 0);
}
struct object { int (*invoke)(int); };
static int twice(int value) { return value * 2; }
static const struct object local = {twice};

static void receiver(int ready)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    unsigned char *mapping;
    const unsigned char *payload;
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {.cmd = MINI_READ_REQUEST}, reply = {.cmd = MINI_BC_REPLY};
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, result = {.cmd = MINI_READ_REPLY};
    assert(fd >= 0);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    mapping = map(fd);
    assert(write(ready, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id)); close(ready);
    assert(read_request(fd, &req) == 0 && req.cmd == MINI_BR_TRANSACTION);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    struct parcel_object received = read_object(&req.transaction, payload);
    assert(received.type == OBJECT_PARCEL_HANDLE);
    call.transaction.target_handle = (uint32_t)received.value;
    assert(call.transaction.target_handle == 1);

    reply.transaction.id = req.transaction.id;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    call.transaction.size = 4; put32(call.transaction.data, 7);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(read_reply(fd, &result) == 0 && result.cmd == MINI_BR_REPLY);
    assert(result.transaction.size == 4 && get32(mapping + (2 * MINI_BINDER_MAX_DATA)) == 14);
    release_reply(fd);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd); _exit(0);
}

int main(void)
{
    int ready[2], fd, status;
    uint64_t target, cookie = (uint64_t)(uintptr_t)&local;
    pid_t child;
    void *mapping;
    const unsigned char *payload;
    struct mini_binder_node_create node = {.cookie = cookie};
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, result = {.cmd = MINI_READ_REPLY};
    struct mini_binder_command req = {.cmd = MINI_READ_REQUEST}, reply = {.cmd = MINI_BC_REPLY};
    assert(pipe(ready) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); receiver(ready[1]); }
    close(ready[1]); assert(read(ready[0], &target, sizeof(target)) == sizeof(target)); close(ready[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    mapping = map(fd);
    call.transaction.target_node = target;
    write_object(&call.transaction, OBJECT_PARCEL_LOCAL, cookie);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(read_reply(fd, &result) == 0); release_reply(fd);
    assert(read_request(fd, &req) == 0 && req.cmd == MINI_BR_TRANSACTION);
    assert(req.transaction.object_cookie == cookie && req.transaction.size == 4);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
     reply.transaction.size = 4;
    put32(reply.transaction.data, (uint32_t)local.invoke((int)get32(payload)));
    reply.transaction.id = req.transaction.id;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("ordinary client passed a local object and handled its call: twice(7) = 14");
    return 0;
}
