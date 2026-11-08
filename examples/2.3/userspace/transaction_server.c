// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mini_binder.h>
#include <parcel.h>

int main(void)
{
    int fd = open("/dev/mini_binder", O_RDONLY);
    long page_size = sysconf(_SC_PAGESIZE);
    const unsigned char *mapping;

    if (fd < 0 || page_size < MINI_BINDER_MAX_DATA) {
        perror("open or page size");
        return 1;
    }
    if (ioctl(fd, MINI_BINDER_REGISTER_SERVER) < 0) {
        perror("register server");
        close(fd);
        return 1;
    }
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    puts("transaction server ready");
    fflush(stdout);
    for (;;) {
        struct mini_binder_transaction_data request;
        struct mini_binder_transaction_data reply = {0};
        struct parcel input, output;
        const unsigned char *text;
        size_t length;
        uint32_t number, other;

        if (ioctl(fd, MINI_BINDER_RECEIVE_TRANSACTION, &request) < 0) {
            if (errno == EINTR)
                continue;
            perror("receive");
            break;
        }
        reply.id = request.id;
        parcel_init(&input, (void *)mapping, (size_t)page_size);
        input.size = request.size;
        parcel_init(&output, reply.data, sizeof(reply.data));
        if (request.size > MINI_BINDER_MAX_DATA ||
            parcel_read_u32(&input, &number) || parcel_read_u32(&input, &other) ||
            parcel_read_string(&input, &text, &length) || input.position != input.size ||
            number > UINT32_MAX - other || parcel_write_u32(&output, number + other) ||
            parcel_write_string(&output, (const char *)text, length)) {
            reply.status = -EINVAL;
        } else {
            reply.size = (__u32)output.size;
        }
        if (ioctl(fd, MINI_BINDER_TRANSACTION_REPLY, &reply) < 0) {
            if (errno == ENOENT || errno == ECANCELED)
                continue;
            perror("reply");
            break;
        }
    }
    munmap((void *)mapping, (size_t)page_size);
    close(fd);
    return 1;
}
