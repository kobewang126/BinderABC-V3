// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <mini_binder.h>

int main(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    int fd = -1, second = -1, ok = 0;
    void *mapping = MAP_FAILED;
    void *probe;

    if (page_size <= 0)
        return 1;
    fd = open("/dev/mini_binder", O_RDONLY);
    if (fd < 0)
        goto out;
    probe = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (probe != MAP_FAILED) {
        munmap(probe, (size_t)page_size);
        goto out;
    }
    if (errno != EPERM || ioctl(fd, MINI_BINDER_REGISTER_SERVER))
        goto out;
    probe = mmap(NULL, (size_t)page_size * 2, PROT_READ, MAP_SHARED, fd, 0);
    if (probe != MAP_FAILED) {
        munmap(probe, (size_t)page_size * 2);
        goto out;
    }
    if (errno != EINVAL)
        goto out;
    probe = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, page_size);
    if (probe != MAP_FAILED) {
        munmap(probe, (size_t)page_size);
        goto out;
    }
    if (errno != EINVAL)
        goto out;
    probe = mmap(NULL, (size_t)page_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (probe != MAP_FAILED) {
        munmap(probe, (size_t)page_size);
        goto out;
    }
    if (errno != EINVAL)
        goto out;
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED)
        goto out;
    if (mprotect(mapping, (size_t)page_size, PROT_READ | PROT_WRITE) != -1 ||
        errno != EACCES)
        goto out;
    close(fd);
    fd = -1;
    second = open("/dev/mini_binder", O_RDONLY);
    if (second < 0)
        goto out;
    if (ioctl(second, MINI_BINDER_REGISTER_SERVER) != -1 || errno != EBUSY)
        goto out;
    if (munmap(mapping, (size_t)page_size))
        goto out;
    mapping = MAP_FAILED;
    if (ioctl(second, MINI_BINDER_REGISTER_SERVER))
        goto out;
    ok = 1;
out:
    if (mapping != MAP_FAILED)
        munmap(mapping, (size_t)page_size);
    if (fd >= 0)
        close(fd);
    if (second >= 0)
        close(second);
    if (!ok) {
        perror("mmap access test failed");
        return 1;
    }
    puts("mapping permissions and lifetime passed");
    return 0;
}
