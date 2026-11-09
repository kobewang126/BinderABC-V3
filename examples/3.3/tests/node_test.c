// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mini_binder.h>

int main(void)
{
    int fd = open("/dev/mini_binder", O_RDONLY);
    struct mini_binder_node_create a = {.cookie = 1}, b = {.cookie = 2}, c = {.cookie = 3};
    assert(fd >= 0);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &a) == -1 && errno == EPERM);
    assert(ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, (void *)0) == -1 && errno == EFAULT);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &a) == 0);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &b) == 0 && a.node_id != b.node_id);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &a) == -1 && errno == EEXIST);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &c) == -1 && errno == ENOSPC);
    close(fd);
    fd = open("/dev/mini_binder", O_RDONLY);
    assert(fd >= 0 && ioctl(fd, MINI_BINDER_REGISTER_SERVER) == 0);
    assert(ioctl(fd, MINI_BINDER_CREATE_NODE, &c) == 0 && c.node_id != a.node_id && c.node_id != b.node_id);
    close(fd);
    puts("node permission, duplicate, capacity and restart passed");
    return 0;
}
