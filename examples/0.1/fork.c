#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int value = 10;
    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        value = 20;
        printf("child:  pid=%ld, value=%d, address=%p\n",
               (long)getpid(), value, (void *)&value);
        return 0;
    }

    int status;
    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "child failed\n");
        return 1;
    }
    printf("parent: pid=%ld, value=%d, address=%p\n",
           (long)getpid(), value, (void *)&value);
    return 0;
}
