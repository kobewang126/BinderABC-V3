// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <mini_binder.h>
#include "../userspace/command.h"

static int wait_ok(pid_t pid)
{
    int status;

    return waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

struct service { int fd, notify; };
static void *receive_and_wait(void *data)
{
    struct service *s = data;
    struct mini_binder_command event = {0};
#if MINI_BINDER_PROTOCOL_VERSION >= 703
    struct mini_binder_command enter = {.cmd = MINI_BC_ENTER_LOOPER};
    if (exchange_command(s->fd, &enter, NULL)) _exit(1);
#elif MINI_BINDER_PROTOCOL_VERSION >= 504
    event.cmd = MINI_READ_REQUEST;
#endif
    if (exchange_command(s->fd, NULL, &event) || event.cmd != MINI_BR_TRANSACTION ||
        write(s->notify, "R", 1) != 1) _exit(1);
    for (;;) pause();
    return NULL;
}
int main(void)
{
    int ready[2], received[2];
    char byte;
    pid_t server, clients[2] = {-1, -1};
    unsigned int i;
    int ok = 0;

    if (pipe(ready) || pipe(received)) {
        perror("pipe");
        return 1;
    }
    server = fork();
    if (server < 0) {
        perror("fork server");
        return 1;
    }
    if (server == 0) {
        int fd = open("/dev/mini_binder", O_RDONLY);
        long page = sysconf(_SC_PAGESIZE);
        pthread_t workers[2];
        struct service service = {.fd = fd, .notify = received[1]};
        close(ready[0]); close(received[0]);
        if (fd < 0 || page <= 0) _exit(1);
#ifdef MINI_BINDER_REGISTER_SERVER
        if (ioctl(fd, MINI_BINDER_REGISTER_SERVER)) _exit(1);
#endif
        if (ioctl(fd, MINI_BINDER_REGISTER_MANAGER) ||
            mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0) == MAP_FAILED) _exit(1);
        for (int j = 0; j < 2; j++)
            if (pthread_create(&workers[j], NULL, receive_and_wait, &service)) _exit(1);
        if (write(ready[1], "R", 1) != 1) _exit(1);
        for (;;) pause();
    }
    close(ready[1]);
    close(received[1]);
    if (read(ready[0], &byte, 1) != 1)
        goto cleanup_server;
    for (i = 0; i < 2; i++) {
      clients[i] = fork();
      if (clients[i] < 0) goto cleanup_server;
      if (clients[i] == 0) {
        struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION,
            .transaction = {.size = 1, .data = {1}}};
        struct mini_binder_command reply = {0};
        int fd = open("/dev/mini_binder", O_RDONLY);
        int result;
        long page_size = sysconf(_SC_PAGESIZE);

        if (fd < 0 || page_size <= 0 ||
            mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0) == MAP_FAILED)
            _exit(1);
        result = exchange_command(fd, &call, &reply);
        _exit(result == -EPIPE ? 0 : 1);
      }
    }
    if (read(received[0], &byte, 1) == 1 && read(received[0], &byte, 1) == 1) {
        kill(server, SIGKILL);
        waitpid(server, NULL, 0);
        server = -1;
        ok = 1;
        for (i = 0; i < 2; i++) {
            if (!wait_ok(clients[i])) ok = 0;
            clients[i] = -1;
        }
    }
cleanup_server:
    for (i = 0; i < 2; i++) if (clients[i] > 0) {
        kill(clients[i], SIGKILL);
        waitpid(clients[i], NULL, 0);
    }
    if (server > 0) {
        kill(server, SIGKILL);
        waitpid(server, NULL, 0);
    }
    close(ready[0]);
    close(received[0]);
    if (!ok) {
        fprintf(stderr, "server death test failed\n");
        return 1;
    }
    puts("server death wakes both clients with EPIPE");
    return 0;
}
