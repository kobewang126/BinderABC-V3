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
#include "calc_ipc.h"

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
    const char *names[] = {"add", "sub", "mul", "div"};
    __u32 code = 0;
    int32_t a, b, result;
    long page_size = sysconf(_SC_PAGESIZE);
    void *mapping;
    int fd, status;

    if (argc != 4 || parse_int(argv[2], &a) || parse_int(argv[3], &b)) goto usage;
    for (unsigned int i = 0; i < 4; i++) if (!strcmp(argv[1], names[i])) code = i + 1;
    if (!code) goto usage;
    if (page_size < MINI_BINDER_MAX_DATA) return 1;
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    status = calc_transact(fd, mapping, (size_t)page_size, code, a, b, &result);
    munmap(mapping, (size_t)page_size); close(fd);
    if (status) { fprintf(stderr, "calculator: %s\n", strerror(-status)); return 1; }
    printf("%s(%" PRId32 ", %" PRId32 ") = %" PRId32 "\n", argv[1], a, b, result);
    return 0;
usage:
    fprintf(stderr, "usage: calc_client add|sub|mul|div A B\n"); return 2;
}
