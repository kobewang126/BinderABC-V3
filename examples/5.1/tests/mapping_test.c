// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    int fd = open("/dev/mini_binder", O_RDWR), status;
    pid_t child;
    void *mapping, *other;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command req = {.cmd = MINI_READ_REQUEST};
    assert(fd >= 0 && page > 0);
    assert(ioctl(fd, MINI_BINDER_WRITE, &call) == -1 && errno == ENXIO);
    assert(ioctl(fd, MINI_BINDER_READ, &req) == -1 && errno == ENXIO);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    other = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(other == MAP_FAILED && errno == EBUSY);
    assert(mprotect(mapping, (size_t)page, PROT_READ | PROT_WRITE) == -1 && errno == EACCES);
    child = fork(); assert(child >= 0);
    if (!child) {
        struct mini_binder_command child_req = {.cmd = MINI_READ_REQUEST};
        assert(ioctl(fd, MINI_BINDER_WRITE, &call) == -1 && errno == ENXIO);
        assert(ioctl(fd, MINI_BINDER_READ, &child_req) == -1 && errno == ENXIO);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(munmap(mapping, (size_t)page) == 0);
    req.cmd = MINI_READ_REQUEST;
    assert(ioctl(fd, MINI_BINDER_READ, &req) == -1 && errno == ENXIO);
    mapping = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, fd, 0);
    assert(mapping != MAP_FAILED);
    munmap(mapping, (size_t)page); close(fd);
    puts("one read-only mapping, address-space check and remapping passed");
    return 0;
}
