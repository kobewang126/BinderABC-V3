// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include "../userspace/command.h"

struct worker {
    int fd;
    unsigned char *mapping;
    unsigned char input;
    uint32_t thread_id;
    uint32_t offset;
    unsigned char result;
    pthread_barrier_t *phase;
};

static void barrier(pthread_barrier_t *phase)
{
    int ret = pthread_barrier_wait(phase);
    assert(ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *client_thread(void *arg)
{
    struct worker *worker = arg;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command reply = {0}, release = {.cmd = MINI_BC_FREE_REPLY};
    call.transaction.size = 1;
    call.transaction.data[0] = worker->input;
    assert(ioctl(worker->fd, MINI_BINDER_GET_THREAD_ID, &worker->thread_id) == 0);
    assert(ioctl(worker->fd, MINI_BINDER_WRITE, &call) == 0);
    barrier(worker->phase);
    barrier(worker->phase);
    if (worker->input == 10)
        assert(ioctl(worker->fd, MINI_BINDER_READ, (void *)0) == -1 && errno == EFAULT);
    assert(ioctl(worker->fd, MINI_BINDER_READ, &reply) == 0 && reply.cmd == MINI_BR_REPLY);
    assert(reply.transaction.size == 1 && reply.transaction.reply_offset < 2 * MINI_BINDER_MAX_DATA);
    worker->offset = reply.transaction.reply_offset;
    worker->result = worker->mapping[worker->offset];
    assert(worker->result == worker->input + 1);
    barrier(worker->phase);
    barrier(worker->phase);
    assert(ioctl(worker->fd, MINI_BINDER_WRITE, &release) == 0);
    return NULL;
}

struct service_group {
    int fd;
    pthread_barrier_t phase;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    int second_done;
    uintptr_t addresses[2];
};
static void receive(int fd, struct mini_binder_command *event)
{
    memset(event, 0, sizeof(*event));
#if MINI_BINDER_PROTOCOL_VERSION >= 504 && MINI_BINDER_PROTOCOL_VERSION < 703
    event->cmd = MINI_READ_REQUEST;
#endif
    assert(exchange_command(fd, NULL, event) == 0);
    assert(event->cmd == MINI_BR_TRANSACTION);
}
static void enter_service(int fd)
{
#if MINI_BINDER_PROTOCOL_VERSION >= 703
    struct mini_binder_command enter = {.cmd = MINI_BC_ENTER_LOOPER};
    assert(exchange_command(fd, &enter, NULL) == 0);
#else
    (void)fd;
#endif
}
static void *service_worker(void *arg)
{
    struct service_group *group = arg;
    struct mini_binder_command event, again = {0}, reply = {.cmd = MINI_BC_REPLY};
    enter_service(group->fd);
    receive(group->fd, &event);
    assert(event.transaction.size == 1);
    unsigned char value = *(const unsigned char *)(uintptr_t)event.transaction.data_address;
    assert(value == 10 || value == 20);
    group->addresses[value == 20] = event.transaction.data_address;
#if MINI_BINDER_PROTOCOL_VERSION >= 504 && MINI_BINDER_PROTOCOL_VERSION < 703
    again.cmd = MINI_READ_REQUEST;
#endif
    assert(exchange_command(group->fd, NULL, &again) == -EBUSY);
    barrier(&group->phase);
    barrier(&group->phase);
    assert(pthread_mutex_lock(&group->lock) == 0);
    while (value == 10 && !group->second_done)
        assert(pthread_cond_wait(&group->changed, &group->lock) == 0);
    reply.transaction.size = 1;
    reply.transaction.data[0] = value + 1;
    assert(exchange_command(group->fd, &reply, NULL) == 0);
    /* A second BC_REPLY cannot select a different request. */
    assert(exchange_command(group->fd, &reply, NULL) == -ENOENT);
    if (value == 20) {
        group->second_done = 1;
        assert(pthread_cond_broadcast(&group->changed) == 0);
    }
    assert(pthread_mutex_unlock(&group->lock) == 0);
    return NULL;
}
static void server(int ready, int stop)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    long page = sysconf(_SC_PAGESIZE);
    unsigned char byte;
    pthread_t workers[2];
    struct service_group group = {.fd = fd, .lock = PTHREAD_MUTEX_INITIALIZER,
                                 .changed = PTHREAD_COND_INITIALIZER};
    struct mini_binder_command event, reply = {.cmd = MINI_BC_REPLY};
    assert(fd >= 0);
#ifdef MINI_BINDER_REGISTER_SERVER
    assert(ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
#endif
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    void *mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED && pthread_barrier_init(&group.phase, NULL, 3) == 0);
    for (int i = 0; i < 2; i++)
        assert(pthread_create(&workers[i], NULL, service_worker, &group) == 0);
    assert(write(ready, "R", 1) == 1); close(ready);
    barrier(&group.phase);
    assert(group.addresses[0] != group.addresses[1]);
    /* This thread did not receive either request: it cannot answer for a worker. */
    assert(exchange_command(fd, &reply, NULL) == -ENOENT);
    barrier(&group.phase);
    for (int i = 0; i < 2; i++) assert(pthread_join(workers[i], NULL) == 0);
    {
        enter_service(fd);
        receive(fd, &event);
        reply.transaction.size = 1;
        reply.transaction.data[0] = *(const unsigned char *)(uintptr_t)event.transaction.data_address + 1;
        assert(exchange_command(fd, &reply, NULL) == 0);
    }
    assert(read(stop, &byte, 1) == 1);
    close(stop); munmap(mapping, (size_t)page); close(fd); _exit(0);
}

int main(void)
{
    int ready[2], stop[2], fd, status;
    pid_t child;
    unsigned char byte, *mapping;
    long page = sysconf(_SC_PAGESIZE);
    pthread_t threads[2];
    pthread_barrier_t phase;
    struct worker workers[2];
    struct mini_binder_command reply = {0}, release = {.cmd = MINI_BC_FREE_REPLY};
    struct mini_binder_command third = {.cmd = MINI_BC_TRANSACTION};
    assert(pipe(ready) == 0 && pipe(stop) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(stop[1]); server(ready[1], stop[0]); }
    close(ready[1]); close(stop[0]);
    assert(read(ready[0], &byte, 1) == 1); close(ready[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED && pthread_barrier_init(&phase, NULL, 3) == 0);
    for (int i = 0; i < 2; i++) {
        workers[i] = (struct worker){.fd = fd, .mapping = mapping,
            .input = (unsigned char)(10 + i * 10), .phase = &phase};
        assert(pthread_create(&threads[i], NULL, client_thread, &workers[i]) == 0);
    }
    barrier(&phase);
    assert(ioctl(fd, MINI_BINDER_READ, &reply) == -1 && errno == ENOENT);
    assert(ioctl(fd, MINI_BINDER_WRITE, &release) == -1 && errno == ENOENT);
    third.transaction.size = 1; third.transaction.data[0] = 30;
    assert(ioctl(fd, MINI_BINDER_WRITE, &third) == -1 && errno == EBUSY);
    assert(ioctl(fd, MINI_BINDER_THREAD_EXIT) == 0);
    barrier(&phase);
    barrier(&phase);
    assert(workers[0].offset != workers[1].offset);
    assert(workers[0].thread_id != workers[1].thread_id);
    assert(mapping[workers[0].offset] == workers[0].result && mapping[workers[1].offset] == workers[1].result);
    for (int i = 0; i < 2; i++)
        printf("thread %u: %u -> %u, offset %u\n", workers[i].thread_id,
               (unsigned int)workers[i].input,
               (unsigned int)workers[i].result, workers[i].offset);
    barrier(&phase);
    for (int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    assert(ioctl(fd, MINI_BINDER_WRITE, &third) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &reply) == 0);
    assert(reply.transaction.size == 1 && mapping[reply.transaction.reply_offset] == 31);
    assert(ioctl(fd, MINI_BINDER_WRITE, &release) == 0);
    assert(write(stop[1], "S", 1) == 1); close(stop[1]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    pthread_barrier_destroy(&phase); munmap(mapping, (size_t)page); close(fd);
    puts("reverse replies, thread isolation, full slots and reuse passed");
    return 0;
}
