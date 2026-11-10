// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "object_ipc.h"

static int parse_int(const char *text, int32_t *value)
{
    char *end;
    long long n;
    errno = 0;
    n = strtoll(text, &end, 10);
    if (errno || end == text || *end || n < INT32_MIN || n > INT32_MAX) return -1;
    *value = (int32_t)n;
    return 0;
}

int main(int argc, char **argv)
{
    __u32 object, code;
    int32_t a, b, result, node_id;
    long page_size = sysconf(_SC_PAGESIZE);
    void *mapping;
    int fd, status;

    if (argc != 5 || parse_int(argv[1], &node_id) || node_id <= 0 || parse_int(argv[3], &a) || parse_int(argv[4], &b)) goto usage;
    object = (__u32)node_id;
    if (!strcmp(argv[2], "add")) code = MINI_CALC_ADD;
    else if (!strcmp(argv[2], "mul")) code = MINI_CALC_MUL;
    else goto usage;
    if (page_size < MINI_BINDER_MAX_DATA) return 1;
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    status = object_transact(fd, mapping, (size_t)page_size, object, code, a, b, &result);
    munmap(mapping, (size_t)page_size); close(fd);
    if (status) { fprintf(stderr, "calculator: %s\n", strerror(-status)); return 1; }
    printf("%s(%" PRId32 ", %" PRId32 ") = %" PRId32 "\n", argv[2], a, b, result);
    return 0;
usage:
    fprintf(stderr, "usage: object_client NODE_ID add|mul A B\n"); return 2;
}
