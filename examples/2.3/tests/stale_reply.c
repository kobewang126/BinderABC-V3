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

static void on_signal(int sig)
{
    (void)sig;
}

static void client_run(int interrupted)
{
    struct mini_binder_transaction_data call = {.size = 1, .data = {interrupted ? 1 : 40}};
    struct sigaction action = {0};
    int fd;
    int ret;

    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGUSR1, &action, NULL) == -1)
        _exit(1);
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0)
        _exit(1);
    ret = ioctl(fd, MINI_BINDER_TRANSACT, &call);
    if (interrupted)
        _exit(ret == -1 && errno == EINTR ? 0 : 1);
    _exit(ret == 0 && call.status == 0 && call.size == 1 && call.data[0] == 42 ? 0 : 1);
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
    if (first < 0 || ioctl(fd, MINI_BINDER_RECEIVE_TRANSACTION, &old_req))
        goto out;
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
    if (ioctl(fd, MINI_BINDER_TRANSACTION_REPLY, &reply) != -1 || errno != ECANCELED)
        goto out;
    if (ioctl(fd, MINI_BINDER_RECEIVE_TRANSACTION, &new_req))
        goto out;
    if (new_req.size != 1 || payload[0] != 40)
        goto out;
    reply.id = new_req.id;
    reply.data[0] = 42;
    if (ioctl(fd, MINI_BINDER_TRANSACTION_REPLY, &reply))
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
