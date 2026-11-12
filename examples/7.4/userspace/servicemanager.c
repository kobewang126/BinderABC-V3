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
        const unsigned char *data = (const unsigned char *)(uintptr_t)tx->data_address;
        unsigned int end = 0; char name[33];
        memset(&next, 0, sizeof(next)); next.cmd = MINI_BC_REPLY; next.transaction.id = tx->id;
        int status = get_name(tx, data, name, &end);
        if (!status && tx->code == SM_ADD_SERVICE) {
            if (tx->size != end + 12 || tx->object_count != 1 || tx->object_offsets[0] != end ||
                get32(data + end) != MINI_BINDER_OBJECT_HANDLE) status = -EINVAL;
            else if (saved_handle) status = -EEXIST;
            else {
                strcpy(registered_name, name); saved_handle = get32(data + end + 4);
                printf("addService(%s): manager handle=%u\n", name, saved_handle);
            }
        } else if (!status && tx->code == SM_GET_SERVICE) {
            if (tx->size != end || tx->object_count) status = -EINVAL;
            else if (!saved_handle || strcmp(registered_name, name)) status = -ENOENT;
            else {
                object(&next.transaction, 0, MINI_BINDER_OBJECT_HANDLE, saved_handle);
                printf("getService(%s): return HANDLE object\n", name);
            }
        } else if (!status) status = -EINVAL;
        next.transaction.status = status;
    }
}
