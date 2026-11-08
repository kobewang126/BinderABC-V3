// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
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

int main(void)
{
    int ready[2], received[2];
    char byte;
    pid_t server, client;
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
        struct mini_binder_command req = {0};
        int fd = open("/dev/mini_binder", O_RDONLY);
        close(ready[0]);
        close(received[0]);
        if (fd < 0 || ioctl(fd, MINI_BINDER_REGISTER_SERVER) ||
            write(ready[1], "R", 1) != 1 ||
            exchange_command(fd, NULL, &req) || req.cmd != MINI_BR_TRANSACTION ||
            write(received[1], "R", 1) != 1)
            _exit(1);
        for (;;)
            pause();
    }
    close(ready[1]);
    close(received[1]);
    if (read(ready[0], &byte, 1) != 1)
        goto cleanup_server;
    client = fork();
    if (client < 0)
        goto cleanup_server;
    if (client == 0) {
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
    if (read(received[0], &byte, 1) == 1) {
        kill(server, SIGKILL);
        waitpid(server, NULL, 0);
        server = -1;
        ok = wait_ok(client);
    } else {
        kill(client, SIGKILL);
        waitpid(client, NULL, 0);
    }
cleanup_server:
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
    puts("server death wakes client with EPIPE");
    return 0;
}
