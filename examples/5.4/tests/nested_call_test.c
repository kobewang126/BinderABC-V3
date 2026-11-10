// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>

static void *map(int fd)
{
    void *p = mmap(NULL, (size_t)sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, fd, 0);
    assert(p != MAP_FAILED);
    return p;
}
static void free_reply(int fd)
{
    struct mini_binder_command cmd = {.cmd = MINI_BC_FREE_REPLY};
    assert(ioctl(fd, MINI_BINDER_WRITE, &cmd) == 0);
}
static void stop_wait(int fd)
{
    char c;
    assert(read(fd, &c, 1) == 1);
    close(fd);
}
static void leaf(int ready, int stop)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    void *mapping;
    const unsigned char *data;
    struct mini_binder_node_create node = {.cookie = 77};
    struct mini_binder_command req = {0}, reply = {.cmd = MINI_BC_REPLY};
    struct mini_binder_command callback = {.cmd = MINI_BC_TRANSACTION};
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    mapping = map(fd);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(ready, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id)); close(ready);
    assert(ioctl(fd, MINI_BINDER_READ, &req) == 0 && req.transaction.size == 1);
    data = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    assert(ioctl(fd, MINI_BINDER_WRITE, &callback) == -1 && errno == EBUSY);
    assert(dprintf(STDOUT_FILENO, "third call rejected: EBUSY (two slots occupied)\n") > 0);
    reply.transaction.id = req.transaction.id;
    reply.transaction.size = 1; reply.transaction.data[0] = data[0] * 2;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    stop_wait(stop);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd); _exit(0);
}
struct trace { uint32_t thread_id; uint64_t outer_id, nested_id; unsigned char input, inner, output; };
static void middle(uint64_t target, int ready, int report, int stop)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    unsigned char *mapping;
    const unsigned char *payload;
    uint32_t thread_again;
    struct trace t = {0};
    struct mini_binder_command req = {0}, call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command result = {0}, reply = {.cmd = MINI_BC_REPLY};
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    mapping = map(fd);
    assert(ioctl(fd, MINI_BINDER_GET_THREAD_ID, &t.thread_id) == 0);
    assert(write(ready, "R", 1) == 1); close(ready);
    assert(ioctl(fd, MINI_BINDER_READ, &req) == 0 && req.transaction.size == 1);
    payload = (const unsigned char *)(uintptr_t)req.transaction.data_address;
    t.input = payload[0]; t.outer_id = req.transaction.id;
    call.transaction.target_node = target;
    call.transaction.size = 1; call.transaction.data[0] = t.input;
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &result) == 0 && result.transaction.size == 1);
    assert(result.transaction.reply_offset >= 2 * MINI_BINDER_MAX_DATA);
    t.inner = mapping[result.transaction.reply_offset];
    t.nested_id = result.transaction.id;
    assert(payload[0] == t.input);
    assert(ioctl(fd, MINI_BINDER_GET_THREAD_ID, &thread_again) == 0 && thread_again == t.thread_id);
    free_reply(fd);
    t.output = t.inner + 1;
    reply.transaction.id = req.transaction.id;
    reply.transaction.size = 1; reply.transaction.data[0] = t.output;
    assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    assert(write(report, &t, sizeof(t)) == sizeof(t)); close(report);
    stop_wait(stop);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd); _exit(0);
}
int main(void)
{
    int lr[2], ls[2], mr[2], ms[2], report[2], fd, status;
    uint64_t target;
    char byte;
    pid_t leaf_pid, middle_pid;
    unsigned char *mapping;
    struct trace t;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, result = {0};
    assert(pipe(lr) == 0 && pipe(ls) == 0);
    leaf_pid = fork(); assert(leaf_pid >= 0);
    if (!leaf_pid) { close(lr[0]); close(ls[1]); leaf(lr[1], ls[0]); }
    close(lr[1]); close(ls[0]);
    assert(read(lr[0], &target, sizeof(target)) == sizeof(target)); close(lr[0]);
    assert(pipe(mr) == 0 && pipe(ms) == 0 && pipe(report) == 0);
    middle_pid = fork(); assert(middle_pid >= 0);
    if (!middle_pid) { close(mr[0]); close(ms[1]); close(report[0]); middle(target, mr[1], report[1], ms[0]); }
    close(mr[1]); close(ms[0]); close(report[1]);
    assert(read(mr[0], &byte, 1) == 1); close(mr[0]);
    fd = open("/dev/mini_binder", O_RDWR); assert(fd >= 0); mapping = map(fd);
    call.transaction.size = 1; call.transaction.data[0] = 7;
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == 0);
    assert(ioctl(fd, MINI_BINDER_READ, &result) == 0);
    assert(result.transaction.size == 1 && mapping[result.transaction.reply_offset] == 15);
    free_reply(fd);
    assert(read(report[0], &t, sizeof(t)) == sizeof(t)); close(report[0]);
    assert(t.input == 7 && t.inner == 14 && t.output == 15 && t.outer_id && t.nested_id && t.outer_id != t.nested_id);
    printf("server thread %u: transaction %llu -> nested transaction %llu\n", t.thread_id,
           (unsigned long long)t.outer_id, (unsigned long long)t.nested_id);
    printf("client: %u -> %u -> %u\n", (unsigned int)t.input, (unsigned int)t.inner, (unsigned int)t.output);
    assert(write(ms[1], "S", 1) == 1 && write(ls[1], "S", 1) == 1);
    close(ms[1]); close(ls[1]);
    assert(waitpid(middle_pid, &status, 0) == middle_pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(waitpid(leaf_pid, &status, 0) == leaf_pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    munmap(mapping, (size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("same service thread completed a nested synchronous call");
    return 0;
}
