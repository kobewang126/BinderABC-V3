// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mini_binder.h>

int main(void)
{
    struct mini_binder_transaction_data transaction = {0};
    int fd = open("/dev/mini_binder", O_RDONLY);

    assert(fd >= 0);
    transaction.size = MINI_BINDER_MAX_DATA + 1;
    errno = 0;
    assert(ioctl(fd, MINI_BINDER_TRANSACT, &transaction) == -1 && errno == EMSGSIZE);
    transaction.size = 4;
    assert(ioctl(fd, MINI_BINDER_TRANSACT, &transaction) == 0);
    assert(transaction.status == -EINVAL && transaction.size == 0);
    memset(transaction.data, 0, sizeof(transaction.data));
    memset(transaction.data, 0xff, 4);
    transaction.data[4] = 1;
    transaction.size = 12;
    assert(ioctl(fd, MINI_BINDER_TRANSACT, &transaction) == 0);
    assert(transaction.status == -EINVAL && transaction.size == 0);
    memset(transaction.data, 0, sizeof(transaction.data));
    transaction.data[0] = 42;
    transaction.data[4] = 10;
    transaction.size = 12;
    assert(ioctl(fd, MINI_BINDER_TRANSACT, &transaction) == 0);
    assert(transaction.status == 0 && transaction.size == 8);
    assert(transaction.data[0] == 52);
    close(fd);
    puts("oversized, malformed and overflowing requests rejected");
    return 0;
}
