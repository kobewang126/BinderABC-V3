// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include "../userspace/command.h"

static void client(unsigned char value)
{
    struct mini_binder_command request = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command response = {0};
    struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY);
    unsigned char *mapping;
    if (fd < 0 || page <= 0) _exit(1);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) _exit(1);
    request.transaction.code = value == 11 ? MINI_CALC_SUB : MINI_CALC_ADD;
    request.transaction.size = 2;
    request.transaction.data[0] = value;
    request.transaction.data[1] = 1;
    if (exchange_command(fd, &request, &response) || response.cmd != MINI_BR_REPLY ||
        response.transaction.status || response.transaction.size != 1 ||
        mapping[2 * MINI_BINDER_MAX_DATA] != (unsigned char)(value == 11 ? value - 1 : value + 1) ||
        exchange_command(fd, &release, NULL))
        _exit(1);
    munmap(mapping, (size_t)page);
    close(fd);
    _exit(0);
}

static int wait_ok(pid_t child)
{
    int status;
    return waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status);
}

int main(void)
{
    struct mini_binder_command received[2] = {0}, reply = {.cmd = MINI_BC_REPLY};
    struct mini_binder_command full = {.cmd = MINI_BC_TRANSACTION}, ignored = {0};
    pid_t child[2] = {-1, -1};
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY), third = -1, ok = 0;
    unsigned char *mapping = MAP_FAILED;
    void *third_mapping = MAP_FAILED;
    unsigned char values[2];
    unsigned int i;

    if (fd < 0 || page <= 0 ||
        ioctl(fd, MINI_BINDER_REGISTER_MANAGER)) goto out;
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) goto out;
    for (i = 0; i < 2; i++) {
        child[i] = fork();
        if (!child[i]) client((unsigned char)(10 + i));
        if (child[i] < 0) goto out;
    }
    for (i = 0; i < 2; i++) {
        if (exchange_command(fd, NULL, &received[i]) || received[i].cmd != MINI_BR_TRANSACTION ||
            received[i].transaction.size != 2 ||
            received[i].transaction.data_address < (uintptr_t)mapping ||
            received[i].transaction.data_address - (uintptr_t)mapping > (size_t)page - 2 ||
            ((const unsigned char *)(uintptr_t)received[i].transaction.data_address)[1] != 1)
            goto out;
        values[i] = *(const unsigned char *)(uintptr_t)received[i].transaction.data_address;
    }
    if (received[0].transaction.id == received[1].transaction.id ||
        received[0].transaction.data_address == received[1].transaction.data_address ||
        values[0] == values[1] || values[0] + values[1] != 21 ||
        received[0].transaction.code == received[1].transaction.code) goto out;
    third = open("/dev/mini_binder", O_RDONLY);
    if (third < 0) goto out;
    third_mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, third, 0);
    if (third_mapping == MAP_FAILED) goto out;
    full.transaction.size = 1;
    if (exchange_command(third, &full, &ignored) != -EBUSY) goto out;
    for (i = 2; i-- > 0;) {
        reply.transaction.id = received[i].transaction.id;
        reply.transaction.size = 1;
        if (received[i].transaction.code == MINI_CALC_ADD)
            reply.transaction.data[0] = values[i] + 1;
        else if (received[i].transaction.code == MINI_CALC_SUB)
            reply.transaction.data[0] = values[i] - 1;
        else goto out;
        if (exchange_command(fd, &reply, NULL)) goto out;
    }
    for (i = 0; i < 2; i++) {
        if (!wait_ok(child[i])) { child[i] = -1; goto out; }
        child[i] = -1;
    }
    child[0] = fork();
    if (!child[0]) client(20);
    if (child[0] < 0 || exchange_command(fd, NULL, &received[0]) ||
        received[0].cmd != MINI_BR_TRANSACTION || received[0].transaction.size != 2 ||
        received[0].transaction.data_address < (uintptr_t)mapping ||
        received[0].transaction.data_address - (uintptr_t)mapping >= (size_t)page ||
        *(const unsigned char *)(uintptr_t)received[0].transaction.data_address != 20) goto out;
    reply.transaction.id = received[0].transaction.id;
    reply.transaction.size = 1;
    reply.transaction.data[0] = 21;
    if (exchange_command(fd, &reply, NULL)) goto out;
    if (!wait_ok(child[0])) { child[0] = -1; goto out; }
    child[0] = -1;
    ok = 1;
out:
    for (i = 0; i < 2; i++) if (child[i] > 0) { kill(child[i], SIGKILL); waitpid(child[i], NULL, 0); }
    if (third_mapping != MAP_FAILED) munmap(third_mapping, (size_t)page);
    if (third >= 0) close(third);
    if (mapping != MAP_FAILED) munmap(mapping, (size_t)page);
    if (fd >= 0) close(fd);
    if (!ok) { fputs("concurrent transactions failed\n", stderr); return 1; }
    puts("two slots, distinct user addresses, reverse replies, full capacity and reuse passed");
    return 0;
}
