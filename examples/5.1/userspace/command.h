/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MINI_BINDER_COMMAND_H
#define MINI_BINDER_COMMAND_H
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <mini_binder.h>

static inline int exchange_command(int fd, struct mini_binder_command *write_cmd,
                                   struct mini_binder_command *read_cmd)
{
    if (write_cmd) {
        if (ioctl(fd, MINI_BINDER_WRITE, write_cmd) < 0)
            return -errno;
    }
    if (read_cmd) {
        read_cmd->cmd = write_cmd ? MINI_READ_REPLY : MINI_READ_REQUEST;
        if (ioctl(fd, MINI_BINDER_READ, read_cmd) < 0) {
            int error = -errno;
            if (write_cmd && write_cmd->cmd == MINI_BC_TRANSACTION) {
                struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
                ioctl(fd, MINI_BINDER_WRITE, &release);
            }
            return error;
        }
    }
    return 0;
}
#endif
