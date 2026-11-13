// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mini_binder.h>
#include <parcel.h>
#include <test_io.h>

extern long syscall(long number, ...);
#define ROUNDS 2
struct service {
    int fd;
    pid_t tid;
    uint32_t id, input[ROUNDS], result[ROUNDS];
    uintptr_t address[ROUNDS];
    pthread_barrier_t *ready, *received;
};
static void phase(pthread_barrier_t *barrier)
{
    int ret = pthread_barrier_wait(barrier);
    assert(ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD);
}
static void *map_buffer(int fd)
{
    void *mapping = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    return mapping;
}
static uint32_t decode(const void *data, size_t size)
{
    struct parcel p;
    uint32_t value;
    parcel_init(&p, (void *)data, size); p.size = size;
    assert(parcel_read_u32(&p, &value) == 0 && p.position == p.size);
    return value;
}
static void encode(struct mini_binder_transaction_data *tx, uint32_t value)
{
    struct parcel p;
    parcel_init(&p, tx->data, sizeof(tx->data));
    assert(parcel_write_u32(&p, value) == 0);
    tx->size = p.size;
}
static void *serve(void *arg)
{
    struct service *worker = arg;
    worker->tid = (pid_t)syscall(SYS_gettid);
    assert(ioctl(worker->fd, MINI_BINDER_GET_THREAD_ID, &worker->id) == 0);
    phase(worker->ready);
    for (int round = 0; round < ROUNDS; round++) {
        struct mini_binder_command event = {0}, reply = {.cmd = MINI_BC_REPLY};
        assert(read_request(worker->fd, &event) == 0 && event.cmd == MINI_BR_TRANSACTION);
        worker->address[round] = (uintptr_t)event.transaction.data_address;
        worker->input[round] = decode((const void *)worker->address[round], event.transaction.size);
        /* Keep both requests in flight, so both service threads take one. */
        phase(worker->received);
        worker->result[round] = worker->input[round] + 1;
        reply.transaction.id = event.transaction.id;
        encode(&reply.transaction, worker->result[round]);
        assert(ioctl(worker->fd, MINI_BINDER_WRITE, &reply) == 0);
    }
    return NULL;
}
/* Observe both threads sleeping inside ioctl before allowing clients to send. */
static void wait_for_request(pid_t tid)
{
    char path[96], stat_path[96], line[512];
    struct timespec delay = {.tv_nsec = 10000000};
    snprintf(path, sizeof(path), "/proc/self/task/%ld/syscall", (long)tid);
    snprintf(stat_path, sizeof(stat_path), "/proc/self/task/%ld/stat", (long)tid);
    for (int attempt = 0; attempt < 500; attempt++) {
        long number = -1;
        char state = 0;
        FILE *f = fopen(path, "r");
        if (f) { if (fscanf(f, "%ld", &number) != 1) number = -1; fclose(f); }
        f = fopen(stat_path, "r");
        if (f) {
            if (fgets(line, sizeof(line), f)) {
                /* Thread names can contain spaces; the last ')' precedes state. */
                char *end = NULL;
                for (char *p = line; *p; p++) if (*p == ')') end = p;
                if (end && end[1] == ' ') state = end[2];
            }
            fclose(f);
        }
        if (number == SYS_ioctl && state == 'S') return;
        nanosleep(&delay, NULL);
    }
    assert(!"service thread did not sleep in ioctl; check /proc access");
}
struct caller { int fd; const unsigned char *mapping; uint32_t base; };
static void *call(void *arg)
{
    struct caller *caller = arg;
    caller->fd = open("/dev/mini_binder", O_RDWR); assert(caller->fd >= 0);
    caller->mapping = map_buffer(caller->fd);
    for (int round = 0; round < ROUNDS; round++) {
        struct mini_binder_command request = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
        struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
        uint32_t input = caller->base + (uint32_t)round;
        encode(&request.transaction, input);
        assert(ioctl(caller->fd, MINI_BINDER_WRITE, &request) == 0);
        assert(read_reply(caller->fd, &reply) == 0 && reply.cmd == MINI_BR_REPLY);
        assert(decode(caller->mapping + 2 * MINI_BINDER_MAX_DATA, reply.transaction.size) == input + 1);
        assert(ioctl(caller->fd, MINI_BINDER_WRITE, &release) == 0);
    }
    munmap((void *)caller->mapping, (size_t)sysconf(_SC_PAGESIZE)); close(caller->fd);
    return NULL;
}
static void clients(int start)
{
    char token;
    assert(read(start, &token, 1) == 1); close(start);
    pthread_t threads[2];
    struct caller callers[2] = {{.base = 10}, {.base = 20}};
    for (int i = 0; i < 2; i++) assert(pthread_create(&threads[i], NULL, call, &callers[i]) == 0);
    for (int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    _exit(0);
}
int main(void)
{
    int start[2], status;
    assert(pipe(start) == 0);
    pid_t child = fork(); assert(child >= 0);
    if (!child) { close(start[1]); clients(start[0]); }
    close(start[0]);
    /* Service threads share one open file; callers use separate open files. */
    int fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    void *mapping = map_buffer(fd);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    pthread_t threads[2];
    pthread_barrier_t ready, received;
    assert(pthread_barrier_init(&ready, NULL, 3) == 0);
    assert(pthread_barrier_init(&received, NULL, 2) == 0);
    struct service workers[2] = {{.fd = fd, .ready = &ready, .received = &received},
                                 {.fd = fd, .ready = &ready, .received = &received}};
    for (int i = 0; i < 2; i++) assert(pthread_create(&threads[i], NULL, serve, &workers[i]) == 0);
    phase(&ready);
    for (int i = 0; i < 2; i++) wait_for_request(workers[i].tid);
    puts("one service fd and mapping; two service threads sleeping in READ");
    assert(write(start[1], "R", 1) == 1); close(start[1]);
    for (int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    assert(workers[0].id != workers[1].id);
    for (int round = 0; round < ROUNDS; round++) {
        assert(workers[0].address[round] != workers[1].address[round]);
        for (int i = 0; i < 2; i++)
            printf("round %d, worker %d: %u -> %u\n", round + 1, i + 1,
                   workers[i].input[round], workers[i].result[round]);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    pthread_barrier_destroy(&received); pthread_barrier_destroy(&ready);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("two service threads completed four calls; callers received their own replies");
    return 0;
}
