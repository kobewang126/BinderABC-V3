// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mini_binder.h>

struct worker {
    int fd;
    uint32_t id;
    uint64_t request_id;
    unsigned char input;
    unsigned char result;
};
static void *serve(void *data)
{
    struct worker *worker = data;
    struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
    const unsigned char *payload;
    assert(ioctl(worker->fd, MINI_BINDER_GET_THREAD_ID, &worker->id) == 0);
    assert(ioctl(worker->fd, MINI_BINDER_READ, &req) == 0);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    assert(req.transaction.size == 1);
    worker->request_id = req.transaction.id;
    worker->input = payload[0];
    worker->result = worker->input + 1;
    reply.transaction.id = req.transaction.id;
    reply.transaction.size = 1;
    reply.transaction.data[0] = worker->result;
    assert(ioctl(worker->fd, MINI_BINDER_WRITE, &reply) == 0);
    return NULL;
}
static void client(int start, unsigned char number)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    long page = sysconf(_SC_PAGESIZE);
    unsigned char *mapping, token;
    struct mini_binder_command req = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
    struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
    assert(fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED && read(start, &token, 1) == 1);
    req.transaction.size = 1; req.transaction.data[0] = number;
    assert(ioctl(fd, MINI_BINDER_WRITE, &req) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &reply) == 0 && mapping[0] == number + 1);
    assert(ioctl(fd, MINI_BINDER_WRITE, &release) == 0);
    munmap(mapping, (size_t)page); close(fd); _exit(0);
}
static void wait_for_queued_client(pid_t pid)
{
    struct timespec delay = {0, 10000000};
    for (int i = 0; i < 500; i++) {
        char path[80], line[160];
        long nr = -1;
        int sleeping = 0;
        FILE *f;
        snprintf(path, sizeof(path), "/proc/%ld/status", (long)pid);
        f = fopen(path, "r");
        assert(f);
        while (fgets(line, sizeof(line), f))
            if (!strncmp(line, "State:", 6) && strchr(line, 'S')) sleeping = 1;
        fclose(f);
        snprintf(path, sizeof(path), "/proc/%ld/syscall", (long)pid);
        f = fopen(path, "r");
        assert(f);
        if (fscanf(f, "%ld", &nr) != 1) nr = -1;
        fclose(f);
        if (sleeping && nr == SYS_ioctl) return;
        nanosleep(&delay, NULL);
    }
    assert(!"client did not wait in ioctl");
}
int main(void)
{
    int fd = open("/dev/mini_binder", O_RDWR), start[2], status;
    long page = sysconf(_SC_PAGESIZE);
    void *mapping;
    pid_t clients[2];
    pthread_t threads[2];
    struct worker workers[2];
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED && pipe(start) == 0);
    for (int i = 0; i < 2; i++) {
        clients[i] = fork(); assert(clients[i] >= 0);
        if (!clients[i]) { close(start[1]); client(start[0], (unsigned char)(10 + i)); }
    }
    close(start[0]);
    assert(write(start[1], "RR", 2) == 2); close(start[1]);
    for (int i = 0; i < 2; i++) wait_for_queued_client(clients[i]);
    puts("two client requests queued before workers started");
    for (int i = 0; i < 2; i++) { workers[i].fd = fd; assert(pthread_create(&threads[i], NULL, serve, &workers[i]) == 0); }
    for (int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    assert(workers[0].id != workers[1].id);
    for (int i = 0; i < 2; i++) assert(waitpid(clients[i], &status, 0) == clients[i] && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    for (int i = 0; i < 2; i++)
        printf("thread %u handled request %llu: %u -> %u\n", workers[i].id,
               (unsigned long long)workers[i].request_id,
               (unsigned int)workers[i].input, (unsigned int)workers[i].result);
    munmap(mapping, (size_t)page); close(fd);
    puts("two server threads receive separate queued requests");
    return 0;
}
