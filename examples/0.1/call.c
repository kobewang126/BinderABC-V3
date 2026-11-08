#include <stdio.h>

static int add(int a, int b)
{
    return a + b;
}

int main(void)
{
    int result = add(1, 2);

    printf("add(1, 2) = %d\n", result);
    return result == 3 ? 0 : 1;
}
