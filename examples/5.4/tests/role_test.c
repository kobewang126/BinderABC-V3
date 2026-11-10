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
    assert(ioctl(fd, MINI_BINDER_READ, &req) == 0 && req.cmd == MINI_BR_TRANSACTION);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    assert(req.transaction.object_count == 1 && get32(payload) == MINI_BINDER_OBJECT_HANDLE);
    call.transaction.target_handle = get32(payload + 4);
    assert(call.transaction.target_handle == 1);
    reply.transaction.id = req.transaction.id;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    call.transaction.size = 4; put32(call.transaction.data, 7);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &result) == 0 && result.cmd == MINI_BR_REPLY);
    assert(result.transaction.size == 4 && get32(mapping + result.transaction.reply_offset) == 14);
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
    call.transaction.size = 12; call.transaction.object_count = 1;
    put32(call.transaction.data, MINI_BINDER_OBJECT_LOCAL);
    put32(call.transaction.data + 4, (uint32_t)cookie);
    put32(call.transaction.data + 8, (uint32_t)(cookie >> 32));
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &result) == 0); release_reply(fd);
    assert(ioctl(fd, MINI_BINDER_READ, &req) == 0 && req.cmd == MINI_BR_TRANSACTION);
    assert(req.transaction.object_cookie == cookie && req.transaction.size == 4);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    reply.transaction.id = req.transaction.id; reply.transaction.size = 4;
    put32(reply.transaction.data, (uint32_t)local.invoke((int)get32(payload)));
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("ordinary client passed a local object and handled its call: twice(7) = 14");
    return 0;
}
