/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef BINDERABC_LAB_H
#define BINDERABC_LAB_H
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>

static inline void put32(unsigned char *p, uint32_t n)
{ for (int i = 0; i < 4; i++) p[i] = (unsigned char)(n >> (8 * i)); }
static inline uint32_t get32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline void object(struct mini_binder_transaction_data *tx, unsigned int index, uint32_t type, uint64_t value)
{
    unsigned int offset = index * 12;
    assert(index < MINI_BINDER_MAX_OBJECTS);
    tx->object_offsets[index] = offset;
    tx->object_count = index + 1; tx->size = offset + 12;
    put32(tx->data + offset, type);
    put32(tx->data + offset + 4, (uint32_t)value);
    put32(tx->data + offset + 8, (uint32_t)(value >> 32));
}
static inline unsigned char *map_buffer(int fd)
{
    void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(p != MAP_FAILED); return p;
}
static inline void unmap_close(int fd, void *mapping)
{ assert(munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)) == 0); assert(close(fd) == 0); }
static inline void free_reply(int fd)
{
    struct mini_binder_command cmd = {.cmd = MINI_BC_FREE_REPLY};
    assert(ioctl(fd, MINI_BINDER_WRITE, &cmd) == 0);
}
static inline void enter_loop(int fd)
{
    (void)fd; /* READ_REQUEST selects service work in this chapter. */
}
static inline void read_request(int fd, struct mini_binder_command *event)
{
    event->cmd = MINI_READ_REQUEST;
    assert(ioctl(fd, MINI_BINDER_READ, event) == 0 && event->cmd == MINI_BR_TRANSACTION);
}
static inline void read_reply(int fd, struct mini_binder_command *event)
{
    event->cmd = MINI_READ_REPLY;
    assert(ioctl(fd, MINI_BINDER_READ, event) == 0 && event->cmd == MINI_BR_REPLY);
}
static inline void transact(int fd, uint32_t handle, uint32_t code, struct mini_binder_command *event)
{
    struct mini_binder_command cmd = {.cmd = MINI_BC_TRANSACTION};
    cmd.transaction.target_handle = handle; cmd.transaction.code = code;
    assert(ioctl(fd, MINI_BINDER_WRITE, &cmd) == 0);
    read_reply(fd, event);
}
static inline void lab_wait_ok(pid_t child)
{
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
#endif
