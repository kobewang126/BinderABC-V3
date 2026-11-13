// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mini_binder.h>

struct argument { int fd; uint32_t id; };
static void *worker(void *data)
{
    struct argument *arg = data;
    uint32_t again;
    assert(ioctl(arg->fd, MINI_BINDER_GET_THREAD_ID, &arg->id) == 0);
    assert(ioctl(arg->fd, MINI_BINDER_GET_THREAD_ID, &again) == 0 && again == arg->id);
    return NULL;
}
static void *overflow(void *data)
{
    struct argument *arg = data;
    assert(ioctl(arg->fd, MINI_BINDER_GET_THREAD_ID, &arg->id) == -1 && errno == ENOSPC);
    return NULL;
}
int main(void)
{
    int fd = open("/dev/mini_binder", O_RDWR), second, shared;
    pthread_t threads[2];
    struct argument args[2];
    uint32_t main_id, shared_id, other_id;
    assert(fd >= 0);
    assert(ioctl(fd, MINI_BINDER_GET_THREAD_ID, &main_id) == 0 && main_id == 1);
    shared = dup(fd); assert(shared >= 0);
    assert(ioctl(shared, MINI_BINDER_GET_THREAD_ID, &shared_id) == 0 && shared_id == main_id);
    second = open("/dev/mini_binder", O_RDWR); assert(second >= 0);
    assert(ioctl(second, MINI_BINDER_GET_THREAD_ID, &other_id) == 0 && other_id == 1);
    for (int i = 0; i < 2; i++) { args[i].fd = fd; assert(pthread_create(&threads[i], NULL, worker, &args[i]) == 0); }
    for (int i = 0; i < 2; i++) assert(pthread_join(threads[i], NULL) == 0);
    assert(args[0].id != args[1].id && args[0].id != main_id && args[1].id != main_id);
    for (int i = 0; i < 5; i++) {
        assert(pthread_create(&threads[0], NULL, worker, &args[0]) == 0);
        assert(pthread_join(threads[0], NULL) == 0);
    }
    assert(pthread_create(&threads[0], NULL, overflow, &args[0]) == 0);
    assert(pthread_join(threads[0], NULL) == 0);
    close(second); close(shared); close(fd);
    puts("one proc, distinct thread records; dup shares proc passed");
    return 0;
}
