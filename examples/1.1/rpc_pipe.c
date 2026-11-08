#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct request {
    int32_t a;
    int32_t b;
};

static int write_all(int fd, const void *buffer, size_t length)
{
    const char *p = buffer;

    while (length > 0) {
        ssize_t n = write(fd, p, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        p += n;
        length -= (size_t)n;
    }
    return 0;
}

static int read_all(int fd, void *buffer, size_t length)
{
    char *p = buffer;

    while (length > 0) {
        ssize_t n = read(fd, p, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        p += n;
        length -= (size_t)n;
    }
    return 0;
}

int main(void)
{
    int requests[2];
    int replies[2];

    if (pipe(requests) == -1) {
        perror("pipe requests");
        return 1;
    }
    if (pipe(replies) == -1) {
        perror("pipe replies");
        close(requests[0]);
        close(requests[1]);
        return 1;
    }

    pid_t child = fork();
    if (child == -1) {
        perror("fork");
        close(requests[0]);
        close(requests[1]);
        close(replies[0]);
        close(replies[1]);
        return 1;
    }
    if (child == 0) {
        struct request req;
        int32_t result;

        close(requests[1]);
        close(replies[0]);
        if (read_all(requests[0], &req, sizeof(req)) != 0)
            _exit(1);
        /* The example only accepts values whose sum fits in int32_t. */
        int64_t sum = (int64_t)req.a + req.b;
        if (sum < INT32_MIN || sum > INT32_MAX)
            _exit(1);
        result = (int32_t)sum;
        if (write_all(replies[1], &result, sizeof(result)) != 0)
            _exit(1);
        close(requests[0]);
        close(replies[1]);
        _exit(0);
    }

    struct request req = { .a = 1, .b = 2 };
    int32_t result = 0;
    int failed = 0;
    int status;

    close(requests[0]);
    close(replies[1]);
    if (write_all(requests[1], &req, sizeof(req)) != 0)
        failed = 1;
    close(requests[1]);
    if (!failed && read_all(replies[0], &result, sizeof(result)) != 0)
        failed = 1;
    close(replies[0]);
    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }
    if (failed || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "RPC failed\n");
        return 1;
    }
    printf("remote add(%d, %d) = %d\n", (int)req.a, (int)req.b, (int)result);
    return result == 3 ? 0 : 1;
}
