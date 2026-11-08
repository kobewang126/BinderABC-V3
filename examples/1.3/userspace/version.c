// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <mini_binder.h>

int main(int argc, char **argv)
{
    int invalid = argc == 2 && strcmp(argv[1], "--invalid") == 0;
    int bad_address = argc == 2 && strcmp(argv[1], "--bad-address") == 0;
    __u32 version = 0;

    if (argc > 2 || (argc == 2 && !invalid && !bad_address)) {
        fprintf(stderr, "usage: %s [--invalid|--bad-address]\n", argv[0]);
        return 2;
    }
    int fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0) {
        perror("open /dev/mini_binder");
        return 1;
    }
    if (invalid) {
        if (ioctl(fd, _IO(MINI_BINDER_IOC_MAGIC, 255)) == -1 && errno == ENOTTY) {
            puts("unknown ioctl rejected: ENOTTY");
            close(fd);
            return 0;
        }
        fprintf(stderr, "unknown ioctl did not return ENOTTY\n");
        close(fd);
        return 1;
    }
    if (bad_address) {
        if (ioctl(fd, MINI_BINDER_GET_VERSION, (void *)0) == -1 && errno == EFAULT) {
            puts("invalid user address rejected: EFAULT");
            close(fd);
            return 0;
        }
        fprintf(stderr, "invalid address did not return EFAULT\n");
        close(fd);
        return 1;
    }
    if (ioctl(fd, MINI_BINDER_GET_VERSION, &version) == -1) {
        perror("MINI_BINDER_GET_VERSION");
        close(fd);
        return 1;
    }
    printf("mini-binder protocol version: %u\n", (unsigned int)version);
    if (close(fd) == -1) {
        perror("close");
        return 1;
    }
    return version == MINI_BINDER_PROTOCOL_VERSION ? 0 : 1;
}
