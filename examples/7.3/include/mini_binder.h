/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef MINI_BINDER_H
#define MINI_BINDER_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MINI_BINDER_PROTOCOL_VERSION 703U
#define MINI_BINDER_MAX_DATA 256U
#define MINI_BINDER_MAX_OBJECTS 8U
#define MINI_BINDER_OBJECT_LOCAL 1U
#define MINI_BINDER_OBJECT_HANDLE 2U
#define MINI_BINDER_OBJECT_FD 3U
#define MINI_BINDER_IOC_MAGIC 'M'
#define MINI_BINDER_GET_VERSION _IOR(MINI_BINDER_IOC_MAGIC, 0, __u32)
#define MINI_BINDER_REGISTER_MANAGER _IO(MINI_BINDER_IOC_MAGIC, 22)
#define MINI_BINDER_GET_THREAD_ID _IOR(MINI_BINDER_IOC_MAGIC, 23, __u32)
#define MINI_BINDER_THREAD_EXIT _IO(MINI_BINDER_IOC_MAGIC, 24)

struct mini_binder_transaction_data {
    __u64 data_address;
    __u64 object_cookie;
    __u32 target_handle;
    __u32 object_count;
    __u32 object_offsets[MINI_BINDER_MAX_OBJECTS];
    __u32 reply_offset;
    __u32 size;
    __s32 status;
    __u32 code;
    __u8 data[MINI_BINDER_MAX_DATA];
};

enum mini_binder_command_code {
    MINI_BC_TRANSACTION = 1,
    MINI_BC_REPLY = 2,
    MINI_BC_FREE_REPLY = 3,
    MINI_BC_ENTER_LOOPER = 4,
    MINI_BC_EXIT_LOOPER = 5,
    MINI_BR_TRANSACTION = 101,
    MINI_BR_REPLY = 102,
};

struct mini_binder_command {
    __u32 cmd;
    struct mini_binder_transaction_data transaction;
};

#define MINI_BINDER_WRITE _IOW(MINI_BINDER_IOC_MAGIC, 21, struct mini_binder_command)
#define MINI_BINDER_READ _IOR(MINI_BINDER_IOC_MAGIC, 22, struct mini_binder_command)

#endif
