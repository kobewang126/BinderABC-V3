#include <stddef.h>
#include <stdio.h>
#include <mini_binder.h>

int main(void)
{
    printf("%zu %zu %zu %zu %zu %zu %lu %lu %lu\n",
           sizeof(struct mini_binder_transaction_data),
           offsetof(struct mini_binder_transaction_data, id),
           offsetof(struct mini_binder_transaction_data, size),
           offsetof(struct mini_binder_transaction_data, status),
           offsetof(struct mini_binder_transaction_data, code),
           offsetof(struct mini_binder_transaction_data, data),
           (unsigned long)MINI_BINDER_TRANSACT,
           (unsigned long)MINI_BINDER_TRANSACTION_REPLY,
           (unsigned long)MINI_BINDER_RECEIVE_TRANSACTION);
    return 0;
}
