// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../userspace/object_ipc.h"

int main(int argc, char **argv)
{
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY);
    void *mapping;
    int32_t result;
    char *end;
    unsigned long a, b;
    __u32 missing;
    assert(argc == 3);
    errno = 0;
    a = strtoul(argv[1], &end, 10);
    assert(!errno && *argv[1] && !*end && a > 0 && a <= INT32_MAX);
    b = strtoul(argv[2], &end, 10);
    assert(!errno && *argv[2] && !*end && b > 0 && b <= INT32_MAX && a != b);
    missing = a != 1 && b != 1 ? 1 : a != 2 && b != 2 ? 2 : 3;
    assert(page > 0 && fd >= 0);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    assert(object_transact(fd, mapping, (size_t)page, (__u32)a, MINI_CALC_ADD, 7, 3, &result) == 0 && result == 10);
    assert(object_transact(fd, mapping, (size_t)page, (__u32)b, MINI_CALC_MUL, 7, 3, &result) == 0 && result == 21);
    assert(object_transact(fd, mapping, (size_t)page, missing, MINI_CALC_ADD, 1, 2, &result) == -ENOENT);
    assert(object_transact(fd, mapping, (size_t)page, (__u32)a, MINI_CALC_MUL, 1, 2, &result) == -ENOTTY);
    assert(object_transact(fd, mapping, (size_t)page, (__u32)a, MINI_CALC_ADD, 1, 2, &result) == 0 && result == 3);
    munmap(mapping, (size_t)page);
    close(fd);
    puts("two objects, invalid target and code, recovery passed");
    return 0;
}
