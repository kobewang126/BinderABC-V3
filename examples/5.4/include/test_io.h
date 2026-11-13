/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MINI_BINDER_TEST_IO_H
#define MINI_BINDER_TEST_IO_H
#include <sys/ioctl.h>
#include <mini_binder.h>
static inline int read_request(int fd, struct mini_binder_command *event)
{
    if (event) event->cmd = MINI_READ_REQUEST;
    return ioctl(fd, MINI_BINDER_READ, event);
}
static inline int read_reply(int fd, struct mini_binder_command *event)
{
    if (event) event->cmd = MINI_READ_REPLY;
    return ioctl(fd, MINI_BINDER_READ, event);
}
#endif
