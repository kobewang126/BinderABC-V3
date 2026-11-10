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

static uint32_t get32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(unsigned char *p, uint32_t x)
{ for (int i = 0; i < 4; i++) p[i] = (unsigned char)(x >> (8 * i)); }
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
static void receiver(int ready_fd)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    struct mini_binder_node_create node = {.cookie = 22};
    struct mini_binder_command request = {0}, reply = {.cmd = MINI_BC_REPLY};
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, result = {0};
    const unsigned char *data, *mapping;
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    mapping = map(fd);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(ready_fd, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id));
    close(ready_fd);
    assert(ioctl(fd, MINI_BINDER_READ, &request) == 0);
    data = (const unsigned char *)(uintptr_t)request.transaction.data_address;
    assert(request.transaction.object_count == 1 && get32(data) == MINI_BINDER_OBJECT_HANDLE);
    call.transaction.target_handle = get32(data + 4);
    assert(call.transaction.target_handle == 1);
    reply.transaction.id = request.transaction.id;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &result) == 0);
    assert(result.transaction.size == 4 && get32(mapping + result.transaction.reply_offset) == 77);
    free_reply(fd);
    munmap((void *)mapping, (size_t)sysconf(_SC_PAGESIZE));
    close(fd);
    _exit(0);
}
int main(void)
{
    int pipefd[2], fd, status;
    pid_t child;
    uint64_t receiver_node;
    void *mapping;
    struct mini_binder_node_create node = {.cookie = 11};
    struct mini_binder_command send = {.cmd = MINI_BC_TRANSACTION}, result = {0};
    struct mini_binder_command request = {0}, reply = {.cmd = MINI_BC_REPLY};
    assert(pipe(pipefd) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(pipefd[0]); receiver(pipefd[1]); }
    close(pipefd[1]);
    assert(read(pipefd[0], &receiver_node, sizeof(receiver_node)) == sizeof(receiver_node));
    close(pipefd[0]);
    fd = open("/dev/mini_binder", O_RDWR);
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    mapping = map(fd);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    send.transaction.target_node = receiver_node;
    send.transaction.size = 12;
    send.transaction.object_count = 1;
    put32(send.transaction.data, MINI_BINDER_OBJECT_LOCAL);
    put32(send.transaction.data + 4, 11);
    assert(ioctl(fd, MINI_BINDER_WRITE, &send) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &result) == 0);
    free_reply(fd);
    assert(ioctl(fd, MINI_BINDER_READ, &request) == 0);
    assert(request.transaction.target_node == node.node_id && request.transaction.object_cookie == 11);
    reply.transaction.id = request.transaction.id;
    reply.transaction.size = 4;
    put32(reply.transaction.data, 77);
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("local Node -> receiver Ref 1 -> original object: 77");
    return 0;
}
