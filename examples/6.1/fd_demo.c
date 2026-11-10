// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    char path[] = "/tmp/binderabc-fd-XXXXXX";
    char child_data[2], parent_data[2], independent_data[2];
    int fd = mkstemp(path), duplicate, independent, pipefd[2], status;
    pid_t child;
    assert(fd >= 0);
    assert(write(fd, "abcdef", 6) == 6 && lseek(fd, 0, SEEK_SET) == 0);
    independent = open(path, O_RDONLY); assert(independent >= 0);
    assert(unlink(path) == 0);
    duplicate = dup(fd); assert(duplicate >= 0 && duplicate != fd);
    assert(pipe(pipefd) == 0);
    child = fork(); assert(child >= 0);
    if (!child) {
        char data[2];
        close(pipefd[0]);
        assert(read(duplicate, data, sizeof(data)) == sizeof(data));
        assert(write(pipefd[1], data, sizeof(data)) == sizeof(data));
        close(pipefd[1]); close(independent); close(duplicate); close(fd);
        _exit(0);
    }
    close(pipefd[1]);
    assert(read(pipefd[0], child_data, sizeof(child_data)) == sizeof(child_data)); close(pipefd[0]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(read(fd, parent_data, sizeof(parent_data)) == sizeof(parent_data));
    assert(read(independent, independent_data, sizeof(independent_data)) == sizeof(independent_data));
    printf("fd %d and dup %d share an open file\n", fd, duplicate);
    printf("child via dup: %.*s; parent via fd: %.*s\n", 2, child_data, 2, parent_data);
    printf("independent open: %.*s\n", 2, independent_data);
    close(independent); close(duplicate); close(fd);
    return 0;
}
