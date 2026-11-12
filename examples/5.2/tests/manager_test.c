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
#include <parcel_binding.h>

static void put32(unsigned char *p, uint32_t x) { for (int i = 0; i < 4; i++) p[i] = (unsigned char)(x >> (8 * i)); }
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t put_name(unsigned char *data, const char *name)
{
    struct parcel p;
    parcel_init(&p, data, MINI_BINDER_MAX_DATA);
    assert(parcel_write_string(&p, name, strlen(name)) == 0);
    return (uint32_t)p.size;
}
static void ready(int fd) { char c = 'x'; assert(write(fd, &c, 1) == 1); close(fd); }
static void await_ready(int fd) { char c; assert(read(fd, &c, 1) == 1); close(fd); }
static void *map(int fd) { void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0); assert(p != MAP_FAILED); return p; }
static void transact(int fd, struct mini_binder_command *req, struct mini_binder_command *reply)
{ assert(ioctl(fd, MINI_BINDER_WRITE, req) == 0); assert(ioctl(fd, MINI_BINDER_READ, reply) == 0 && reply->cmd == MINI_BR_REPLY); }
static void free_reply(int fd) { struct mini_binder_command c = {.cmd = MINI_BC_FREE_REPLY}; assert(ioctl(fd, MINI_BINDER_WRITE, &c) == 0); }

static void manager(int ready_fd)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    uint32_t saved = 0;
    char name[33] = {0};
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    (void)map(fd);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    ready(ready_fd);
    for (;;) {
        struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
        struct object_parcel request, response;
        struct parcel_object object;
        const unsigned char *text;
        size_t length;
        assert(ioctl(fd, MINI_BINDER_READ, &req) == 0);
        reply.transaction.id = req.transaction.id;
        object_parcel_init(&response, reply.transaction.data, sizeof(reply.transaction.data));
        int invalid = parcel_from_transaction(&request, &req.transaction,
                          (const void *)(uintptr_t)req.transaction.data_address);
        if (invalid || parcel_read_string(&request.bytes, &text, &length) ||
            !length || length > 32 || memchr(text, '\0', length)) {
            reply.transaction.status = -EINVAL;
        } else if (req.transaction.code == 1 && request.count == 1 &&
                   object_parcel_read_next(&request, &object) == 0 &&
                   object.type == OBJECT_PARCEL_HANDLE && object.value &&
                   request.bytes.position == request.bytes.size) {
            memcpy(name, text, length); name[length] = '\0';
            saved = (uint32_t)object.value;
        } else if (req.transaction.code == 2 && !request.count &&
                   request.bytes.position == request.bytes.size && saved &&
                   strlen(name) == length && memcmp(name, text, length) == 0) {
            assert(object_parcel_write(&response, OBJECT_PARCEL_HANDLE, saved) == 0);
        } else reply.transaction.status = -ENOENT;
        parcel_to_transaction(&reply.transaction, &response);
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    }
}
static void provider(int ready_fd)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    struct mini_binder_node_create node = {.cookie = 123};
    struct mini_binder_command req = {.cmd = MINI_BC_TRANSACTION}, result = {0};
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    (void)map(fd);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    struct object_parcel registration;
    object_parcel_init(&registration, req.transaction.data, sizeof(req.transaction.data));
    assert(parcel_write_string(&registration.bytes, "calculator", strlen("calculator")) == 0);
    assert(object_parcel_write(&registration, OBJECT_PARCEL_LOCAL, 123) == 0);
    parcel_to_transaction(&req.transaction, &registration);
    req.transaction.code = 1;
    transact(fd, &req, &result); assert(result.transaction.status == 0); free_reply(fd);
    ready(ready_fd);
    for (;;) {
        struct mini_binder_command in = {0}, reply = {.cmd = MINI_BC_REPLY};
        assert(ioctl(fd, MINI_BINDER_READ, &in) == 0 && in.transaction.object_cookie == 123);
        reply.transaction.id = in.transaction.id; reply.transaction.size = 4; put32(reply.transaction.data, 42);
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
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
    assert(ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
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
    assert(get32(mapping) == MINI_BINDER_OBJECT_HANDLE && get32(mapping + 4) == 1);
    free_reply(fd);
    memset(&req, 0, sizeof(req)); memset(&reply, 0, sizeof(reply));
    req.cmd = MINI_BC_TRANSACTION; req.transaction.target_handle = 1; req.transaction.code = 3;
    transact(fd, &req, &reply); assert(reply.transaction.size == 4 && get32(mapping) == 42); free_reply(fd);
    kill(sm, SIGKILL); waitpid(sm, &status, 0);
    memset(&req, 0, sizeof(req)); req.cmd = MINI_BC_TRANSACTION;
    assert(ioctl(fd, MINI_BINDER_WRITE, &req) == -1 && errno == ENOTCONN);
    memset(&req, 0, sizeof(req)); memset(&reply, 0, sizeof(reply));
    req.cmd = MINI_BC_TRANSACTION; req.transaction.target_handle = 1;
    transact(fd, &req, &reply); assert(get32(mapping) == 42); free_reply(fd);
    kill(service, SIGKILL); waitpid(service, &status, 0);
    memset(&req, 0, sizeof(req)); req.cmd = MINI_BC_TRANSACTION; req.transaction.target_handle = 1;
    assert(ioctl(fd, MINI_BINDER_WRITE, &req) == -1 && errno == ENOENT);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("service calculator: registered, found and called (result 42)");
    puts("manager handle 0: add, get, call and manager exit passed");
    return 0;
}
