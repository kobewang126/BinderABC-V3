// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mini_binder.h>
#include <parcel.h>
#include "../userspace/command.h"

static int transact(int fd, void *map, size_t page, __u32 code, __u32 handle, __u32 a, __u32 b, __u32 *value)
{
    struct mini_binder_command request = {.cmd = MINI_BC_TRANSACTION};
    struct mini_binder_command reply = {0}, release = {.cmd = MINI_BC_FREE_REPLY};
    struct parcel p;
    __u32 type, high;
    int ret;
    request.transaction.code = code;
    request.transaction.target_handle = handle;
    parcel_init(&p, request.transaction.data, sizeof(request.transaction.data));
    assert(parcel_write_u32(&p, a) == 0);
    if (code != MINI_CALC_GET_OBJECT) assert(parcel_write_u32(&p, b) == 0);
    request.transaction.size = (__u32)p.size;
    ret = exchange_command(fd, &request, &reply);
    if (ret) return ret;
    ret = reply.transaction.status;
    if (!ret) {
        parcel_init(&p, map, page);
        p.size = reply.transaction.size;
        if (code == MINI_CALC_GET_OBJECT) {
            assert(reply.transaction.object_count == 1 && reply.transaction.object_offsets[0] == 0);
            assert(parcel_read_u32(&p, &type) == 0 && type == MINI_BINDER_OBJECT_HANDLE);
            assert(parcel_read_u32(&p, value) == 0 && parcel_read_u32(&p, &high) == 0 && high == 0);
        } else assert(parcel_read_u32(&p, value) == 0);
    }
    assert(exchange_command(fd, &release, NULL) == 0);
    return ret;
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    int a = open("/dev/mini_binder", O_RDONLY), b = open("/dev/mini_binder", O_RDONLY);
    void *ma, *mb;
    __u32 ha, hb, value, result_a, result_b;
    assert(page > 0 && a >= 0 && b >= 0);
    ma = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, a, 0);
    mb = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, b, 0);
    assert(ma != MAP_FAILED && mb != MAP_FAILED);
    assert(transact(a, ma, page, MINI_CALC_GET_OBJECT, 0, 2, 0, &ha) == -EINVAL);
    assert(transact(a, ma, page, MINI_CALC_GET_OBJECT, 0, 0, 0, &ha) == 0);
    assert(transact(b, mb, page, MINI_CALC_GET_OBJECT, 0, 1, 0, &hb) == 0);
    assert(ha == 1 && hb == 1);
    assert(transact(a, ma, page, MINI_CALC_ADD, ha, 7, 3, &result_a) == 0 && result_a == 10);
    assert(transact(b, mb, page, MINI_CALC_MUL, hb, 7, 3, &result_b) == 0 && result_b == 21);
    assert(transact(a, ma, page, MINI_CALC_ADD, 8, 7, 3, &value) == -ENOENT);
    {
        pid_t child = fork();
        int status;
        assert(child >= 0);
        if (child == 0) {
            int c = open("/dev/mini_binder", O_RDONLY);
            void *mc;
            __u32 hc, answer;
            assert(c >= 0);
            mc = mmap(NULL, (size_t)page, PROT_READ, MAP_SHARED, c, 0);
            assert(mc != MAP_FAILED);
            assert(transact(c, mc, page, MINI_CALC_GET_OBJECT, 0, 1, 0, &hc) == 0 && hc == 1);
            assert(transact(c, mc, page, MINI_CALC_MUL, hc, 7, 3, &answer) == 0 && answer == 21);
            munmap(mc, (size_t)page); close(c);
            _exit(0);
        }
        assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    munmap(ma, (size_t)page); munmap(mb, (size_t)page); close(a); close(b);
    printf("file A: handle %u, add(7, 3) = %u\n", ha, result_a);
    printf("file B: handle %u, mul(7, 3) = %u\n", hb, result_b);
    puts("per-file handles, node conversion and calls passed");
    return 0;
}
