// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "service.h"

int main(void)
{
    unsigned char *mapping;
    int fd = open_binder(&mapping);
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event, next;
    struct object_parcel registration;
    setvbuf(stdout, NULL, _IONBF, 0);
    call.transaction.code = SM_ADD_SERVICE; /* handle 0 is the manager. */
    object_parcel_init(&registration, call.transaction.data, sizeof(call.transaction.data));
    assert(parcel_write_string(&registration.bytes, CALCULATOR_NAME, strlen(CALCULATOR_NAME)) == 0);
    assert(object_parcel_write(&registration, OBJECT_PARCEL_LOCAL, CALCULATOR_COOKIE) == 0);
    parcel_to_transaction(&call.transaction, &registration);
    assert(command_io(fd, &call, &event) == 0 && event.cmd == MINI_BR_REPLY);
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
        struct object_parcel request, reply;
        uint32_t a, b;
        memset(&next, 0, sizeof(next)); next.cmd = MINI_BC_REPLY;
        object_parcel_init(&reply, next.transaction.data, sizeof(next.transaction.data));
        int status = parcel_from_transaction(&request, tx, (const void *)(uintptr_t)tx->data_address);
        if (status || tx->object_cookie != CALCULATOR_COOKIE || request.count ||
            parcel_read_u32(&request.bytes, &a) || parcel_read_u32(&request.bytes, &b) ||
            request.bytes.position != request.bytes.size) next.transaction.status = -EINVAL;
        else if (tx->code == CALCULATOR_ADD || tx->code == CALCULATOR_SUBTRACT) {
            /* Unsigned 32-bit arithmetic has defined wraparound behaviour. */
            uint32_t result = tx->code == CALCULATOR_ADD ? a + b : a - b;
            assert(parcel_write_u32(&reply.bytes, result) == 0);
            printf("%s(%u, %u) = %u\n", tx->code == CALCULATOR_ADD ? "add" : "sub", a, b, result);
        } else next.transaction.status = -ENOSYS;
        parcel_to_transaction(&next.transaction, &reply);
    }
}
