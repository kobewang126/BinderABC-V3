// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../userspace/object_ipc.h"

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY);
    void *mapping;
    int32_t result;
    assert(page > 0 && fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    assert(object_transact(fd, mapping, (size_t)page, 1, MINI_CALC_ADD, 7, 3, &result) == 0 && result == 10);
    assert(object_transact(fd, mapping, (size_t)page, 2, MINI_CALC_MUL, 7, 3, &result) == 0 && result == 21);
    assert(object_transact(fd, mapping, (size_t)page, 1, MINI_CALC_SUB, 7, 3, &result) == 0 && result == 4);
    assert(object_transact(fd, mapping, (size_t)page, 2, MINI_CALC_SUB, 7, 3, &result) == 0 && result == -4);
    assert(object_transact(fd, mapping, (size_t)page, 1, MINI_CALC_ADD, INT32_MAX, 1, &result) == -ERANGE);
    assert(object_transact(fd, mapping, (size_t)page, 2, MINI_CALC_SUB, INT32_MIN, 1, &result) == -ERANGE);
    assert(object_transact(fd, mapping, (size_t)page, 2, MINI_CALC_MUL, INT32_MAX, 2, &result) == -ERANGE);
    assert(object_transact(fd, mapping, (size_t)page, 3, MINI_CALC_ADD, 1, 2, &result) == -ENOENT);
    assert(object_transact(fd, mapping, (size_t)page, 1, 999, 1, 2, &result) == -ENOTTY);
    assert(object_transact(fd, mapping, (size_t)page, 1, MINI_CALC_ADD, 1, 2, &result) == 0 && result == 3);
    munmap(mapping, (size_t)page);
    close(fd);
    puts("two objects, invalid target and code, recovery passed");
    return 0;
}
