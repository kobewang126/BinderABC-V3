/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef MINI_BINDER_H
#define MINI_BINDER_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MINI_BINDER_PROTOCOL_VERSION 203U
#define MINI_BINDER_MAX_DATA 256U
#define MINI_BINDER_IOC_MAGIC 'M'
#define MINI_BINDER_GET_VERSION _IOR(MINI_BINDER_IOC_MAGIC, 0, __u32)

struct mini_binder_transaction_data {
    __u64 id;
    __u32 size;
    __s32 status;
    __u32 code;
    __u8 data[MINI_BINDER_MAX_DATA];
};

#define MINI_BINDER_REGISTER_SERVER _IO(MINI_BINDER_IOC_MAGIC, 1)
#define MINI_BINDER_TRANSACT _IOWR(MINI_BINDER_IOC_MAGIC, 5, struct mini_binder_transaction_data)
#define MINI_BINDER_TRANSACTION_REPLY _IOW(MINI_BINDER_IOC_MAGIC, 6, struct mini_binder_transaction_data)
#define MINI_BINDER_RECEIVE_TRANSACTION _IOR(MINI_BINDER_IOC_MAGIC, 20, struct mini_binder_transaction_data)

#endif
