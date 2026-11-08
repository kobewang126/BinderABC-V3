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

static void on_signal(int sig)
{
    (void)sig;
}

static void client_run(int interrupted)
{
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION,
        .transaction = {.size = 1, .data = {interrupted ? 1 : 40}}};
    struct mini_binder_command response = {0};
    struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
    struct sigaction action = {0};
    int fd;
    int ret;
    long page_size = sysconf(_SC_PAGESIZE);
    const unsigned char *mapping;

    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGUSR1, &action, NULL) == -1)
        _exit(1);
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0)
        _exit(1);
    if (page_size <= 0) _exit(1);
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) _exit(1);
    ret = exchange_command(fd, &call, &response);
    if (interrupted)
        _exit(ret == -EINTR ? 0 : 1);
    if (ret || response.cmd != MINI_BR_REPLY || response.transaction.status ||
        response.transaction.size != 1 || mapping[0] != 42 ||
        exchange_command(fd, &release, NULL)) _exit(1);
    _exit(0);
}

static int wait_ok(pid_t pid)
{
    int status;

    return waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void)
{
    struct mini_binder_transaction_data old_req, new_req;
    struct mini_binder_transaction_data reply = {0};
    struct mini_binder_command incoming = {0};
    struct mini_binder_command outgoing = {.cmd = MINI_BC_REPLY};
    const unsigned char *payload = MAP_FAILED;
    long page_size = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY);
    pid_t first = -1, second = -1;
    int ok = 0;

    if (page_size <= 0 || fd < 0 || ioctl(fd, MINI_BINDER_REGISTER_SERVER))
        goto out;
    payload = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (payload == MAP_FAILED)
        goto out;
    first = fork();
    if (first == 0)
        client_run(1);
    if (first < 0 || exchange_command(fd, NULL, &incoming) || incoming.cmd != MINI_BR_TRANSACTION)
        goto out;
    old_req = incoming.transaction;
    if (old_req.size != 1 || payload[0] != 1)
        goto out;
    if (kill(first, SIGUSR1))
        goto out;
    if (!wait_ok(first)) {
        first = -1;
        goto out;
    }
    first = -1;
    second = fork();
    if (second == 0)
        client_run(0);
    if (second < 0)
        goto out;
    reply.id = old_req.id;
    reply.size = 1;
    reply.data[0] = 99;
    outgoing.transaction = reply;
    if (exchange_command(fd, &outgoing, NULL) != -ECANCELED)
        goto out;
    if (exchange_command(fd, NULL, &incoming) || incoming.cmd != MINI_BR_TRANSACTION)
        goto out;
    new_req = incoming.transaction;
    if (new_req.size != 1 || payload[0] != 40)
        goto out;
    reply.id = new_req.id;
    reply.data[0] = 42;
    outgoing.transaction = reply;
    if (exchange_command(fd, &outgoing, NULL))
        goto out;
    if (!wait_ok(second)) {
        second = -1;
        goto out;
    }
    second = -1;
    ok = 1;
out:
    if (first > 0) {
        kill(first, SIGKILL);
        waitpid(first, NULL, 0);
    }
    if (second > 0) {
        kill(second, SIGKILL);
        waitpid(second, NULL, 0);
    }
    if (payload != MAP_FAILED)
        munmap((void *)payload, (size_t)page_size);
    if (fd >= 0)
        close(fd);
    if (!ok) {
        fprintf(stderr, "stale reply test failed\n");
        return 1;
    }
    puts("cancelled reply discarded; next client received correct result");
    return 0;
}
