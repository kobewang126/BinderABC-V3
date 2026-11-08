// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <mini_binder.h>

int main(void)
{
    int fd = open("/dev/mini_binder", O_RDONLY);

    if (fd == -1) {
        perror("open /dev/mini_binder");
        return 1;
    }
    if (ioctl(fd, MINI_BINDER_REGISTER_SERVER) == -1) {
        perror("register server");
        close(fd);
        return 1;
    }
    puts("server ready");
    fflush(stdout);
    for (;;) {
        struct mini_binder_request req;
        struct mini_binder_reply reply = {0};
        int64_t sum;

        if (ioctl(fd, MINI_BINDER_RECEIVE, &req) == -1) {
            if (errno == EINTR)
                continue;
            perror("receive");
            break;
        }
        sum = (int64_t)req.a + req.b;
        reply.id = req.id;
        if (sum < INT32_MIN || sum > INT32_MAX)
            reply.status = -ERANGE;
        else
            reply.result = (__s32)sum;
        if (ioctl(fd, MINI_BINDER_REPLY, &reply) == -1) {
            if (errno == ENOENT)
                continue;
            perror("reply");
            break;
        }
    }
    close(fd);
    return 1;
}
