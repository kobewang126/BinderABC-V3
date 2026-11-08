/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef MINI_BINDER_H
#define MINI_BINDER_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MINI_BINDER_PROTOCOL_VERSION 3U
#define MINI_BINDER_IOC_MAGIC 'M'
#define MINI_BINDER_GET_VERSION _IOR(MINI_BINDER_IOC_MAGIC, 0, __u32)

struct mini_binder_call {
    __s32 a;
    __s32 b;
    __s32 result;
    __s32 status;
};

struct mini_binder_request {
    __s32 a;
    __s32 b;
};

struct mini_binder_reply {
    __s32 result;
    __s32 status;
};

#define MINI_BINDER_REGISTER_SERVER _IO(MINI_BINDER_IOC_MAGIC, 1)
#define MINI_BINDER_CALL _IOWR(MINI_BINDER_IOC_MAGIC, 2, struct mini_binder_call)
#define MINI_BINDER_RECEIVE _IOR(MINI_BINDER_IOC_MAGIC, 3, struct mini_binder_request)
#define MINI_BINDER_REPLY _IOW(MINI_BINDER_IOC_MAGIC, 4, struct mini_binder_reply)

#endif
