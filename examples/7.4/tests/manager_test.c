// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include "../userspace/command.h"

static void put32(unsigned char *p, uint32_t x) { for (int i = 0; i < 4; i++) p[i] = (unsigned char)(x >> (8 * i)); }
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t put_name(unsigned char *p, const char *name)
{
    size_t len = strlen(name);
    assert(len && len <= 32);
    put32(p, (uint32_t)len);
    memcpy(p + 4, name, len);
    return (uint32_t)((4 + len + 3) & ~3U);
}
static void ready(int fd) { char c = 'x'; assert(write(fd, &c, 1) == 1); close(fd); }
static void await_ready(int fd) { char c; assert(read(fd, &c, 1) == 1); close(fd); }
static void *map(int fd) { void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0); assert(p != MAP_FAILED); return p; }
static void transact(int fd, struct mini_binder_command *req, struct mini_binder_command *reply)
{ assert(command_io(fd, req, reply) == 0 && reply->cmd == MINI_BR_REPLY); }
static void free_reply(int fd) { struct mini_binder_command c = {.cmd = MINI_BC_FREE_REPLY}; assert(mini_write(fd, &c) == 0); }

static void manager(int ready_fd)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    uint32_t saved = 0;
    char name[33] = {0};
    uint32_t name_len = 0;
    assert(fd >= 0);
    (void)map(fd);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    { struct mini_binder_command enter = {.cmd = MINI_BC_ENTER_LOOPER};
      assert(mini_write(fd, &enter) == 0); }
    ready(ready_fd);
    for (;;) {
        struct mini_binder_command req = {.cmd = 0}, reply = {.cmd = MINI_BC_REPLY};
        const unsigned char *data;
        uint32_t len, end;
        assert(mini_read(fd, &req) == 0);
        data = (const unsigned char *)(uintptr_t)req.transaction.data_address;
        reply.transaction.id = req.transaction.id;
        len = req.transaction.size >= 4 ? get32(data) : 0;
        end = len <= 32 ? (4 + len + 3) & ~3U : 0;
        if (!len || !end || end > req.transaction.size) {
            reply.transaction.status = -EINVAL;
        } else if (req.transaction.code == 1 && req.transaction.size == end + 12 && req.transaction.object_count == 1 &&
                   req.transaction.object_offsets[0] == end && get32(data + end) == MINI_BINDER_OBJECT_HANDLE) {
            memcpy(name, data + 4, len);
            name[len] = '\0';
            name_len = len;
            saved = get32(data + end + 4);
        } else if (req.transaction.code == 2 && req.transaction.size == end && len == name_len &&
                   saved && memcmp(name, data + 4, len) == 0) {
            reply.transaction.size = 12;
            reply.transaction.object_count = 1;
            put32(reply.transaction.data, MINI_BINDER_OBJECT_HANDLE);
            put32(reply.transaction.data + 4, saved);
        } else reply.transaction.status = -ENOENT;
        assert(mini_write(fd, &reply) == 0);
    }
}
static void provider(int ready_fd)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    struct mini_binder_command req = {.cmd = MINI_BC_TRANSACTION}, result = {0};
    assert(fd >= 0);
    (void)map(fd);
    req.transaction.code = 1; req.transaction.object_count = 1;
    req.transaction.object_offsets[0] = put_name(req.transaction.data, "calculator");
    req.transaction.size = req.transaction.object_offsets[0] + 12;
    put32(req.transaction.data + req.transaction.object_offsets[0], MINI_BINDER_OBJECT_LOCAL);
    put32(req.transaction.data + req.transaction.object_offsets[0] + 4, 123);
    transact(fd, &req, &result); assert(result.transaction.status == 0); free_reply(fd);
    { struct mini_binder_command enter = {.cmd = MINI_BC_ENTER_LOOPER};
      assert(mini_write(fd, &enter) == 0); }
    ready(ready_fd);
    for (;;) {
        struct mini_binder_command in = {.cmd = 0}, reply = {.cmd = MINI_BC_REPLY};
        assert(mini_read(fd, &in) == 0 && in.transaction.object_cookie == 123);
        reply.transaction.id = in.transaction.id; reply.transaction.size = 4; put32(reply.transaction.data, 42);
        assert(mini_write(fd, &reply) == 0);
    }
}
int main(void)
{
    int p[2], fd, status; pid_t sm, service;
    unsigned char *mapping;
    struct mini_binder_command req = {.cmd = MINI_BC_TRANSACTION}, reply = {0};
    assert(pipe(p) == 0); sm = fork(); assert(sm >= 0);
    if (!sm) { close(p[0]); manager(p[1]); _exit(1); } close(p[1]); await_ready(p[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == -1 && errno == EBUSY);
    close(fd);
    assert(pipe(p) == 0); service = fork(); assert(service >= 0);
    if (!service) { close(p[0]); provider(p[1]); _exit(1); } close(p[1]); await_ready(p[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0); mapping = map(fd);
    req.transaction.code = 2; req.transaction.size = 4; put32(req.transaction.data, 33);
    transact(fd, &req, &reply); assert(reply.transaction.status == -EINVAL); free_reply(fd);
    memset(&reply, 0, sizeof(reply));
    req.transaction.size = 4; put32(req.transaction.data, 4);
    transact(fd, &req, &reply); assert(reply.transaction.status == -EINVAL); free_reply(fd);
    memset(&reply, 0, sizeof(reply));
    req.transaction.code = 2; req.transaction.size = put_name(req.transaction.data, "missing");
    transact(fd, &req, &reply); assert(reply.transaction.status == -ENOENT); free_reply(fd);
    memset(&reply, 0, sizeof(reply));
    req.transaction.size = put_name(req.transaction.data, "calculator");
    transact(fd, &req, &reply); assert(reply.transaction.status == 0 && reply.transaction.object_count == 1);
    assert(get32(mapping + reply.transaction.reply_offset) == MINI_BINDER_OBJECT_HANDLE && get32(mapping + reply.transaction.reply_offset + 4) == 1);
    free_reply(fd);
    memset(&req, 0, sizeof(req)); memset(&reply, 0, sizeof(reply));
    req.cmd = MINI_BC_TRANSACTION; req.transaction.target_handle = 1; req.transaction.code = 3;
    transact(fd, &req, &reply); assert(reply.transaction.size == 4 && get32(mapping + reply.transaction.reply_offset) == 42); free_reply(fd);
    kill(sm, SIGKILL); waitpid(sm, &status, 0);
    memset(&req, 0, sizeof(req)); req.cmd = MINI_BC_TRANSACTION;
    assert(mini_write(fd, &req) == -1 && errno == ENOTCONN);
    memset(&req, 0, sizeof(req)); memset(&reply, 0, sizeof(reply));
    req.cmd = MINI_BC_TRANSACTION; req.transaction.target_handle = 1;
    transact(fd, &req, &reply); assert(get32(mapping + reply.transaction.reply_offset) == 42); free_reply(fd);
    kill(service, SIGKILL); waitpid(service, &status, 0);
    memset(&req, 0, sizeof(req)); req.cmd = MINI_BC_TRANSACTION; req.transaction.target_handle = 1;
    assert(mini_write(fd, &req) == -1 && errno == ENOENT);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("service calculator: registered, found and called (result 42)");
    puts("manager handle 0: add, get, call and manager exit passed");
    return 0;
}
