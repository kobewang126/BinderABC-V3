// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>

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
static void other_server(int ready, int stop)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
    unsigned char byte;
    void *mapping;
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    mapping = map(fd);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(ready, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id)); close(ready);
    assert(ioctl(fd, MINI_BINDER_READ, &req) == 0);

    reply.transaction.size = 1; reply.transaction.data[0] = 99;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(read(stop, &byte, 1) == 1); close(stop);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd); _exit(0);
}
static void incoming_client(int inherited_fd)
{
    int fd;
    unsigned char *mapping;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
    close(inherited_fd);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    mapping = map(fd);
    call.transaction.size = 1; call.transaction.data[0] = 55;
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &reply) == 0);
    assert(mapping[reply.transaction.reply_offset] == 56);
    release_reply(fd);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd); _exit(0);
}
struct caller { int fd; uint64_t target; unsigned char *mapping; const unsigned char *request; };
static void *outgoing_client(void *arg)
{
    struct caller *caller = arg;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
    call.transaction.target_node = caller->target;
    assert(ioctl(caller->fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(caller->fd, MINI_BINDER_READ, &reply) == 0);
    assert(reply.transaction.reply_offset >= 2 * MINI_BINDER_MAX_DATA);
    assert(caller->mapping[reply.transaction.reply_offset] == 99);
    assert(*caller->request == 55);
    release_reply(caller->fd);
    return NULL;
}
int main(void)
{
    int ready[2], stop[2], fd, status;
    pid_t other, client;
    pthread_t thread;
    uint64_t target;
    unsigned char *mapping;
    struct caller caller;
    struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
    assert(pipe(ready) == 0 && pipe(stop) == 0);
    other = fork(); assert(other >= 0);
    if (!other) { close(ready[0]); close(stop[1]); other_server(ready[1], stop[0]); }
    close(ready[1]); close(stop[0]);
    assert(read(ready[0], &target, sizeof(target)) == sizeof(target)); close(ready[0]);
    fd = open("/dev/mini_binder", O_RDWR);
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = map(fd);
    client = fork(); assert(client >= 0);
    if (!client) incoming_client(fd);
    assert(ioctl(fd, MINI_BINDER_READ, &req) == 0 && req.transaction.size == 1);
    caller = (struct caller){.fd = fd, .target = target, .mapping = mapping,
        .request = (const unsigned char *)(uintptr_t)req.transaction.data_address};
    assert(*caller.request == 55);
    assert(pthread_create(&thread, NULL, outgoing_client, &caller) == 0);
    assert(pthread_join(thread, NULL) == 0 && *caller.request == 55);

    reply.transaction.size = 1; reply.transaction.data[0] = *caller.request + 1;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(waitpid(client, &status, 0) == client && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(write(stop[1], "S", 1) == 1); close(stop[1]);
    assert(waitpid(other, &status, 0) == other && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("server request and outgoing reply occupy separate mapping regions");
    return 0;
}
