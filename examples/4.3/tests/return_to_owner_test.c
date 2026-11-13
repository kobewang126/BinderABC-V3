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
static void free_reply(int fd)
{
    struct mini_binder_command cmd = {.cmd = MINI_BC_FREE_REPLY};
    assert(ioctl(fd, MINI_BINDER_WRITE, &cmd) == 0);
}
struct service_object { int (*invoke)(int); };
static int double_value(int value) { return value * 2; }
static const struct service_object object = { double_value };

static void peer(int send_id, int receive_id)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    uint64_t owner_node;
    uint32_t handle;
    const unsigned char *mapping, *payload;
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, result = {0};
    assert(fd >= 0);
    mapping = map(fd);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(send_id, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id));
    close(send_id);
    assert(read(receive_id, &owner_node, sizeof(owner_node)) == sizeof(owner_node));
    close(receive_id);
    assert(read_request(fd, &req) == 0);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    struct parcel_object received = read_object(&req.transaction, payload);
    assert(received.type == OBJECT_PARCEL_HANDLE);
    handle = (uint32_t)received.value;
    assert(handle == 1);
    reply.transaction.id = req.transaction.id;
    reply.transaction.size = 12;
    reply.transaction.object_count = 1;
    write_object(&reply.transaction, MINI_BINDER_OBJECT_HANDLE, handle);
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);

    call.transaction.target_node = owner_node;
    call.transaction.size = 12;
    call.transaction.object_count = 1;
    write_object(&call.transaction, MINI_BINDER_OBJECT_HANDLE, handle);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(read_reply(fd, &result) == 0);
    assert(result.transaction.size == 4 && get32(mapping + 2 * MINI_BINDER_MAX_DATA) == 42);
    free_reply(fd);
    munmap((void *)mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    _exit(0);
}

int main(void)
{
    int a_to_b[2], b_to_a[2], fd, status;
    uint64_t peer_node, cookie = (uint64_t)(uintptr_t)&object;
    pid_t child;
    unsigned char *mapping;
    const unsigned char *payload;
    const struct service_object *returned;
    struct mini_binder_node_create node = {.cookie = cookie};
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, result = {0};
    struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
    assert(pipe(a_to_b) == 0 && pipe(b_to_a) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(a_to_b[1]); close(b_to_a[0]); peer(b_to_a[1], a_to_b[0]); }
    close(a_to_b[0]); close(b_to_a[1]);
    assert(read(b_to_a[0], &peer_node, sizeof(peer_node)) == sizeof(peer_node)); close(b_to_a[0]);
    fd = open("/dev/mini_binder", O_RDWR);
    assert(fd >= 0);
    mapping = map(fd);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(a_to_b[1], &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id)); close(a_to_b[1]);

    call.transaction.target_node = peer_node;
    call.transaction.size = 12;
    call.transaction.object_count = 1;
    write_object(&call.transaction, MINI_BINDER_OBJECT_LOCAL, cookie);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(read_reply(fd, &result) == 0);
    struct parcel_object received = read_object(&result.transaction, mapping + 2 * MINI_BINDER_MAX_DATA);
    assert(received.type == OBJECT_PARCEL_LOCAL && received.value == cookie);
    returned = (const struct service_object *)(uintptr_t)received.value;
    assert(returned->invoke(21) == 42);
    free_reply(fd);

    assert(read_request(fd, &req) == 0);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    received = read_object(&req.transaction, payload);
    assert(received.type == OBJECT_PARCEL_LOCAL && received.value == cookie);
    returned = (const struct service_object *)(uintptr_t)received.value;
    reply.transaction.id = req.transaction.id;
    reply.transaction.size = 4;
    put32(reply.transaction.data, (uint32_t)returned->invoke(21));
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("reply Ref -> owner local object: 42");
    puts("request Ref -> owner local object: 42");
    return 0;
}
