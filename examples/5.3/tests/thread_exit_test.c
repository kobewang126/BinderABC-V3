// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
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
static void server(int ready, int stop)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    unsigned char byte;
    void *mapping;
    struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = map(fd);
    assert(write(ready, "R", 1) == 1); close(ready);
    for (int i = 0; i < 3; i++) {
        assert(ioctl(fd, MINI_BINDER_READ, &req) == 0);
        reply.transaction.id = req.transaction.id;
        reply.transaction.size = 1; reply.transaction.data[0] = (unsigned char)(11 + i * 10);
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    }
    assert(read(stop, &byte, 1) == 1); close(stop);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd); _exit(0);
}
struct arg { int fd; unsigned char *mapping; uint32_t offset; };
static void *registration(void *data)
{
    struct arg *arg = data;
    uint32_t id;
    assert(ioctl(arg->fd, MINI_BINDER_GET_THREAD_ID, &id) == 0 && id == 1);
    assert(ioctl(arg->fd, MINI_BINDER_THREAD_EXIT) == 0);
    assert(ioctl(arg->fd, MINI_BINDER_THREAD_EXIT) == -1 && errno == ENOENT);
    return NULL;
}
static void *leave_reply(void *data)
{
    struct arg *arg = data;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
    call.transaction.size = 1; call.transaction.data[0] = 20;
    assert(ioctl(arg->fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(arg->fd, MINI_BINDER_READ, &reply) == 0);
    assert(reply.transaction.size == 1 && reply.transaction.reply_offset < 2 * MINI_BINDER_MAX_DATA);
    arg->offset = reply.transaction.reply_offset;
    assert(arg->mapping[arg->offset] == 21);
    assert(ioctl(arg->fd, MINI_BINDER_THREAD_EXIT) == 0);
    return NULL;
}
static void *leave_unread(void *data)
{
    struct arg *arg = data;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    call.transaction.size = 1; call.transaction.data[0] = 10;
    assert(ioctl(arg->fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(arg->fd, MINI_BINDER_THREAD_EXIT) == 0);
    return NULL;
}
int main(void)
{
    int ready[2], stop[2], fd, status;
    pid_t child;
    unsigned char byte, *mapping;
    pthread_t thread;
    struct arg arg;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
    struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
    assert(pipe(ready) == 0 && pipe(stop) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(stop[1]); server(ready[1], stop[0]); }
    close(ready[1]); close(stop[0]);
    assert(read(ready[0], &byte, 1) == 1); close(ready[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    mapping = map(fd);
    arg = (struct arg){.fd = fd, .mapping = mapping};
    assert(ioctl(fd, MINI_BINDER_THREAD_EXIT) == -1 && errno == ENOENT);
    for (int i = 0; i < 12; i++) {
        assert(pthread_create(&thread, NULL, registration, &arg) == 0);
        assert(pthread_join(thread, NULL) == 0);
    }
    assert(pthread_create(&thread, NULL, leave_unread, &arg) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert(pthread_create(&thread, NULL, leave_reply, &arg) == 0);
    assert(pthread_join(thread, NULL) == 0);
    call.transaction.size = 1; call.transaction.data[0] = 30;
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &reply) == 0);
    assert(reply.transaction.reply_offset == arg.offset && mapping[arg.offset] == 31);
    assert(ioctl(fd, MINI_BINDER_WRITE, &release) == 0);
    assert(ioctl(fd, MINI_BINDER_THREAD_EXIT) == 0);
    assert(write(stop[1], "S", 1) == 1); close(stop[1]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    printf("12 exited threads reused record; reply offset %u reused\n", arg.offset);
    puts("explicit thread exit and reply cleanup passed");
    return 0;
}
