// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include "../userspace/command.h"

static void phase(pthread_barrier_t *p)
{ int ret = pthread_barrier_wait(p); assert(ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD); }
static void on_signal(int sig) { (void)sig; }
static void *map(int fd)
{
    void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(p != MAP_FAILED); return p;
}
struct service { int fd, cancel, success; pthread_barrier_t gate; };
static void *serve_once(void *data)
{
    struct service *s = data;
    struct mini_binder_command event = {0}, reply = {.cmd = MINI_BC_REPLY};
#if MINI_BINDER_PROTOCOL_VERSION >= 703
    struct mini_binder_command enter = {.cmd = MINI_BC_ENTER_LOOPER};
    assert(exchange_command(s->fd, &enter, NULL) == 0);
#elif MINI_BINDER_PROTOCOL_VERSION >= 504
    event.cmd = MINI_READ_REQUEST;
#endif
    assert(exchange_command(s->fd, NULL, &event) == 0 && event.cmd == MINI_BR_TRANSACTION);
    phase(&s->gate); phase(&s->gate);
    if (s->success) {
        reply.transaction.size = 1; reply.transaction.data[0] = 42;
        assert(exchange_command(s->fd, &reply, NULL) == 0);
    }
    /* Explicit notification while the fd and other service threads stay alive. */
    assert(ioctl(s->fd, MINI_BINDER_THREAD_EXIT) == 0);
    assert(ioctl(s->fd, MINI_BINDER_THREAD_EXIT) == -1 && errno == ENOENT);
    return NULL;
}
static void client(int cancel, int success)
{
    struct sigaction action = {.sa_handler = on_signal};
    sigemptyset(&action.sa_mask); assert(sigaction(SIGUSR1, &action, NULL) == 0);
    int fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    unsigned char *mapping = map(fd);
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
    call.transaction.size = 1; call.transaction.data[0] = 40;
#if MINI_BINDER_PROTOCOL_VERSION >= 504 && MINI_BINDER_PROTOCOL_VERSION < 703
    reply.cmd = MINI_READ_REPLY;
#endif
    int ret = exchange_command(fd, &call, &reply);
    if (cancel) assert(ret == -EINTR);
    else if (!success) assert(ret == -EPIPE);
    else {
        assert(ret == 0 && reply.cmd == MINI_BR_REPLY && reply.transaction.size == 1);
        assert(mapping[reply.transaction.reply_offset] == 42);
        struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
        assert(exchange_command(fd, &release, NULL) == 0);
    }
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd); _exit(0);
}
static void wait_ok(pid_t pid)
{ int status; assert(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0); }
int main(int argc, char **argv)
{
    int demo = argc == 2 && strcmp(argv[1], "--demo") == 0;
    assert(argc == 1 || demo);
    int fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
#ifdef MINI_BINDER_REGISTER_SERVER
    assert(ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
#endif
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    void *mapping = map(fd);
    for (int i = 0; i < (demo ? 2 : 12); i++) {
        struct service service = {.fd = fd, .cancel = !demo && i == 1,
                                  .success = i == (demo ? 1 : 11)};
        pthread_t thread;
        assert(pthread_barrier_init(&service.gate, NULL, 2) == 0);
        pid_t pid = fork(); assert(pid >= 0);
        if (!pid) client(service.cancel, service.success);
        assert(pthread_create(&thread, NULL, serve_once, &service) == 0);
        phase(&service.gate);
        if (service.cancel) { assert(kill(pid, SIGUSR1) == 0); wait_ok(pid); }
        phase(&service.gate);
        assert(pthread_join(thread, NULL) == 0);
        if (!service.cancel) wait_ok(pid);
        assert(pthread_barrier_destroy(&service.gate) == 0);
    }
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("service thread exit: waiting caller received EPIPE");
    if (!demo) puts("cancelled transaction and repeated thread records reclaimed");
    puts("next service thread: reply = 42; slots reused");
    return 0;
}
