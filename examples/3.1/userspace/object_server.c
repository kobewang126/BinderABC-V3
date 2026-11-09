// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mini_binder.h>
#include <parcel.h>
#include "command.h"

struct service_object {
    __u32 id;
    int (*invoke)(__u32 code, int32_t a, int32_t b, int32_t *result);
};

static int add(__u32 code, int32_t a, int32_t b, int32_t *result)
{
    int64_t value = (int64_t)a + b;
    if (code != MINI_CALC_ADD) return -ENOTTY;
    if (value < INT32_MIN || value > INT32_MAX) return -ERANGE;
    *result = (int32_t)value;
    return 0;
}

static int multiply(__u32 code, int32_t a, int32_t b, int32_t *result)
{
    int64_t value = (int64_t)a * b;
    if (code != MINI_CALC_MUL) return -ENOTTY;
    if (value < INT32_MIN || value > INT32_MAX) return -ERANGE;
    *result = (int32_t)value;
    return 0;
}

static int dispatch(__u32 id, __u32 code, int32_t a, int32_t b, int32_t *result)
{
    static const struct service_object objects[] = {{1, add}, {2, multiply}};
    for (unsigned int i = 0; i < sizeof(objects) / sizeof(objects[0]); i++)
        if (objects[i].id == id) return objects[i].invoke(code, a, b, result);
    return -ENOENT;
}

int main(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY);
    const unsigned char *mapping;
    if (fd < 0 || page_size < MINI_BINDER_MAX_DATA) { perror("open or page size"); return 1; }
    if (ioctl(fd, MINI_BINDER_REGISTER_SERVER) < 0) { perror("register server"); close(fd); return 1; }
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    puts("object server ready"); fflush(stdout);
    for (;;) {
        struct mini_binder_transaction_data request;
        struct mini_binder_transaction_data reply = {0};
        struct mini_binder_command incoming = {0};
        struct mini_binder_command outgoing = { .cmd = MINI_BC_REPLY };
        struct parcel input, output;
        uint32_t object, bits_a, bits_b, bits_result;
        int32_t a, b, result;

        int ret = exchange_command(fd, NULL, &incoming);
        if (ret == -EINTR) continue;
        if (ret || incoming.cmd != MINI_BR_TRANSACTION) { fprintf(stderr, "receive: %d\n", ret); break; }
        request = incoming.transaction;
        reply.id = request.id;
        parcel_init(&output, reply.data, sizeof(reply.data));
        if (request.data_address < (uintptr_t)mapping ||
            request.data_address - (uintptr_t)mapping > (size_t)page_size ||
            request.size > (size_t)page_size - (request.data_address - (uintptr_t)mapping) ||
            request.size > MINI_BINDER_MAX_DATA) {
            reply.status = -EINVAL;
        } else {
            parcel_init(&input, (void *)(uintptr_t)request.data_address, request.size);
            input.size = request.size;
            if (parcel_read_u32(&input, &object) || parcel_read_u32(&input, &bits_a) || parcel_read_u32(&input, &bits_b) ||
                input.position != input.size) {
                reply.status = -EINVAL;
            } else {
                memcpy(&a, &bits_a, sizeof(a));
                memcpy(&b, &bits_b, sizeof(b));
                reply.status = dispatch(object, request.code, a, b, &result);
                if (!reply.status) {
                    memcpy(&bits_result, &result, sizeof(result));
                    if (parcel_write_u32(&output, bits_result)) reply.status = -EINVAL;
                    else reply.size = (__u32)output.size;
                }
            }
        }
        printf("request id=%llu code=%u size=%u\n",
               (unsigned long long)request.id, request.code, request.size);
        fflush(stdout);
        outgoing.transaction = reply;
        ret = exchange_command(fd, &outgoing, NULL);
        if (ret == -ENOENT || ret == -ECANCELED) continue;
        if (ret) { fprintf(stderr, "reply: %d\n", ret); break; }
    }
    munmap((void *)mapping, (size_t)page_size); close(fd); return 1;
}
