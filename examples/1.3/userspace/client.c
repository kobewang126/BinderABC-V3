// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <mini_binder.h>

static int parse_int32(const char *text, __s32 *value)
{
    char *end;
    long long n;

    errno = 0;
    n = strtoll(text, &end, 10);
    if (errno || end == text || *end || n < INT32_MIN || n > INT32_MAX)
        return -1;
    *value = (__s32)n;
    return 0;
}

int main(int argc, char **argv)
{
    struct mini_binder_call call = {0};
    int fd;

    if (argc != 3 || parse_int32(argv[1], &call.a) || parse_int32(argv[2], &call.b)) {
        fprintf(stderr, "usage: %s <int32> <int32>\n", argv[0]);
        return 2;
    }
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd == -1) {
        perror("open /dev/mini_binder");
        return 1;
    }
    if (ioctl(fd, MINI_BINDER_CALL, &call) == -1) {
        perror("call");
        close(fd);
        return 1;
    }
    close(fd);
    if (call.status) {
        fprintf(stderr, "server error: %d\n", (int)call.status);
        return 1;
    }
    printf("remote add(%d, %d) = %d\n", (int)call.a, (int)call.b, (int)call.result);
    return 0;
}
