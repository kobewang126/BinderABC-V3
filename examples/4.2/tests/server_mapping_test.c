// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mini_binder.h>

struct reader_args { int fd; int ready; int result; atomic_long tid; };

static int wait_for_blocked_ioctl(long tid)
{
    char path[80], line[256];
    const struct timespec delay = {.tv_nsec = 10000000};
    int i;
    for (i = 0; i < 200; i++) {
        FILE *f;
        int sleeping = 0;
        long nr = -1;
        snprintf(path, sizeof(path), "/proc/self/task/%ld/status", tid);
        f = fopen(path, "r");
        if (!f) return 0;
        while (fgets(line, sizeof(line), f))
            if (!strncmp(line, "State:", 6) && strchr(line, 'S')) sleeping = 1;
        fclose(f);
        snprintf(path, sizeof(path), "/proc/self/task/%ld/syscall", tid);
        f = fopen(path, "r");
        if (!f) return 0;
        if (fscanf(f, "%ld", &nr) != 1) nr = -1;
        fclose(f);
        if (sleeping && nr == SYS_ioctl) return 1;
        nanosleep(&delay, NULL);
    }
    return 0;
}

static void *read_request(void *arg)
{
    struct reader_args *a = arg;
    struct mini_binder_command command;
    atomic_store(&a->tid, syscall(SYS_gettid));
    if (write(a->ready, "R", 1) != 1) { a->result = 1; return NULL; }
    a->result = !(ioctl(a->fd, MINI_BINDER_READ, &command) == -1 && errno == ENXIO);
    return NULL;
}

int main(void)
{
    struct mini_binder_command command;
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY), status, client_fd;
    void *mapping, *other, *destination;
    pid_t child;
    pthread_t thread;
    struct reader_args args;
    int ready[2];
    char byte;
    if (fd < 0 || page <= 0 || ioctl(fd, MINI_BINDER_REGISTER_SERVER)) return 1;
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) return 1;
    other = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    if (other != MAP_FAILED || errno != EBUSY) return 1;
    destination = mmap(NULL, (size_t)page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (destination == MAP_FAILED || munmap(destination, (size_t)page)) return 1;
    other = mremap(mapping, (size_t)page, (size_t)page, MREMAP_MAYMOVE | MREMAP_FIXED, destination);
    if (other != MAP_FAILED || errno != EINVAL) return 1;
    child = fork();
    if (child < 0) return 1;
    if (!child) _exit(ioctl(fd, MINI_BINDER_READ, &command) == -1 && errno == ENXIO ? 0 : 1);
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status)) return 1;
    if (pipe(ready)) return 1;
    args = (struct reader_args){.fd = fd, .ready = ready[1], .result = 1};
    if (pthread_create(&thread, NULL, read_request, &args)) return 1;
    if (read(ready[0], &byte, 1) != 1) return 1;
    if (!wait_for_blocked_ioctl(atomic_load(&args.tid))) {
        fputs("reader did not block in ioctl\n", stderr);
        return 1;
    }
    if (munmap(mapping, (size_t)page)) return 1;
    if (pthread_join(thread, NULL) || args.result) return 1;
    close(ready[0]);
    close(ready[1]);
    if (ioctl(fd, MINI_BINDER_READ, &command) != -1 || errno != ENXIO) return 1;
    client_fd = open("/dev/mini_binder", O_RDONLY);
    if (client_fd < 0) return 1;
    other = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, client_fd, 0);
    if (other == MAP_FAILED) return 1;
    command = (struct mini_binder_command){.cmd = MINI_BC_TRANSACTION};
    if (ioctl(client_fd, MINI_BINDER_WRITE, &command) != -1 || errno != ENXIO) return 1;
    if (munmap(other, (size_t)page)) return 1;
    close(client_fd);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED || munmap(mapping, (size_t)page)) return 1;
    close(fd);
    puts("server mapping ownership, unmap wakeup and remap passed");
    return 0;
}
