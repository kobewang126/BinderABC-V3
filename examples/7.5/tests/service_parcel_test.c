// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "../userspace/service.h"

int main(void)
{
    unsigned char *mapping;
    int fd = open_binder(&mapping);
    struct mini_binder_command call = {.cmd = MINI_BC_TRANSACTION}, event;
    struct object_parcel request, reply;
    struct parcel_object object;
    object_parcel_init(&request, call.transaction.data, sizeof(call.transaction.data));
    assert(parcel_write_u32(&request.bytes, 33) == 0); /* Truncated service name. */
    parcel_to_transaction(&call.transaction, &request); call.transaction.code = SM_GET_SERVICE;
    assert(command_io(fd, &call, &event) == 0 && event.cmd == MINI_BR_REPLY); assert(event.transaction.status == -EINVAL); free_reply(fd);
    memset(&call, 0, sizeof(call)); call.cmd = MINI_BC_TRANSACTION; call.transaction.code = SM_GET_SERVICE;
    object_parcel_init(&request, call.transaction.data, sizeof(call.transaction.data));
    assert(parcel_write_string(&request.bytes, CALCULATOR_NAME, strlen(CALCULATOR_NAME)) == 0);
    parcel_to_transaction(&call.transaction, &request); assert(command_io(fd, &call, &event) == 0 && event.cmd == MINI_BR_REPLY);
    assert(!event.transaction.status && parcel_from_transaction(&reply, &event.transaction,
           mapping + event.transaction.reply_offset) == 0);
    assert(object_parcel_read_next(&reply, &object) == 0 && object.type == OBJECT_PARCEL_HANDLE);
    uint32_t handle = (uint32_t)object.value; free_reply(fd);
    for (unsigned int count = 1; count <= 3; count++) {
        memset(&call, 0, sizeof(call)); call.cmd = MINI_BC_TRANSACTION;
        call.transaction.target_handle = handle; call.transaction.code = CALCULATOR_ADD;
        object_parcel_init(&request, call.transaction.data, sizeof(call.transaction.data));
        for (unsigned int i = 0; i < count; i++) assert(parcel_write_u32(&request.bytes, i + 1) == 0);
        parcel_to_transaction(&call.transaction, &request); assert(command_io(fd, &call, &event) == 0 && event.cmd == MINI_BR_REPLY);
        assert(event.transaction.status == (count == 2 ? 0 : -EINVAL));
        if (count == 2) {
            uint32_t result;
            assert(parcel_from_transaction(&reply, &event.transaction, mapping + event.transaction.reply_offset) == 0);
            assert(parcel_read_u32(&reply.bytes, &result) == 0 && result == 3);
        }
        free_reply(fd);
    }
    puts("Parcel RPC: truncated name/arguments and trailing arguments rejected; valid call recovered");
    unmap_close(fd, mapping); return 0;
}
