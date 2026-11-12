// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "service.h"

int main(void)
{
    unsigned char *mapping;
    int fd = open_binder(&mapping);
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event, next;
    setvbuf(stdout, NULL, _IONBF, 0);
    call.transaction.code = SM_ADD_SERVICE; /* handle 0 is the manager. */
    unsigned int end = put_name(&call.transaction, CALCULATOR_NAME);
    call.transaction.object_count = 1; call.transaction.object_offsets[0] = end;
    call.transaction.size = end + 12;
    put32(call.transaction.data + end, MINI_BINDER_OBJECT_LOCAL);
    put32(call.transaction.data + end + 4, CALCULATOR_COOKIE);
    call_service(fd, &call, &event);
    if (event.transaction.status) {
        fprintf(stderr, "addService failed: %d\n", event.transaction.status);
        free_reply(fd); unmap_close(fd, mapping); return 1;
    }
    free_reply(fd);
    puts("calculator registered: LOCAL(cookie=123)");
    memset(&next, 0, sizeof(next)); next.cmd = MINI_BC_ENTER_LOOPER;
    for (;;) {
        assert(command_io(fd, &next, &event) == 0 && event.cmd == MINI_BR_TRANSACTION);
        const struct mini_binder_transaction_data *tx = &event.transaction;
        const unsigned char *data = (const unsigned char *)(uintptr_t)tx->data_address;
        memset(&next, 0, sizeof(next)); next.cmd = MINI_BC_REPLY; next.transaction.id = tx->id;
        if (tx->object_cookie != CALCULATOR_COOKIE || tx->size != 8 || tx->object_count)
            next.transaction.status = -EINVAL;
        else if (tx->code == CALCULATOR_ADD || tx->code == CALCULATOR_SUBTRACT) {
            /* Unsigned 32-bit arithmetic has defined wraparound behaviour. */
            uint32_t a = get32(data), b = get32(data + 4);
            uint32_t result = tx->code == CALCULATOR_ADD ? a + b : a - b;
            next.transaction.size = 4; put32(next.transaction.data, result);
            printf("%s(%u, %u) = %u\n", tx->code == CALCULATOR_ADD ? "add" : "sub", a, b, result);
        } else next.transaction.status = -ENOSYS;
    }
}
