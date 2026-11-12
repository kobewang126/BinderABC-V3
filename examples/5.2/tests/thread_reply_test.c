// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>

static int demo;

struct worker {
    int fd;
    unsigned char *mapping;
    unsigned char input;
    uint32_t thread_id;
    uint64_t transaction_id;
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
    if (!demo && worker->input == 10)
        assert(ioctl(worker->fd, MINI_BINDER_READ, (void *)0) == -1 && errno == EFAULT);
    assert(ioctl(worker->fd, MINI_BINDER_READ, &reply) == 0 && reply.cmd == MINI_BR_REPLY);
    assert(reply.transaction.size == 1 && reply.transaction.reply_offset < 2 * MINI_BINDER_MAX_DATA);
    worker->offset = reply.transaction.reply_offset;
    worker->transaction_id = reply.transaction.id;
    worker->result = worker->mapping[worker->offset];
    assert(worker->result == worker->input + 1);
    barrier(worker->phase);
    barrier(worker->phase);
    assert(ioctl(worker->fd, MINI_BINDER_WRITE, &release) == 0);
    return NULL;
}

static void server(int ready, int stop)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    long page = sysconf(_SC_PAGESIZE);
    void *mapping;
    struct mini_binder_command received[2] = {0};
    struct mini_binder_command reply = {.cmd = MINI_BC_REPLY};
    unsigned char values[2], byte;
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED && write(ready, "R", 1) == 1);
    close(ready);
    for (int i = 0; i < 2; i++) {
        assert(ioctl(fd, MINI_BINDER_READ, &received[i]) == 0);
        assert(received[i].transaction.size == 1);
        values[i] = *(const unsigned char *)(uintptr_t)received[i].transaction.data_address;
    }
    for (int i = 1; i >= 0; i--) {
        reply.transaction.id = received[i].transaction.id;
        reply.transaction.size = 1;
        reply.transaction.data[0] = values[i] + 1;
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    }
    if (!demo) {
        assert(ioctl(fd, MINI_BINDER_READ, &received[0]) == 0);
        assert(received[0].transaction.size == 1);
        reply.transaction.id = received[0].transaction.id;
        reply.transaction.data[0] = *(const unsigned char *)(uintptr_t)received[0].transaction.data_address + 1;
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    }
    assert(read(stop, &byte, 1) == 1);
    close(stop); munmap(mapping, (size_t)page); close(fd); _exit(0);
}

int main(int argc, char **argv)
{
    demo = argc == 2 && strcmp(argv[1], "--demo") == 0;
    assert(argc == 1 || demo);
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
    if (!demo) {
        assert(ioctl(fd, MINI_BINDER_READ, &reply) == -1 && errno == ENOENT);
        assert(ioctl(fd, MINI_BINDER_WRITE, &release) == -1 && errno == ENOENT);
        third.transaction.size = 1; third.transaction.data[0] = 30;
        assert(ioctl(fd, MINI_BINDER_WRITE, &third) == -1 && errno == EBUSY);
    }
    barrier(&phase);
    barrier(&phase);
    assert(workers[0].offset != workers[1].offset);
    assert(workers[0].thread_id != workers[1].thread_id &&
           workers[0].transaction_id && workers[1].transaction_id &&
           workers[0].transaction_id != workers[1].transaction_id);
    assert(mapping[workers[0].offset] == workers[0].result && mapping[workers[1].offset] == workers[1].result);
    for (int i = 0; i < 2; i++)
        printf("thread %u: transaction %llu, %u -> %u, offset %u\n", workers[i].thread_id,
               (unsigned long long)workers[i].transaction_id, (unsigned int)workers[i].input,
               (unsigned int)workers[i].result, workers[i].offset);
    barrier(&phase);
    for (int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    if (!demo) {
        assert(ioctl(fd, MINI_BINDER_WRITE, &third) == 0);
        assert(ioctl(fd, MINI_BINDER_READ, &reply) == 0);
        assert(reply.transaction.size == 1 && mapping[reply.transaction.reply_offset] == 31);
        assert(ioctl(fd, MINI_BINDER_WRITE, &release) == 0);
    }
    assert(write(stop[1], "S", 1) == 1); close(stop[1]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    pthread_barrier_destroy(&phase); munmap(mapping, (size_t)page); close(fd);
    puts(demo ? "reverse replies and thread isolation passed" :
                "reverse replies, thread isolation, full slots and reuse passed");
    return 0;
}
