// SPDX-License-Identifier: GPL-2.0-only
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <mini_binder.h>

int main(void)
{
    int fd = open("/dev/mini_binder", O_RDONLY);
    long page_size;
    const void *mapping;

    if (fd == -1) {
        perror("open /dev/mini_binder");
        return 1;
    }
    if (ioctl(fd, MINI_BINDER_REGISTER_SERVER) == -1) {
        perror("register server");
        close(fd);
        return 1;
    }
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || (size_t)page_size < sizeof(struct mini_binder_payload)) {
        fprintf(stderr, "cannot determine page size\n");
        close(fd);
        return 1;
    }
    mapping = mmap(NULL, (size_t)page_size, PROT_READ, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    puts("server ready");
    fflush(stdout);
    for (;;) {
        struct mini_binder_request req;
        const struct mini_binder_payload *payload;
        struct mini_binder_reply reply = {0};
        int64_t sum;

        if (ioctl(fd, MINI_BINDER_RECEIVE, &req) == -1) {
            if (errno == EINTR)
                continue;
            perror("receive");
            break;
        }
        if (req.size != sizeof(*payload)) {
            fprintf(stderr, "invalid mapped payload\n");
            break;
        }
        payload = (const struct mini_binder_payload *)mapping;
        sum = (int64_t)payload->a + payload->b;
        reply.id = req.id;
        if (sum < INT32_MIN || sum > INT32_MAX)
            reply.status = -ERANGE;
        else
            reply.result = (__s32)sum;
        if (ioctl(fd, MINI_BINDER_REPLY, &reply) == -1) {
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
