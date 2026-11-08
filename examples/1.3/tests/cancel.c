// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <mini_binder.h>

static void on_signal(int sig) { (void)sig; }

static void run_client(int expected_errno)
{
    struct mini_binder_call call = {.a = 1, .b = 2};
    struct sigaction action = {0};
    int fd, ret;

    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGUSR1, &action, NULL))
        _exit(1);
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0)
        _exit(1);
    ret = ioctl(fd, MINI_BINDER_CALL, &call);
    if (expected_errno)
        _exit(ret == -1 && errno == expected_errno ? 0 : 1);
    _exit(ret == 0 && call.status == 0 && call.result == 3 ? 0 : 1);
}

static int wait_ok(pid_t pid)
{
    int status;

    return waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void)
{
    struct mini_binder_request req;
    struct mini_binder_reply reply = {.result = 3};
    int fd = open("/dev/mini_binder", O_RDONLY);
    pid_t child = -1;
    int ok = 0;

    if (fd < 0 || ioctl(fd, MINI_BINDER_REGISTER_SERVER))
        goto out;
    child = fork();
    if (child == 0)
        run_client(EINTR);
    if (child < 0 || ioctl(fd, MINI_BINDER_RECEIVE, &req))
        goto out;
    if (kill(child, SIGUSR1))
        goto out;
    if (!wait_ok(child)) {
        child = -1;
        goto out;
    }
    child = fork();
    if (child == 0)
        run_client(EBUSY);
    if (child < 0)
        goto out;
    if (!wait_ok(child)) {
        child = -1;
        goto out;
    }
    child = -1;
    if (ioctl(fd, MINI_BINDER_REPLY, &reply) != -1 || errno != ECANCELED)
        goto out;
    child = fork();
    if (child == 0)
        run_client(0);
    if (child < 0 || ioctl(fd, MINI_BINDER_RECEIVE, &req) ||
        ioctl(fd, MINI_BINDER_REPLY, &reply))
        goto out;
    if (!wait_ok(child)) {
        child = -1;
        goto out;
    }
    child = -1;
    ok = 1;
out:
    if (child > 0) {
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
    }
    if (fd >= 0)
        close(fd);
    if (!ok) {
        fprintf(stderr, "cancellation test failed\n");
        return 1;
    }
    puts("cancelled reply discarded; next call succeeded");
    return 0;
}
