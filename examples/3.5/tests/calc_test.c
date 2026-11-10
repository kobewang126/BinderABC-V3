// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../userspace/calc_ipc.h"

int main(void)
{
    const struct { __u32 code; int32_t a, b, expected; int status; } cases[] = {
        {MINI_CALC_ADD, 7, 3, 10, 0}, {MINI_CALC_SUB, 7, 3, 4, 0},
        {MINI_CALC_MUL, 7, 3, 21, 0}, {MINI_CALC_DIV, -7, 3, -2, 0},
        {MINI_CALC_DIV, 7, 0, 0, -EDOM}, {MINI_CALC_DIV, INT32_MIN, -1, 0, -ERANGE},
        {MINI_CALC_ADD, INT32_MAX, 1, 0, -ERANGE}, {MINI_CALC_MUL, INT32_MAX, 2, 0, -ERANGE},
        {999, 1, 2, 0, -ENOTTY}, {MINI_CALC_ADD, 1, 2, 3, 0},
    };
    long page_size = sysconf(_SC_PAGESIZE);
    void *mapping;
    int fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0 || page_size < MINI_BINDER_MAX_DATA) return 1;
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int32_t result = 0;
        int status = calc_transact(fd, mapping, (size_t)page_size, cases[i].code,
                                   cases[i].a, cases[i].b, &result);
        if (status != cases[i].status || (!status && result != cases[i].expected)) {
            fprintf(stderr, "case %u failed: status=%d result=%d\n", i, status, result);
            munmap(mapping, (size_t)page_size); close(fd); return 1;
        }
    }
    {
        struct mini_binder_transaction_data malformed = { .code = MINI_CALC_ADD, .size = 4 };
        struct mini_binder_command request = {.cmd = MINI_BC_TRANSACTION};
        struct mini_binder_command response = {0};
        struct mini_binder_command release = {.cmd = MINI_BC_FREE_REPLY};
        int32_t result = 0;
        request.transaction = malformed;
        if (exchange_command(fd, &request, &response) ||
            response.cmd != MINI_BR_REPLY || response.transaction.status != -EINVAL ||
            exchange_command(fd, &release, NULL) ||
            calc_transact(fd, mapping, (size_t)page_size, MINI_CALC_SUB, 5, 8, &result) ||
            result != -3) {
            fprintf(stderr, "malformed request or recovery failed\n");
            munmap(mapping, (size_t)page_size); close(fd); return 1;
        }
    }
    munmap(mapping, (size_t)page_size); close(fd);
    puts("arithmetic commands and error recovery passed");
    return 0;
}
