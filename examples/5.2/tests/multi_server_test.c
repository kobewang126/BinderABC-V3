// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include <test_io.h>

static void server(int pipefd, uint32_t value)
{
    int fd = open("/dev/mini_binder", O_RDWR);
    struct mini_binder_node_create node = {.cookie = 42};
    long page = sysconf(_SC_PAGESIZE);
    assert(fd >= 0);
    assert(mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0) != MAP_FAILED);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &node) == 0);
    assert(write(pipefd, &node.node_id, sizeof(node.node_id)) == sizeof(node.node_id));
    close(pipefd);
    for (;;) {
        struct mini_binder_command read_cmd = {0};
        struct mini_binder_command reply = {.cmd = MINI_BC_REPLY};
        assert(read_request(fd, &read_cmd) == 0);
        assert(read_cmd.cmd == MINI_BR_TRANSACTION);
        assert(read_cmd.transaction.target_node == node.node_id);
        assert(read_cmd.transaction.object_cookie == 42);

        reply.transaction.size = sizeof(value);
        __builtin_memcpy(reply.transaction.data, &value, sizeof(value));
        assert(ioctl(fd, MINI_BINDER_WRITE, &reply) == 0);
    }
}

static int call(int fd, const void *mapping, uint64_t node, uint32_t expected)
{
    struct mini_binder_command request = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command response = {0};
    struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
    uint32_t result;
    request.transaction.target_node = node;
    if (ioctl(fd, MINI_BINDER_WRITE, &request) < 0) return -errno;
    assert(read_reply(fd, &response) == 0);
    assert(response.cmd == MINI_BR_REPLY && response.transaction.size == sizeof(result));
    __builtin_memcpy(&result, (const char *)mapping + 2 * MINI_BINDER_MAX_DATA, sizeof(result));
    assert(result == expected);
    assert(ioctl(fd, MINI_BINDER_WRITE, &release) == 0);
    return 0;
}

int main(void)
{
    int pipes[2][2], fd, status;
    pid_t children[2];
    uint64_t ids[2];
    long page = sysconf(_SC_PAGESIZE);
    void *mapping;
    for (int i = 0; i < 2; i++) {
        assert(pipe(pipes[i]) == 0);
        children[i] = fork();
        assert(children[i] >= 0);
        if (!children[i]) { close(pipes[i][0]); server(pipes[i][1], (uint32_t)(i + 1)); _exit(1); }
        close(pipes[i][1]);
        assert(read(pipes[i][0], &ids[i], sizeof(ids[i])) == sizeof(ids[i]));
        close(pipes[i][0]);
    }
    assert(ids[0] != ids[1]);
    fd = open("/dev/mini_binder", O_RDWR);
    assert(fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    assert(call(fd, mapping, ids[0], 1) == 0);
    assert(call(fd, mapping, ids[1], 2) == 0);
    assert(kill(children[0], SIGKILL) == 0 && waitpid(children[0], &status, 0) == children[0]);
    assert(call(fd, mapping, ids[0], 1) == -ENOENT);
    assert(call(fd, mapping, ids[1], 2) == 0);
    assert(kill(children[1], SIGKILL) == 0 && waitpid(children[1], &status, 0) == children[1]);
    munmap(mapping, (size_t)page);
    close(fd);
    puts("two servers: routing and exit isolation passed");
    return 0;
}
