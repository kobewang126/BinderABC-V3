// SPDX-License-Identifier: GPL-2.0-only
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mini_binder.h>
#include <parcel.h>

int main(void)
{
    struct mini_binder_transaction_data transaction = {0};
    struct parcel p;
    const unsigned char *text;
    size_t length;
    uint32_t number;
    int fd;

    parcel_init(&p, transaction.data, sizeof(transaction.data));
    if (parcel_write_u32(&p, 42) || parcel_write_u32(&p, 10) || parcel_write_string(&p, "Binder", 6))
        return 1;
    transaction.size = (__u32)p.size;
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0 || ioctl(fd, MINI_BINDER_TRANSACT, &transaction) < 0) {
        perror("transaction");
        if (fd >= 0)
            close(fd);
        return 1;
    }
    close(fd);
    if (transaction.status || transaction.size > sizeof(transaction.data)) {
        fprintf(stderr, "invalid reply: status=%d size=%u\n", transaction.status, transaction.size);
        return 1;
    }
    parcel_init(&p, transaction.data, sizeof(transaction.data));
    p.size = transaction.size;
    if (parcel_read_u32(&p, &number) || parcel_read_string(&p, &text, &length) ||
        p.position != p.size) {
        fprintf(stderr, "invalid reply parcel\n");
        return 1;
    }
    printf("reply: number=%" PRIu32 ", text=%.*s\n", number, (int)length, text);
    return 0;
}
