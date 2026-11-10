// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>

static void put32(unsigned char *p, uint32_t x)
{
    for (int i = 0; i < 4; i++) p[i] = (unsigned char)(x >> (8 * i));
}
static uint32_t get32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int device(void **mapping)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    assert(fd >= 0);
    *mapping = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(*mapping != MAP_FAILED);
    return fd;
}
static void transact(int fd, struct mini_binder_command *cmd, struct mini_binder_command *reply)
{
    assert(ioctl(fd, MINI_BINDER_WRITE, cmd) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, reply) == 0 && reply->cmd == MINI_BR_REPLY);
}
static void release_reply(int fd)
{
    struct mini_binder_command cmd = {.cmd = MINI_BC_FREE_REPLY};
    assert(ioctl(fd, MINI_BINDER_WRITE, &cmd) == 0);
}
static void provider(int pipefd)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    struct mini_binder_node_create nodes[2] = {{.cookie = 41}, {.cookie = 42}};
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0) != MAP_FAILED);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &nodes[0]) == 0);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &nodes[1]) == 0);
    assert(write(pipefd, &nodes[0].node_id, sizeof(nodes[0].node_id)) == sizeof(nodes[0].node_id));
    close(pipefd);
    for (;;) {
        struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
        const unsigned char *data;
        assert(ioctl(fd, MINI_BINDER_READ, &req) == 0);
        data = (const unsigned char *)(uintptr_t)req.transaction.data_address;
        reply.transaction.id = req.transaction.id;
        if (req.transaction.code == 5) {
            uint32_t index = get32(data);
            assert(index < 2);
            reply.transaction.size = 12;
            reply.transaction.object_count = 1;
            put32(reply.transaction.data, MINI_BINDER_OBJECT_LOCAL);
            put32(reply.transaction.data + 4, (uint32_t)nodes[index].cookie);
        } else {
            assert(req.transaction.object_cookie == 42);
            reply.transaction.size = 4;
            put32(reply.transaction.data, 99);
        }
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    }
}
static void recipient(int pipefd, int proceed)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {0}, call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command result = {0}, reply = {.cmd = MINI_BC_REPLY};
    const unsigned char *mapping, *data;
    char token;
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    mapping = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED && ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(pipefd, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id));
    close(pipefd);
    assert(ioctl(fd, MINI_BINDER_READ, &req) == 0);
    data = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    assert(req.transaction.object_count == 1 && req.transaction.object_offsets[0] == 0);
    assert(get32(data) == MINI_BINDER_OBJECT_HANDLE && get32(data + 4) == 1);
    call.transaction.target_handle = get32(data + 4);
    reply.transaction.id = req.transaction.id;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(read(proceed, &token, 1) == 1);
    close(proceed);
    call.transaction.code = 9;
    transact(fd, &call, &result);
    assert(result.transaction.size == 4 && get32(mapping) == 99);
    release_reply(fd);
    munmap((void *)mapping, (size_t)sysconf(_SC_PAGESIZE));
    close(fd);
    _exit(0);
}
int main(void)
{
    int p[2], proceed[2], fd, status;
    pid_t a, b;
    uint64_t provider_id, recipient_id;
    unsigned char *mapping;
    struct mini_binder_command req, reply;
    assert(pipe(p) == 0);
    a = fork(); assert(a >= 0);
    if (!a) { close(p[0]); provider(p[1]); _exit(1); }
    close(p[1]); assert(read(p[0], &provider_id, sizeof(provider_id)) == sizeof(provider_id)); close(p[0]);
    assert(pipe(p) == 0 && pipe(proceed) == 0);
    b = fork(); assert(b >= 0);
    if (!b) { close(p[0]); close(proceed[1]); recipient(p[1], proceed[0]); _exit(1); }
    close(proceed[0]);
    close(p[1]); assert(read(p[0], &recipient_id, sizeof(recipient_id)) == sizeof(recipient_id)); close(p[0]);
    fd = device((void **)&mapping);
    for (uint32_t i = 0; i < 2; i++) {
        memset(&req, 0, sizeof(req)); memset(&reply, 0, sizeof(reply));
        req.cmd = MINI_BC_TRANSACTION; req.transaction.target_node = provider_id;
        req.transaction.code = 5; req.transaction.size = 4; put32(req.transaction.data, i);
        transact(fd, &req, &reply);
        assert(get32(mapping) == MINI_BINDER_OBJECT_HANDLE && get32(mapping + 4) == i + 1);
        release_reply(fd);
    }
    memset(&req, 0, sizeof(req)); memset(&reply, 0, sizeof(reply));
    req.cmd = MINI_BC_TRANSACTION; req.transaction.target_node = recipient_id;
    req.transaction.size = 24; req.transaction.object_count = 2;
    req.transaction.object_offsets[1] = 12;
    put32(req.transaction.data, MINI_BINDER_OBJECT_HANDLE); put32(req.transaction.data + 4, 2);
    put32(req.transaction.data + 12, MINI_BINDER_OBJECT_HANDLE); put32(req.transaction.data + 16, 99);
    assert(ioctl(fd, MINI_BINDER_WRITE, &req) == -1 && errno == ENOENT);
    req.transaction.object_offsets[1] = 4;
    assert(ioctl(fd, MINI_BINDER_WRITE, &req) == -1 && errno == EINVAL);
    memset(&req, 0, sizeof(req));
    req.cmd = MINI_BC_TRANSACTION; req.transaction.target_node = recipient_id;
    req.transaction.size = 12; req.transaction.object_count = 1;
    put32(req.transaction.data, MINI_BINDER_OBJECT_HANDLE); put32(req.transaction.data + 4, 2);
    transact(fd, &req, &reply);
    assert(reply.transaction.status == 0 && reply.transaction.size == 0);
    release_reply(fd);
    assert(write(proceed[1], "R", 1) == 1);
    close(proceed[1]);
    assert(waitpid(b, &status, 0) == b && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    kill(a, SIGKILL); waitpid(a, &status, 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("handle 2 translated to recipient handle 1 and invoked successfully");
    return 0;
}
