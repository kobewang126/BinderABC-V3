// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
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
struct report { int received_fd; uint32_t handle; char data[4]; };
static void receiver(int ready, int control, int report)
{
    int binder = open("/dev/mini_binder", O_RDWR), received_fd, flags;
    unsigned char byte;
    void *mapping;
    const unsigned char *payload;
    struct report result = {0};
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {.cmd = MINI_READ_REQUEST}, reply = {.cmd = MINI_BC_REPLY};
    assert(binder >= 0 && ioctl(binder, MINI_BINDER_CREATE_NODE, &node) == 0);
    mapping = map(binder);
    assert(write(ready, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id)); close(ready);
    assert(read(control, &byte, 1) == 1);
    assert(ioctl(binder, MINI_BINDER_READ, &req) == 0 && req.cmd == MINI_BR_TRANSACTION);
    assert(req.transaction.size == 28 && req.transaction.object_count == 2);
    assert(req.transaction.object_offsets[0] == 4 && req.transaction.object_offsets[1] == 16);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    assert(get32(payload) == 123 && get32(payload + 4) == MINI_BINDER_OBJECT_HANDLE);
    result.handle = get32(payload + 8); assert(result.handle == 1);
    assert(get32(payload + 16) == MINI_BINDER_OBJECT_FD && get32(payload + 24) == 0);
    received_fd = (int)get32(payload + 20); result.received_fd = received_fd;
    flags = fcntl(received_fd, F_GETFD);
    assert(flags >= 0 && (flags & FD_CLOEXEC));
    assert(read(received_fd, result.data, sizeof(result.data)) == sizeof(result.data));
    assert(memcmp(result.data, "data", 4) == 0);
    assert(close(received_fd) == 0);
    assert(write(report, &result, sizeof(result)) == sizeof(result)); close(report);
    reply.transaction.id = req.transaction.id; reply.transaction.size = 4; put32(reply.transaction.data, 42);
    assert(ioctl(binder, MINI_BINDER_WRITE, &reply) == 0);
    assert(read(control, &byte, 1) == 1); close(control);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(binder); _exit(0);
}
struct sender { int binder; unsigned char *mapping; struct mini_binder_command call; atomic_long tid; };
static void *send_request(void *data)
{
    struct sender *sender = data;
    struct mini_binder_command reply = {.cmd = MINI_READ_REPLY}, release = {.cmd = MINI_BC_FREE_REPLY};
    atomic_store(&sender->tid, syscall(SYS_gettid));
    assert(ioctl(sender->binder, MINI_BINDER_WRITE, &sender->call) == 0);
    assert(ioctl(sender->binder, MINI_BINDER_READ, &reply) == 0 && reply.transaction.size == 4);
    assert(get32(sender->mapping + reply.transaction.reply_offset) == 42);
    assert(ioctl(sender->binder, MINI_BINDER_WRITE, &release) == 0);
    return NULL;
}
static void wait_ioctl_sleep(atomic_long *tid_ptr)
{
    struct timespec delay = {.tv_nsec = 10000000};
    for (int i = 0; i < 500; i++) {
        long tid = atomic_load(tid_ptr), nr = -1;
        char path[80], line[160]; int sleeping = 0;
        FILE *f;
        if (!tid) { nanosleep(&delay, NULL); continue; }
        snprintf(path, sizeof(path), "/proc/self/task/%ld/status", tid);
        f = fopen(path, "r");
        if (f) {
            while (fgets(line, sizeof(line), f))
                if (!strncmp(line, "State:", 6) && strchr(line, 'S')) sleeping = 1;
            fclose(f);
        }
        snprintf(path, sizeof(path), "/proc/self/task/%ld/syscall", tid);
        f = fopen(path, "r");
        if (f) { if (fscanf(f, "%ld", &nr) != 1) nr = -1; fclose(f); }
        if (sleeping && nr == SYS_ioctl) return;
        nanosleep(&delay, NULL);
    }
    assert(!"sender did not sleep in ioctl");
}
int main(void)
{
    int ready[2], control[2], report_pipe[2], binder, data_fd, status;
    char path[] = "/tmp/binderabc-fd-XXXXXX";
    uint64_t target;
    pid_t child;
    pthread_t thread;
    struct report report;
    struct mini_binder_node_create node = {.cookie = 99};
    struct sender sender = {.tid = ATOMIC_VAR_INIT(0)};
    struct mini_binder_command invalid;
    assert(pipe(ready) == 0 && pipe(control) == 0 && pipe(report_pipe) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(control[1]); close(report_pipe[0]); receiver(ready[1], control[0], report_pipe[1]); }
    close(ready[1]); close(control[0]); close(report_pipe[1]);
    assert(read(ready[0], &target, sizeof(target)) == sizeof(target)); close(ready[0]);
    data_fd = mkstemp(path); assert(data_fd >= 0 && unlink(path) == 0);
    assert(write(data_fd, "data", 4) == 4 && lseek(data_fd, 0, SEEK_SET) == 0);
    binder = open("/dev/mini_binder", O_RDWR); assert(binder >= 0);
    assert(ioctl(binder, MINI_BINDER_CREATE_NODE, &node) == 0);
    sender.binder = binder; sender.mapping = map(binder);
    sender.call.cmd = MINI_BC_TRANSACTION; sender.call.transaction.target_node = target;
    sender.call.transaction.size = 28; sender.call.transaction.object_count = 2;
    sender.call.transaction.object_offsets[0] = 4; sender.call.transaction.object_offsets[1] = 16;
    put32(sender.call.transaction.data, 123);
    put32(sender.call.transaction.data + 4, MINI_BINDER_OBJECT_LOCAL);
    put32(sender.call.transaction.data + 8, 99);
    put32(sender.call.transaction.data + 16, MINI_BINDER_OBJECT_FD);
    invalid = sender.call; put32(invalid.transaction.data + 20, INT_MAX);
    assert(ioctl(binder, MINI_BINDER_WRITE, &invalid) == -1 && errno == EBADF);
    put32(sender.call.transaction.data + 20, (uint32_t)data_fd);
    assert(pthread_create(&thread, NULL, send_request, &sender) == 0);
    wait_ioctl_sleep(&sender.tid);
    assert(close(data_fd) == 0);
    assert(write(control[1], "G", 1) == 1);
    assert(read(report_pipe[0], &report, sizeof(report)) == sizeof(report)); close(report_pipe[0]);
    assert(pthread_join(thread, NULL) == 0);
    assert(write(control[1], "S", 1) == 1); close(control[1]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("sender fd %d -> receiver fd %d; binder handle %u\n", data_fd, report.received_fd, report.handle);
    printf("receiver read after sender closed: %.*s\n", 4, report.data);
    munmap(sender.mapping, (size_t)sysconf(_SC_PAGESIZE)); close(binder);
    return 0;
}
