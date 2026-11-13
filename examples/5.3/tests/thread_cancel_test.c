// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include <test_io.h>

struct worker { int fd; unsigned char *mapping; unsigned char input; };
static void on_signal(int sig) { (void)sig; }
static void *client(void *arg)
{
    struct worker *w = arg;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command reply = {0}, release = {.cmd = MINI_BC_FREE_REPLY};
    call.transaction.size = 1; call.transaction.data[0] = w->input;
    if (w->input == 10) {
        assert(ioctl(w->fd, MINI_BINDER_WRITE, &call) == -1 && errno == EINTR);
    } else {
        assert(ioctl(w->fd, MINI_BINDER_WRITE, &call) == 0);
        assert(read_reply(w->fd, &reply) == 0);
        assert(reply.transaction.reply_offset >= 2 * MINI_BINDER_MAX_DATA && reply.transaction.reply_offset < 4 * MINI_BINDER_MAX_DATA);
        assert(w->mapping[reply.transaction.reply_offset] == 21);
        assert(ioctl(w->fd, MINI_BINDER_WRITE, &release) == 0);
    }
    return NULL;
}
static void server(int notify, int control)
{
    int fd = open("/dev/mini_binder", O_RDWR), cancelled = -1;
    long page = sysconf(_SC_PAGESIZE);
    void *mapping;
    unsigned char byte, values[2];
    struct mini_binder_command req[2] = {0}, reply = {.cmd = MINI_BC_REPLY};
    assert(fd >= 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED && write(notify, "R", 1) == 1);
    for (int i = 0; i < 2; i++) {
        assert(read_request(fd, &req[i]) == 0);
        values[i] = *(const unsigned char *)(uintptr_t)req[i].transaction.data_address;
        if (values[i] == 10) cancelled = i;
    }
    assert(cancelled >= 0 && write(notify, "R", 1) == 1);
    close(notify);
    assert(read(control, &byte, 1) == 1);
    reply.transaction.id = req[cancelled].transaction.id;
    reply.transaction.size = 1; reply.transaction.data[0] = 11;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == -1 && errno == ECANCELED);
    reply.transaction.id = req[1 - cancelled].transaction.id;
    reply.transaction.data[0] = 21;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(read_request(fd, &req[0]) == 0);
    reply.transaction.id = req[0].transaction.id;
    reply.transaction.data[0] = 31;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(read(control, &byte, 1) == 1);
    close(control); munmap(mapping, (size_t)page); close(fd); _exit(0);
}
int main(void)
{
    int notify[2], control[2], fd, status;
    long page = sysconf(_SC_PAGESIZE);
    unsigned char byte, *mapping;
    pid_t child;
    pthread_t threads[2];
    struct sigaction action = {0};
    struct worker workers[2];
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command reply = {0}, release = {.cmd = MINI_BC_FREE_REPLY};
    action.sa_handler = on_signal; sigemptyset(&action.sa_mask);
    assert(sigaction(SIGUSR1, &action, NULL) == 0);
    assert(pipe(notify) == 0 && pipe(control) == 0);
    child = fork(); assert(child >= 0);
    if (!child) { close(notify[0]); close(control[1]); server(notify[1], control[0]); }
    close(notify[1]); close(control[0]);
    assert(read(notify[0], &byte, 1) == 1);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    for (int i = 0; i < 2; i++) {
        workers[i] = (struct worker){.fd = fd, .mapping = mapping, .input = (unsigned char)(10 + 10 * i)};
        assert(pthread_create(&threads[i], NULL, client, &workers[i]) == 0);
    }
    assert(read(notify[0], &byte, 1) == 1); close(notify[0]);
    assert(pthread_kill(threads[0], SIGUSR1) == 0 && pthread_join(threads[0], NULL) == 0);
    assert(write(control[1], "G", 1) == 1);
    assert(pthread_join(threads[1], NULL) == 0);
    call.transaction.size = 1; call.transaction.data[0] = 30;
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(read_reply(fd, &reply) == 0);
    assert(reply.transaction.reply_offset >= 2 * MINI_BINDER_MAX_DATA && reply.transaction.reply_offset < 4 * MINI_BINDER_MAX_DATA && mapping[reply.transaction.reply_offset] == 31);
    assert(ioctl(fd, MINI_BINDER_WRITE, &release) == 0);
    assert(write(control[1], "S", 1) == 1); close(control[1]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)page); close(fd);
    puts("one shared-file call cancelled; other reply and slot reuse passed");
    return 0;
}
