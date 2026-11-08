// SPDX-License-Identifier: GPL-2.0-only
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mini_binder.h>
#include <parcel.h>

int main(void)
{
    struct mini_binder_transaction_data transaction = {0};
    struct parcel p;
    const unsigned char *text;
    const void *mapping = MAP_FAILED;
    size_t length;
    uint32_t number;
    long page_size = sysconf(_SC_PAGESIZE);
    int fd = -1, result = 1;

    if (page_size < MINI_BINDER_MAX_DATA)
        return 1;
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0) {
        perror("open");
        goto out;
    }
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        perror("mmap");
        goto out;
    }
    parcel_init(&p, transaction.data, sizeof(transaction.data));
    if (parcel_write_u32(&p, 42) || parcel_write_u32(&p, 10) || parcel_write_string(&p, "Binder", 6))
        goto out;
    transaction.size = (__u32)p.size;
    if (ioctl(fd, MINI_BINDER_TRANSACT_MAPPED, &transaction) < 0) {
        perror("transaction");
        goto out;
    }
    if (transaction.status || transaction.size > MINI_BINDER_MAX_DATA) {
        fprintf(stderr, "invalid reply: status=%d size=%u\n", transaction.status, transaction.size);
        goto release;
    }
    parcel_init(&p, (void *)mapping, (size_t)page_size);
    p.size = transaction.size;
    if (parcel_read_u32(&p, &number) || parcel_read_string(&p, &text, &length) ||
        p.position != p.size) {
        fprintf(stderr, "invalid reply parcel\n");
        goto release;
    }
    printf("mapped reply: number=%" PRIu32 ", text=%.*s\n", number, (int)length, text);
    result = 0;
release:
    if (ioctl(fd, MINI_BINDER_RELEASE_REPLY) < 0) {
        perror("release reply");
        result = 1;
    }
out:
    if (mapping != MAP_FAILED)
        munmap((void *)mapping, (size_t)page_size);
    if (fd >= 0)
        close(fd);
    return result;
}
