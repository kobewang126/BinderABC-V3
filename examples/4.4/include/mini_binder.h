/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef MINI_BINDER_H
#define MINI_BINDER_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MINI_BINDER_PROTOCOL_VERSION 404U
#define MINI_BINDER_MAX_DATA 256U
#define MINI_BINDER_MAX_OBJECTS 8U
#define MINI_BINDER_OBJECT_LOCAL 1U
#define MINI_BINDER_OBJECT_HANDLE 2U
#define MINI_BINDER_IOC_MAGIC 'M'
#define MINI_BINDER_GET_VERSION _IOR(MINI_BINDER_IOC_MAGIC, 0, __u32)
#define MINI_BINDER_REGISTER_MANAGER _IO(MINI_BINDER_IOC_MAGIC, 22)
#define MINI_BINDER_GET_THREAD_ID _IOR(MINI_BINDER_IOC_MAGIC, 23, __u32)

struct mini_binder_transaction_data {
    __u64 id;
    __u64 data_address;
    __u64 target_node;
    __u64 object_cookie;
    __u32 target_handle;
    __u32 object_count;
    __u32 object_offsets[MINI_BINDER_MAX_OBJECTS];
    __u32 size;
    __s32 status;
    __u32 code;
    __u8 data[MINI_BINDER_MAX_DATA];
};

struct mini_binder_node_create {
    __u64 cookie;
    __u64 node_id;
};

enum mini_binder_command_code {
    MINI_BC_TRANSACTION = 1,
    MINI_BC_REPLY = 2,
    MINI_BC_FREE_REPLY = 3,
    MINI_BR_TRANSACTION = 101,
    MINI_BR_REPLY = 102,
};

struct mini_binder_command {
    __u32 cmd;
    struct mini_binder_transaction_data transaction;
};

enum mini_binder_calc_cmd {
    MINI_CALC_ADD = 1,
    MINI_CALC_SUB = 2,
    MINI_CALC_MUL = 3,
    MINI_CALC_DIV = 4,
    MINI_CALC_GET_OBJECT = 5,
};

#define MINI_BINDER_REGISTER_SERVER _IO(MINI_BINDER_IOC_MAGIC, 1)
#define MINI_BINDER_CREATE_NODE _IOWR(MINI_BINDER_IOC_MAGIC, 2, struct mini_binder_node_create)
#define MINI_BINDER_WRITE _IOW(MINI_BINDER_IOC_MAGIC, 21, struct mini_binder_command)
#define MINI_BINDER_READ _IOR(MINI_BINDER_IOC_MAGIC, 22, struct mini_binder_command)

#endif
