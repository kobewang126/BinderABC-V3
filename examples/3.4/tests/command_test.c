// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mini_binder.h>

int main(void)
{
    struct mini_binder_command command = {.cmd = 999};
    int fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0) return 1;
    if (ioctl(fd, MINI_BINDER_WRITE, &command) != -1 || errno != EINVAL) return 1;
    if (ioctl(fd, MINI_BINDER_WRITE, NULL) != -1 || errno != EFAULT) return 1;
    if (ioctl(fd, MINI_BINDER_READ, &command) != -1 || errno != ENOENT) return 1;
    if (ioctl(fd, _IOWR(MINI_BINDER_IOC_MAGIC, 5, struct mini_binder_transaction_data),
              &command.transaction) != -1 || errno != ENOTTY) return 1;
    close(fd);
    puts("unknown command, invalid address and empty read rejected");
    return 0;
}
