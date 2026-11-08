// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
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

int main(void)
{
    struct mini_binder_transaction_data t = {0};
    struct parcel p;
    const unsigned char *text;
    size_t length;
    uint32_t value;
    __u32 request_size;
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDONLY);
    int second;
    void *mapping;
    void *other;
    struct mini_binder_transaction_data t2 = {0};

    assert(fd >= 0 && page >= MINI_BINDER_MAX_DATA);
    parcel_init(&p, t.data, sizeof(t.data));
    assert(!parcel_write_u32(&p, 7));
    assert(!parcel_write_u32(&p, 3));
    assert(!parcel_write_string(&p, "test", 4));
    t.size = (__u32)p.size;
    request_size = t.size;
    errno = 0;
    assert(ioctl(fd, MINI_BINDER_TRANSACT_MAPPED, &t) == -1 && errno == ENXIO);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    assert(mprotect(mapping, (size_t)page, PROT_READ | PROT_WRITE) == -1 && errno == EACCES);
    assert(ioctl(fd, MINI_BINDER_TRANSACT_MAPPED, &t) == 0);
    assert(t.status == 0 && t.size == 12);
    parcel_init(&p, mapping, (size_t)page);
    p.size = t.size;
    assert(!parcel_read_u32(&p, &value) && value == 10);
    assert(!parcel_read_string(&p, &text, &length));
    assert(length == 4 && !memcmp(text, "test", 4));
    second = open("/dev/mini_binder", O_RDONLY);
    assert(second >= 0);
    other = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, second, 0);
    assert(other != MAP_FAILED);
    parcel_init(&p, t2.data, sizeof(t2.data));
    assert(!parcel_write_u32(&p, 9));
    assert(!parcel_write_u32(&p, 4));
    assert(!parcel_write_string(&p, "other", 5));
    t2.size = (__u32)p.size;
    assert(ioctl(second, MINI_BINDER_TRANSACT_MAPPED, &t2) == 0 && t2.status == 0);
    parcel_init(&p, other, (size_t)page);
    p.size = t2.size;
    assert(!parcel_read_u32(&p, &value) && value == 13);
    parcel_init(&p, mapping, (size_t)page);
    p.size = t.size;
    assert(!parcel_read_u32(&p, &value) && value == 10);
    assert(ioctl(second, MINI_BINDER_RELEASE_REPLY) == 0);
    assert(munmap(other, (size_t)page) == 0);
    close(second);
    errno = 0;
    assert(ioctl(fd, MINI_BINDER_TRANSACT_MAPPED, &t) == -1 && errno == EBUSY);
    assert(ioctl(fd, MINI_BINDER_RELEASE_REPLY) == 0);
    assert(ioctl(fd, MINI_BINDER_RELEASE_REPLY) == -1 && errno == ENOENT);
    t.size = request_size;
    assert(ioctl(fd, MINI_BINDER_TRANSACT_MAPPED, &t) == 0 && t.status == 0);
    assert(ioctl(fd, MINI_BINDER_RELEASE_REPLY) == 0);
    assert(munmap(mapping, (size_t)page) == 0);
    close(fd);
    puts("mapped reply permissions and reuse passed");
    return 0;
}
