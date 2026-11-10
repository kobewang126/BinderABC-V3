// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mini_binder.h>

enum mode { INVALID, CANCEL, DEATH, FD_FULL, BAD_ADDRESS };
static void put32(unsigned char *p, uint32_t n)
{ for (int i = 0; i < 4; i++) p[i] = (unsigned char)(n >> (8 * i)); }
static void *map(int fd)
{
    void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(p != MAP_FAILED);
    return p;
}
static int first_free(void)
{
    for (int fd = 0; fd < 512; fd++)
        if (fcntl(fd, F_GETFD) == -1 && errno == EBADF) return fd;
    return -1;
}
static void receiver(enum mode mode, int notify, int control)
{
    int binder = open("/dev/mini_binder", O_RDWR), saved[128], count = 0, expected = -1;
    long page = sysconf(_SC_PAGESIZE);
    char byte;
    void *mapping, *guard = MAP_FAILED;
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {.cmd = MINI_READ_REQUEST};
    assert(binder >= 0 && ioctl(binder, MINI_BINDER_CREATE_NODE, &node) == 0);
    mapping = map(binder);
    if (mode == FD_FULL) {
        struct rlimit limit;
        assert(getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_max >= 64);
        limit.rlim_cur = 64; assert(setrlimit(RLIMIT_NOFILE, &limit) == 0);
        while (count < (int)(sizeof(saved) / sizeof(saved[0]))) {
            int fd = open("/dev/null", O_RDONLY);
            if (fd < 0) { assert(errno == EMFILE); break; }
            saved[count++] = fd;
        }
        assert(count > 0 && count < (int)(sizeof(saved) / sizeof(saved[0])));
        expected = saved[--count]; assert(close(expected) == 0);
    } else if (mode == BAD_ADDRESS) {
        guard = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(guard != MAP_FAILED && mprotect((char *)guard + page, (size_t)page, PROT_NONE) == 0);
        expected = first_free(); assert(expected >= 0);
    }
    assert(write(notify, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id));
    if (mode == FD_FULL || mode == BAD_ADDRESS) {
        if (mode == FD_FULL) {
            assert(ioctl(binder, MINI_BINDER_READ, &req) == -1 && errno == EMFILE);
        } else {
            struct mini_binder_command *bad = (void *)((char *)guard + page - sizeof(uint32_t));
            bad->cmd = MINI_READ_REQUEST;
            assert(ioctl(binder, MINI_BINDER_READ, bad) == -1 && errno == EFAULT);
        }
        assert(fcntl(expected, F_GETFD) == -1 && errno == EBADF);
        {
            int probe = open("/dev/null", O_RDONLY);
            assert(probe == expected); close(probe);
        }
        assert(write(notify, "P", 1) == 1);
    }
    assert(read(control, &byte, 1) == 1);
    close(control); close(notify);
    while (count) close(saved[--count]);
    if (guard != MAP_FAILED) munmap(guard, (size_t)page * 2);
    munmap(mapping, (size_t)page); close(binder); _exit(0);
}
struct receiver { pid_t pid; uint64_t node; int notify; int control; };
static struct receiver start_receiver(enum mode mode)
{
    int notify[2], control[2];
    struct receiver r;
    assert(pipe(notify) == 0 && pipe(control) == 0);
    r.pid = fork(); assert(r.pid >= 0);
    if (!r.pid) { close(notify[0]); close(control[1]); receiver(mode, notify[1], control[0]); }
    close(notify[1]); close(control[0]);
    r.notify = notify[0]; r.control = control[1];
    assert(read(r.notify, &r.node, sizeof(r.node)) == sizeof(r.node));
    return r;
}
static void stop_receiver(struct receiver *r)
{
    int status;
    assert(write(r->control, "S", 1) == 1); close(r->control); close(r->notify);
    assert(waitpid(r->pid, &status, 0) == r->pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
static void setup_call(struct mini_binder_command *call, uint64_t node, int first, int second)
{
    memset(call, 0, sizeof(*call)); call->cmd = MINI_BC_TRANSACTION;
    call->transaction.target_node = node;
    call->transaction.object_count = second >= 0 ? 2 : 1;
    call->transaction.size = call->transaction.object_count * 12;
    call->transaction.object_offsets[1] = 12;
    put32(call->transaction.data, MINI_BINDER_OBJECT_FD);
    put32(call->transaction.data + 4, (uint32_t)first);
    if (second >= 0) {
        put32(call->transaction.data + 12, MINI_BINDER_OBJECT_FD);
        put32(call->transaction.data + 16, (uint32_t)second);
    }
}
static void assert_eof(int fd)
{
    char byte;
    assert(read(fd, &byte, 1) == 0);
    close(fd);
}
static void failure(enum mode mode)
{
    struct receiver r = start_receiver(mode);
    struct mini_binder_command call;
    int binder = open("/dev/mini_binder", O_RDWR), p1[2], p2[2], ret;
    char ack;
    void *mapping;
    assert(binder >= 0 && pipe(p1) == 0 && pipe(p2) == 0);
    mapping = map(binder);
    setup_call(&call, r.node, p1[1], mode == INVALID ? INT_MAX : p2[1]);
    ret = ioctl(binder, MINI_BINDER_WRITE, &call);
    assert(ret == -1 && errno == (mode == INVALID ? EBADF : EPIPE));
    close(p1[1]); close(p2[1]);
    assert_eof(p1[0]); assert_eof(p2[0]);
    if (mode != INVALID) assert(read(r.notify, &ack, 1) == 1 && ack == 'P');
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(binder);
    stop_receiver(&r);
}
static void on_signal(int sig) { (void)sig; }
struct sender { int binder; struct mini_binder_command call; atomic_long tid; int error; };
static void *blocked_sender(void *data)
{
    struct sender *s = data;
    atomic_store(&s->tid, syscall(SYS_gettid));
    assert(ioctl(s->binder, MINI_BINDER_WRITE, &s->call) == -1);
    s->error = errno;
    return NULL;
}
static void wait_ioctl_sleep(atomic_long *tid_ptr)
{
    struct timespec delay = {.tv_nsec = 10000000};
    for (int i = 0; i < 500; i++) {
        long tid = atomic_load(tid_ptr), nr = -1;
        char path[80], line[160]; int sleeping = 0; FILE *f;
        if (!tid) { nanosleep(&delay, NULL); continue; }
        snprintf(path, sizeof(path), "/proc/self/task/%ld/status", tid);
        f = fopen(path, "r");
        if (f) { while (fgets(line, sizeof(line), f)) if (!strncmp(line, "State:", 6) && strchr(line, 'S')) sleeping = 1; fclose(f); }
        snprintf(path, sizeof(path), "/proc/self/task/%ld/syscall", tid);
        f = fopen(path, "r");
        if (f) { if (fscanf(f, "%ld", &nr) != 1) nr = -1; fclose(f); }
        if (sleeping && nr == SYS_ioctl) return;
        nanosleep(&delay, NULL);
    }
    assert(!"sender did not sleep in ioctl");
}
static void abandoned(enum mode mode)
{
    struct receiver r = start_receiver(mode);
    struct sender s = {.tid = ATOMIC_VAR_INIT(0)};
    pthread_t thread;
    int p[2], status;
    void *mapping;
    assert(pipe(p) == 0);
    s.binder = open("/dev/mini_binder", O_RDWR); assert(s.binder >= 0); mapping = map(s.binder);
    setup_call(&s.call, r.node, p[1], -1);
    assert(pthread_create(&thread, NULL, blocked_sender, &s) == 0);
    wait_ioctl_sleep(&s.tid); close(p[1]);
    if (mode == CANCEL) {
        assert(pthread_kill(thread, SIGUSR1) == 0);
    } else {
        assert(kill(r.pid, SIGKILL) == 0);
        assert(waitpid(r.pid, &status, 0) == r.pid && WIFSIGNALED(status));
    }
    assert(pthread_join(thread, NULL) == 0);
    assert(s.error == (mode == CANCEL ? EINTR : EPIPE));
    assert_eof(p[0]);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(s.binder);
    if (mode == CANCEL) stop_receiver(&r); else { close(r.notify); close(r.control); }
}
int main(void)
{
    struct sigaction action = {0};
    action.sa_handler = on_signal; sigemptyset(&action.sa_mask);
    assert(sigaction(SIGUSR1, &action, NULL) == 0);
    failure(INVALID);
    abandoned(CANCEL);
    abandoned(DEATH);
    failure(FD_FULL);
    failure(BAD_ADDRESS);
    puts("invalid fd, queued cancellation and receiver death released file references");
    puts("fd-table exhaustion and copy failure rolled back reserved descriptors");
    return 0;
}
