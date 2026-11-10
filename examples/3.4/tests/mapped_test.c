// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <parcel.h>
#include "../userspace/command.h"

static void prepare(struct mini_binder_command *cmd, uint32_t a, uint32_t b)
{
    struct parcel p;
    *cmd = (struct mini_binder_command){.cmd = MINI_BC_TRANSACTION};
    cmd->transaction.code = MINI_CALC_ADD;
    parcel_init(&p, cmd->transaction.data, sizeof(cmd->transaction.data));
    assert(!parcel_write_u32(&p, a) && !parcel_write_u32(&p, b));
    cmd->transaction.size = (__u32)p.size;
}

static uint32_t read_value(void *mapping, size_t page, size_t size)
{
    struct parcel p;
    uint32_t value;
    parcel_init(&p, mapping, page);
    p.size = size;
    assert(!parcel_read_u32(&p, &value) && p.position == p.size);
    return value;
}

int main(void)
{
    struct mini_binder_command first, second, reply1 = {0}, reply2 = {0};
    struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY);
    int other_fd;
    void *mapping, *other;

    assert(fd >= 0 && page >= MINI_BINDER_MAX_DATA);
    prepare(&first, 7, 3);
    assert(exchange_command(fd, &first, &reply1) == -ENXIO);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    assert(mprotect(mapping, (size_t)page, PROT_READ | PROT_WRITE) == -1 && errno == EACCES);
    assert(!exchange_command(fd, &first, &reply1));
    assert(reply1.cmd == MINI_BR_REPLY && !reply1.transaction.status);
    assert(read_value(mapping, (size_t)page, reply1.transaction.size) == 10);
    other_fd = open("/dev/mini_binder", O_RDONLY);
    assert(other_fd >= 0);
    other = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, other_fd, 0);
    assert(other != MAP_FAILED);
    prepare(&second, 9, 4);
    assert(!exchange_command(other_fd, &second, &reply2));
    assert(reply2.cmd == MINI_BR_REPLY && !reply2.transaction.status);
    assert(read_value(other, (size_t)page, reply2.transaction.size) == 13);
    assert(read_value(mapping, (size_t)page, reply1.transaction.size) == 10);
    assert(!exchange_command(other_fd, &release, NULL));
    assert(munmap(other, (size_t)page) == 0);
    close(other_fd);
    assert(exchange_command(fd, &first, &reply1) == -EBUSY);
    assert(!exchange_command(fd, &release, NULL));
    assert(exchange_command(fd, &release, NULL) == -ENOENT);
    assert(!exchange_command(fd, &first, &reply1) && !reply1.transaction.status);
    assert(!exchange_command(fd, &release, NULL));
    assert(munmap(mapping, (size_t)page) == 0);
    close(fd);
    puts("mapped reply permissions and reuse passed");
    return 0;
}
