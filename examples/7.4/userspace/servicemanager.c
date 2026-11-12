// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "service.h"

int main(void)
{
    unsigned char *mapping;
    int fd = open_binder(&mapping);
    char registered_name[33] = {0};
    uint32_t saved_handle = 0;
    struct mini_binder_command event = {0}, next = {.cmd = MINI_BC_ENTER_LOOPER};
    setvbuf(stdout, NULL, _IONBF, 0);
    assert(ioctl(fd, MINI_BINDER_REGISTER_MANAGER) == 0);
    puts("ServiceManager ready: handle 0");
    for (;;) {
        /* First enter and read; afterwards send the previous reply and read again. */
        assert(command_io(fd, &next, &event) == 0 && event.cmd == MINI_BR_TRANSACTION);
        const struct mini_binder_transaction_data *tx = &event.transaction;
        struct object_parcel request, reply;
        struct parcel_object object;
        char name[33];
        memset(&next, 0, sizeof(next)); next.cmd = MINI_BC_REPLY; next.transaction.id = tx->id;
        object_parcel_init(&reply, next.transaction.data, sizeof(next.transaction.data));
        int status = parcel_from_transaction(&request, tx, (const void *)(uintptr_t)tx->data_address);
        if (!status) status = read_service_name(&request, name);
        if (!status && tx->code == SM_ADD_SERVICE) {
            if (request.count != 1 || object_parcel_read_next(&request, &object) ||
                object.type != OBJECT_PARCEL_HANDLE || request.bytes.position != request.bytes.size)
                status = -EINVAL;
            else if (saved_handle) status = -EEXIST;
            else {
                strcpy(registered_name, name); saved_handle = (uint32_t)object.value;
                printf("addService(%s): manager handle=%u\n", name, saved_handle);
            }
        } else if (!status && tx->code == SM_GET_SERVICE) {
            if (request.count || request.bytes.position != request.bytes.size) status = -EINVAL;
            else if (!saved_handle || strcmp(registered_name, name)) status = -ENOENT;
            else {
                assert(object_parcel_write(&reply, OBJECT_PARCEL_HANDLE, saved_handle) == 0);
                printf("getService(%s): return HANDLE object\n", name);
            }
        } else if (!status) status = -EINVAL;
        next.transaction.status = status;
        parcel_to_transaction(&next.transaction, &reply);
    }
}
