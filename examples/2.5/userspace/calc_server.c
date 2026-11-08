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

static int add(int32_t a, int32_t b, int64_t *value)
{
    *value = (int64_t)a + b;
    return 0;
}

static int subtract(int32_t a, int32_t b, int64_t *value)
{
    *value = (int64_t)a - b;
    return 0;
}

static int multiply(int32_t a, int32_t b, int64_t *value)
{
    *value = (int64_t)a * b;
    return 0;
}

static int divide(int32_t a, int32_t b, int64_t *value)
{
    if (!b) return -EDOM;
    *value = (int64_t)a / b;
    return 0;
}

static int calculate(__u32 code, int32_t a, int32_t b, int32_t *result)
{
    int64_t value;
    int status;

    switch (code) {
    case MINI_CALC_ADD: status = add(a, b, &value); break;
    case MINI_CALC_SUB: status = subtract(a, b, &value); break;
    case MINI_CALC_MUL: status = multiply(a, b, &value); break;
    case MINI_CALC_DIV: status = divide(a, b, &value); break;
    default: return -ENOTTY;
    }
    if (status) return status;
    if (value < INT32_MIN || value > INT32_MAX) return -ERANGE;
    *result = (int32_t)value;
    return 0;
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
    puts("calculator server ready"); fflush(stdout);
    for (;;) {
        struct mini_binder_transaction_data request;
        struct mini_binder_transaction_data reply = {0};
        struct parcel input, output;
        uint32_t bits_a, bits_b, bits_result;
        int32_t a, b, result;

        if (ioctl(fd, MINI_BINDER_RECEIVE_TRANSACTION, &request) < 0) {
            if (errno == EINTR) continue;
            perror("receive"); break;
        }
        reply.id = request.id;
        parcel_init(&input, (void *)mapping, (size_t)page_size);
        input.size = request.size;
        parcel_init(&output, reply.data, sizeof(reply.data));
        if (request.size > MINI_BINDER_MAX_DATA ||
            parcel_read_u32(&input, &bits_a) || parcel_read_u32(&input, &bits_b) ||
            input.position != input.size) {
            reply.status = -EINVAL;
        } else {
            memcpy(&a, &bits_a, sizeof(a));
            memcpy(&b, &bits_b, sizeof(b));
            reply.status = calculate(request.code, a, b, &result);
            if (!reply.status) {
                memcpy(&bits_result, &result, sizeof(result));
                if (parcel_write_u32(&output, bits_result)) reply.status = -EINVAL;
                else reply.size = (__u32)output.size;
            }
        }
        if (ioctl(fd, MINI_BINDER_TRANSACTION_REPLY, &reply) < 0) {
            if (errno == ENOENT || errno == ECANCELED) continue;
            perror("reply"); break;
        }
    }
    munmap((void *)mapping, (size_t)page_size); close(fd); return 1;
}
