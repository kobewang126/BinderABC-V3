/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MINI_BINDER_COMMAND_H
#define MINI_BINDER_COMMAND_H
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <mini_binder.h>

static inline int command_io(int fd, struct mini_binder_command *out,
                             struct mini_binder_command *in)
{
    struct mini_binder_write_read bwr = {
        .write_size = out ? sizeof(*out) : 0,
        .write_buffer = (uintptr_t)out,
        .read_size = in ? sizeof(*in) : 0,
        .read_buffer = (uintptr_t)in,
    };
    return ioctl(fd, MINI_BINDER_WRITE_READ, &bwr);
}
static inline int mini_write(int fd, struct mini_binder_command *out)
{ return command_io(fd, out, NULL); }
static inline int mini_read(int fd, struct mini_binder_command *in)
{ return command_io(fd, NULL, in); }
static inline int exchange_command(int fd, struct mini_binder_command *out,
                                   struct mini_binder_command *in)
{ return command_io(fd, out, in) < 0 ? -errno : 0; }
#endif
