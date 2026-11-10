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
static void server(int ready, int finish)
{
    int binder = open("/dev/mini_binder", O_RDWR), data[2];
    char token;
    void *mapping;
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {.cmd = MINI_READ_REQUEST}, reply = {.cmd = MINI_BC_REPLY};
    assert(binder >= 0 && ioctl(binder, MINI_BINDER_CREATE_NODE, &node) == 0);
    mapping = map(binder);
    assert(write(ready, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id)); close(ready);
    assert(ioctl(binder, MINI_BINDER_READ, &req) == 0);
    assert(pipe(data) == 0 && write(data[1], "data", 4) == 4); close(data[1]);
    reply.transaction.id = req.transaction.id; reply.transaction.size = 12; reply.transaction.object_count = 1;
    put32(reply.transaction.data, MINI_BINDER_OBJECT_FD);
    put32(reply.transaction.data + 4, (uint32_t)data[0]);
    assert(ioctl(binder, MINI_BINDER_WRITE, &reply) == 0); close(data[0]);
    assert(read(finish, &token, 1) == 1); close(finish);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(binder); _exit(0);
}
int main(void)
{
    int ready[2], finish[2], binder, received, status;
    uint64_t target;
    char data[4];
    pid_t child;
    unsigned char *mapping, *payload;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command reply = {.cmd = MINI_READ_REPLY}, release = {.cmd = MINI_BC_FREE_REPLY};
    assert(pipe(ready) == 0 && pipe(finish) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(finish[1]); server(ready[1], finish[0]); }
    close(ready[1]); close(finish[0]);
    assert(read(ready[0], &target, sizeof(target)) == sizeof(target)); close(ready[0]);
    binder = open("/dev/mini_binder", O_RDWR); assert(binder >= 0); mapping = map(binder);
    call.transaction.target_node = target;
    assert(ioctl(binder, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(binder, MINI_BINDER_READ, &reply) == 0);
    payload = mapping + reply.transaction.reply_offset;
    assert(reply.transaction.object_count == 1 && get32(payload) == MINI_BINDER_OBJECT_FD);
    received = (int)get32(payload + 4);
    assert(read(received, data, sizeof(data)) == sizeof(data)); close(received);
    assert(ioctl(binder, MINI_BINDER_WRITE, &release) == 0);
    assert(write(finish[1], "x", 1) == 1); close(finish[1]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("reply fd %d read: %.*s\n", received, 4, data);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(binder);
    return 0;
}
