// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <time.h>
#include "../userspace/lab.h"

static atomic_int reader_tid;
static int shared_fd, reader_error;
static void on_signal(int sig) { (void)sig; }
/* Observe an actual blocked ioctl instead of relying on a scheduling delay. */
static void await_ioctl(pid_t pid, pid_t tid)
{
    char path[128], state[512];
    struct timespec tick = {.tv_nsec = 1000000};
    snprintf(path, sizeof(path), "/proc/%d/task/%d/syscall", pid, tid);
    for (int i = 0; i < 5000; i++) {
        FILE *f = fopen(path, "r"); long number = -1;
        if (f) { if (fgets(state, sizeof(state), f)) sscanf(state, "%ld", &number); fclose(f); }
        if (number == SYS_ioctl) return;
        nanosleep(&tick, NULL);
    }
    assert(!"thread did not block in ioctl");
}
static void *ordinary_reader(void *unused)
{
    struct mini_binder_command event = {0};
    (void)unused;
    atomic_store(&reader_tid, (int)syscall(SYS_gettid));
    assert(ioctl(shared_fd, MINI_BINDER_READ, &event) == -1);
    reader_error = errno;
    assert(ioctl(shared_fd, MINI_BINDER_THREAD_EXIT) == 0);
    return NULL;
}
static void peer(int ready, int go, int finish)
{
    int fd = open("/dev/mini_binder", O_RDWR); char byte;
    unsigned char *mapping;
    struct mini_binder_command event, reply = {.cmd = MINI_BC_REPLY};
    assert(fd >= 0); mapping = map_buffer(fd);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0); enter_loop(fd);
    assert(write(ready, "R", 1) == 1); close(ready);
    read_request(fd, &event);
    const unsigned char *data = (const unsigned char *)(uintptr_t)event.transaction.data_address;
    assert(get32(data) == MINI_BINDER_OBJECT_HANDLE && get32(data + 4) == 1);

    object(&reply.transaction, 0, MINI_BINDER_OBJECT_LOCAL, 88);
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    read_request(fd, &event); assert(event.transaction.object_cookie == 88);
    memset(&reply, 0, sizeof(reply)); reply.cmd = MINI_BC_REPLY;
     reply.transaction.size = 4; put32(reply.transaction.data, 11);
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(read(go, &byte, 1) == 1); close(go);
    transact(fd, 1, 3, &event);
    assert(get32(mapping + event.transaction.reply_offset) == 22); free_reply(fd);
    assert(read(finish, &byte, 1) == 1); close(finish);
    unmap_close(fd, mapping); _exit(0);
}
int main(void)
{
    int ready[2], go[2], finish[2]; pid_t child; char byte;
    pthread_t reader; unsigned char *mapping;
    struct sigaction action = {.sa_handler = on_signal};
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event, reply = {.cmd = MINI_BC_REPLY};
    assert(sigaction(SIGUSR1, &action, NULL) == 0);
    assert(pipe(ready) == 0 && pipe(go) == 0 && pipe(finish) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(ready[0]); close(go[1]); close(finish[1]); peer(ready[1], go[0], finish[0]); }
    close(ready[1]); close(go[0]); close(finish[0]);
    assert(read(ready[0], &byte, 1) == 1); close(ready[0]);
    shared_fd = open("/dev/mini_binder", O_RDWR); assert(shared_fd >= 0); mapping = map_buffer(shared_fd);
    object(&call.transaction, 0, MINI_BINDER_OBJECT_LOCAL, 77);
    assert(ioctl(shared_fd, MINI_BINDER_WRITE, &call) == 0); read_reply(shared_fd, &event);
    assert(get32(mapping + event.transaction.reply_offset + 4) == 1); free_reply(shared_fd);
    puts("ordinary caller: unified READ returned BR_REPLY without entering looper");
    memset(&call, 0, sizeof(call)); call.cmd = MINI_BC_TRANSACTION; call.transaction.target_handle = 1; call.transaction.code = 2;
    assert(ioctl(shared_fd, MINI_BINDER_WRITE, &call) == 0);
    assert(write(go[1], "G", 1) == 1); close(go[1]); await_ioctl(child, child);
    assert(pthread_create(&reader, NULL, ordinary_reader, NULL) == 0);
    while (!atomic_load(&reader_tid)) sched_yield();
    await_ioctl(getpid(), atomic_load(&reader_tid));
    assert(pthread_kill(reader, SIGUSR1) == 0 && pthread_join(reader, NULL) == 0 && reader_error == EINTR);
    puts("ordinary reader: did not steal queued request or another thread's reply");
    enter_loop(shared_fd);
    call.cmd = MINI_BC_ENTER_LOOPER;
    assert(ioctl(shared_fd, MINI_BINDER_WRITE, &call) == -1 && errno == EALREADY);
    memset(&event, 0, sizeof(event)); event.cmd = 999;
    assert(ioctl(shared_fd, MINI_BINDER_READ, &event) == 0 && event.cmd == MINI_BR_REPLY);
    assert(get32(mapping + event.transaction.reply_offset) == 11); free_reply(shared_fd);
    puts("looper READ 1: BR_REPLY, result=11 (own reply has priority)");
    read_request(shared_fd, &event); assert(event.transaction.object_cookie == 77);
    puts("looper READ 2: BR_TRANSACTION, cookie=77");
     reply.transaction.size = 4; put32(reply.transaction.data, 22);
    assert(ioctl(shared_fd, MINI_BINDER_WRITE, &reply) == 0);
    call.cmd = MINI_BC_EXIT_LOOPER;
    assert(ioctl(shared_fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(shared_fd, MINI_BINDER_WRITE, &call) == -1 && errno == EINVAL);
    atomic_store(&reader_tid, 0);
    assert(pthread_create(&reader, NULL, ordinary_reader, NULL) == 0);
    while (!atomic_load(&reader_tid)) sched_yield();
    await_ioctl(getpid(), atomic_load(&reader_tid));
    assert(munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)) == 0);
    assert(pthread_join(reader, NULL) == 0 && reader_error == ENXIO);
    assert(write(finish[1], "F", 1) == 1); close(finish[1]); lab_wait_ok(child); close(shared_fd);
    puts("ENTER/EXIT checks, interrupted READ and unmap wakeup passed");
    return 0;
}
