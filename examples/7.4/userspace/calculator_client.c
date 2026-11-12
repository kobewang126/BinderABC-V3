// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "service.h"

int main(int argc, char **argv)
{
    unsigned char *mapping;
    int fd = open_binder(&mapping);
    const char *name = argc == 2 ? argv[1] : CALCULATOR_NAME;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event;
    call.transaction.code = SM_GET_SERVICE; put_name(&call.transaction, name);
    call_service(fd, &call, &event);
    if (event.transaction.status) {
        fprintf(stderr, "getService(%s): %d\n", name, event.transaction.status);
        free_reply(fd); unmap_close(fd, mapping); return 1;
    }
    const unsigned char *data = mapping + event.transaction.reply_offset;
    assert(event.transaction.object_count == 1 && get32(data) == MINI_BINDER_OBJECT_HANDLE);
    uint32_t handle = get32(data + 4); free_reply(fd);
    printf("getService(%s): client handle=%u\n", name, handle);
    for (unsigned int code = CALCULATOR_ADD; code <= CALCULATOR_SUBTRACT; code++) {
        memset(&call, 0, sizeof(call)); call.cmd = MINI_BC_TRANSACTION;
        call.transaction.target_handle = handle; call.transaction.code = code;
        call.transaction.size = 8; put32(call.transaction.data, 7); put32(call.transaction.data + 4, 3);
        call_service(fd, &call, &event);
        assert(event.transaction.status == 0 && event.transaction.size == 4);
        uint32_t result = get32(mapping + event.transaction.reply_offset);
        assert(result == (code == CALCULATOR_ADD ? 10U : 4U));
        printf("remote %s(7, 3) = %u\n", code == CALCULATOR_ADD ? "add" : "sub", result);
        free_reply(fd);
    }
    unmap_close(fd, mapping); return 0;
}
