#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    static const char message[] = "Hello from userspace\n";
    size_t offset = 0;

    while (offset < sizeof(message) - 1) {
        ssize_t count = write(STDOUT_FILENO, message + offset,
                              sizeof(message) - 1 - offset);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            perror("write");
            return 1;
        }
        if (count == 0) {
            fprintf(stderr, "write made no progress\n");
            return 1;
        }
        offset += (size_t)count;
    }

    int fd = open("/dev/binderabc-p02-nonexistent", O_RDONLY);
    if (fd >= 0) {
        close(fd);
        fprintf(stderr, "expected the example device to be absent\n");
        return 1;
    }
    int saved_errno = errno;
    printf("open failed: errno=%d (%s)\n", saved_errno, strerror(saved_errno));
    return saved_errno == ENOENT ? 0 : 1;
}
