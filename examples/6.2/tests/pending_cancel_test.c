// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mini_binder.h>

static void on_signal(int sig) { (void)sig; }
static void client(int cancelled, int ready)
{
    struct sigaction action = {0};
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command reply = {.cmd = MINI_READ_REPLY}, release = {.cmd = MINI_BC_FREE_REPLY};
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDWR);
    unsigned char *mapping;
    action.sa_handler = on_signal; sigemptyset(&action.sa_mask);
    assert(sigaction(SIGUSR1, &action, NULL) == 0 && fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    call.transaction.size = 1; call.transaction.data[0] = cancelled ? 1 : 2;
    assert(write(ready, "R", 1) == 1); close(ready);
    if (cancelled) {
        assert(ioctl(fd, MINI_BINDER_WRITE, &call) == -1 && errno == EINTR);
    } else {
        assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
        assert(ioctl(fd, MINI_BINDER_READ, &reply) == 0 && mapping[reply.transaction.reply_offset] == 42);
        assert(ioctl(fd, MINI_BINDER_WRITE, &release) == 0);
    }
    munmap(mapping, (size_t)page); close(fd); _exit(0);
}
static void wait_in_ioctl(pid_t child)
{
    char path[64];
    struct timespec delay = {.tv_nsec = 10000000};
    snprintf(path, sizeof(path), "/proc/%ld/syscall", (long)child);
    for (int i = 0; i < 500; i++) {
        FILE *f = fopen(path, "r");
        long nr = -1;
        if (f) { if (fscanf(f, "%ld", &nr) != 1) nr = -1; fclose(f); }
        if (nr == SYS_ioctl) return;
        nanosleep(&delay, NULL);
    }
    assert(!"child did not enter ioctl");
}
static pid_t start_client(int cancelled)
{
    int p[2]; char ready; pid_t child;
    assert(pipe(p) == 0); child = fork(); assert(child >= 0);
    if (!child) { close(p[0]); client(cancelled, p[1]); }
    close(p[1]); assert(read(p[0], &ready, 1) == 1); close(p[0]);
    wait_in_ioctl(child);
    return child;
}
int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDWR), status;
    unsigned char *mapping;
    pid_t first, second;
    struct mini_binder_command incoming = {.cmd = MINI_READ_REQUEST}, reply = {.cmd = MINI_BC_REPLY};
    assert(fd >= 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    first = start_client(1);
    assert(kill(first, SIGUSR1) == 0);
    assert(waitpid(first, &status, 0) == first && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    second = start_client(0);
    assert(ioctl(fd, MINI_BINDER_READ, &incoming) == 0);
    assert(incoming.transaction.size == 1 && *(unsigned char *)(uintptr_t)incoming.transaction.data_address == 2);

    reply.transaction.size = 1; reply.transaction.data[0] = 42;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(waitpid(second, &status, 0) == second && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)page); close(fd);
    puts("pending cancellation removes work and next request succeeds");
    return 0;
}
