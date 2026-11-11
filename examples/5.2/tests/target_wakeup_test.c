// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <mini_binder.h>

struct client {
    int fd, notify;
    unsigned char *mapping, value;
    uint32_t binder_thread;
    atomic_long tid;
};

static void *call_server(void *arg)
{
    struct client *c = arg;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command reply = {0}, release = {.cmd = MINI_BC_FREE_REPLY};
    call.transaction.size = 1;
    call.transaction.data[0] = c->value;
    assert(ioctl(c->fd, MINI_BINDER_GET_THREAD_ID, &c->binder_thread) == 0);
    atomic_store(&c->tid, syscall(SYS_gettid));
    assert(ioctl(c->fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(c->fd, MINI_BINDER_READ, &reply) == 0);
    assert(reply.transaction.size == 1 && c->mapping[reply.transaction.reply_offset] == c->value + 1);
    assert(ioctl(c->fd, MINI_BINDER_WRITE, &release) == 0);
    assert(write(c->notify, &c->value, 1) == 1);
    return NULL;
}

static void wait_in_ioctl(atomic_long *tid)
{
    struct timespec delay = {.tv_nsec = 10000000};
    for (int i = 0; i < 500; i++) {
        long id = atomic_load(tid), nr = -1;
        char path[80], line[160];
        int sleeping = 0;
        FILE *f;
        if (!id) { nanosleep(&delay, NULL); continue; }
        snprintf(path, sizeof(path), "/proc/self/task/%ld/status", id);
        f = fopen(path, "r"); assert(f);
        while (fgets(line, sizeof(line), f))
            if (!strncmp(line, "State:", 6) && strchr(line, 'S')) sleeping = 1;
        fclose(f);
        snprintf(path, sizeof(path), "/proc/self/task/%ld/syscall", id);
        f = fopen(path, "r"); assert(f);
        if (fscanf(f, "%ld", &nr) != 1) nr = -1;
        fclose(f);
        if (sleeping && nr == SYS_ioctl) return;
        nanosleep(&delay, NULL);
    }
    assert(!"client did not sleep in ioctl");
}

static void trace_write(const char *instance, const char *name, const char *value)
{
    char path[256];
    FILE *f;
    assert(snprintf(path, sizeof(path), "%s/%s", instance, name) < (int)sizeof(path));
    f = fopen(path, "w"); assert(f);
    assert(fputs(value, f) >= 0 && fclose(f) == 0);
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE), tids[2];
    int server = open("/dev/mini_binder", O_RDWR), fd, notify[2], first = -1, second = -1;
    unsigned char *mapping, byte;
    void *server_mapping;
    char instance[128], filter[128], path[256], line[512], first_pid[40], second_pid[40];
    int first_wakes = 0, second_wakes = 0;
    FILE *trace;
    pthread_t threads[2];
    struct client clients[2];
    struct mini_binder_command received[2] = {0}, reply = {.cmd = MINI_BC_REPLY};
    assert(server >= 0 && ioctl(server, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(server, MINI_BINDER_REGISTER_MANAGER) == 0);
    server_mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, server, 0);
    assert(server_mapping != MAP_FAILED && pipe(notify) == 0);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0); assert(mapping != MAP_FAILED);
    for (int i = 0; i < 2; i++) {
        clients[i] = (struct client){.fd = fd, .notify = notify[1], .mapping = mapping,
                                      .value = (unsigned char)(10 + i * 10), .tid = ATOMIC_VAR_INIT(0)};
        assert(pthread_create(&threads[i], NULL, call_server, &clients[i]) == 0);
    }
    for (int i = 0; i < 2; i++) {
        assert(ioctl(server, MINI_BINDER_READ, &received[i]) == 0);
        byte = *(const unsigned char *)(uintptr_t)received[i].transaction.data_address;
        if (byte == 10) first = i;
        if (byte == 20) second = i;
    }
    assert(first >= 0 && second >= 0);
    for (int i = 0; i < 2; i++) { wait_in_ioctl(&clients[i].tid); tids[i] = atomic_load(&clients[i].tid); }

    assert(snprintf(instance, sizeof(instance), "/sys/kernel/tracing/instances/binderabc-%ld", (long)getpid()) < (int)sizeof(instance));
    assert(mkdir(instance, 0700) == 0);
    snprintf(filter, sizeof(filter), "pid == %ld || pid == %ld", tids[0], tids[1]);
    trace_write(instance, "events/sched/sched_waking/filter", filter);
    trace_write(instance, "events/sched/sched_waking/enable", "1");
    trace_write(instance, "trace", "");
    trace_write(instance, "tracing_on", "1");

    reply.transaction.id = received[first].transaction.id;
    reply.transaction.size = 1; reply.transaction.data[0] = 11;
    assert(ioctl(server, MINI_BINDER_WRITE, &reply) == 0);
    assert(read(notify[0], &byte, 1) == 1 && byte == 10);
    trace_write(instance, "tracing_on", "0");
    wait_in_ioctl(&clients[1].tid);
    snprintf(first_pid, sizeof(first_pid), "pid=%ld ", tids[0]);
    snprintf(second_pid, sizeof(second_pid), "pid=%ld ", tids[1]);
    snprintf(path, sizeof(path), "%s/trace", instance);
    trace = fopen(path, "r"); assert(trace);
    while (fgets(line, sizeof(line), trace)) {
        if (strstr(line, "sched_waking:") && strstr(line, first_pid)) first_wakes++;
        if (strstr(line, "sched_waking:") && strstr(line, second_pid)) second_wakes++;
    }
    fclose(trace);
    trace_write(instance, "events/sched/sched_waking/enable", "0");
    assert(rmdir(instance) == 0);
    assert(first_wakes >= 1 && second_wakes == 0);
    printf("reply id=%llu -> binder thread=%u: target wake events %d; other wake events %d\n",
           (unsigned long long)received[first].transaction.id, clients[0].binder_thread, first_wakes, second_wakes);

    reply.transaction.id = received[second].transaction.id;
    reply.transaction.data[0] = 21;
    assert(ioctl(server, MINI_BINDER_WRITE, &reply) == 0);
    assert(read(notify[0], &byte, 1) == 1 && byte == 20);
    printf("reply id=%llu -> binder thread=%u\n",
           (unsigned long long)received[second].transaction.id, clients[1].binder_thread);
    for (int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    close(notify[0]); close(notify[1]);
    munmap(mapping, (size_t)page); close(fd);
    munmap(server_mapping, (size_t)page); close(server);
    puts("both threads received their own replies");
    return 0;
}
