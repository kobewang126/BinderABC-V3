// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "service.h"

int main(int argc, char **argv)
{
    unsigned char *mapping;
    int fd = open_binder(&mapping);
    const char *name = argc == 2 ? argv[1] : CALCULATOR_NAME;
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event;
    struct object_parcel request, reply;
    struct parcel_object object;
    call.transaction.code = SM_GET_SERVICE;
    object_parcel_init(&request, call.transaction.data, sizeof(call.transaction.data));
    assert(parcel_write_string(&request.bytes, name, strlen(name)) == 0);
    parcel_to_transaction(&call.transaction, &request);
    call_service(fd, &call, &event);
    if (event.transaction.status) {
        fprintf(stderr, "getService(%s): %d\n", name, event.transaction.status);
        free_reply(fd); unmap_close(fd, mapping); return 1;
    }
    assert(parcel_from_transaction(&reply, &event.transaction, mapping + event.transaction.reply_offset) == 0);
    assert(reply.count == 1 && object_parcel_read_next(&reply, &object) == 0 &&
           object.type == OBJECT_PARCEL_HANDLE && reply.bytes.position == reply.bytes.size);
    uint32_t handle = (uint32_t)object.value; free_reply(fd);
    printf("getService(%s): client handle=%u\n", name, handle);
    for (unsigned int code = CALCULATOR_ADD; code <= CALCULATOR_SUBTRACT; code++) {
        memset(&call, 0, sizeof(call)); call.cmd = MINI_BC_TRANSACTION;
        call.transaction.target_handle = handle; call.transaction.code = code;
        object_parcel_init(&request, call.transaction.data, sizeof(call.transaction.data));
        assert(parcel_write_u32(&request.bytes, 7) == 0 && parcel_write_u32(&request.bytes, 3) == 0);
        parcel_to_transaction(&call.transaction, &request);
        call_service(fd, &call, &event);
        assert(event.transaction.status == 0 && parcel_from_transaction(&reply, &event.transaction,
               mapping + event.transaction.reply_offset) == 0 && !reply.count);
        uint32_t result;
        assert(parcel_read_u32(&reply.bytes, &result) == 0 && reply.bytes.position == reply.bytes.size);
        assert(result == (code == CALCULATOR_ADD ? 10U : 4U));
        printf("remote %s(7, 3) = %u\n", code == CALCULATOR_ADD ? "add" : "sub", result);
        free_reply(fd);
    }
    unmap_close(fd, mapping); return 0;
}
