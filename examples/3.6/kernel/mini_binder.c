// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <asm/unaligned.h>

#include <mini_binder.h>

enum rpc_state { RPC_IDLE, RPC_PENDING, RPC_PROCESSING, RPC_DONE, RPC_FAILED, RPC_CANCELLED };

struct mini_binder_file {
    bool is_server;
    void *buffer;
    struct vm_area_struct *server_vma;
    bool inflight;
    bool reply_ready;
    bool reply_delivered;
    __u64 refs[MINI_BINDER_MAX_OBJECTS];
    struct mini_binder_transaction_data pending_reply;
};

struct transaction_slot {
    struct file *receiver;
    enum rpc_state state;
    struct mini_binder_transaction_data request;
    struct mini_binder_transaction_data reply;
};

struct mini_binder_node {
    __u64 id;
    __u64 cookie;
    struct file *owner;
};

#define SLOT_COUNT 2
static DEFINE_MUTEX(state_lock);
static DEFINE_SPINLOCK(mapping_lock);
static DECLARE_WAIT_QUEUE_HEAD(server_wait);
static DECLARE_WAIT_QUEUE_HEAD(client_wait);
static struct file *server_file;
static struct transaction_slot slots[SLOT_COUNT];
static __u64 next_id;
static struct mini_binder_node nodes[8];
static __u64 next_node_id;

/* Resolve sender-local objects and stage receiver-local references atomically. */
static long translate_request(struct file *sender_file,
                              struct mini_binder_file *receiver,
                              struct mini_binder_transaction_data *transaction)
{
    __u64 refs[MINI_BINDER_MAX_OBJECTS];
    __u32 previous = 0;
    unsigned int i, j, k;

    if (transaction->object_count > MINI_BINDER_MAX_OBJECTS) return -EINVAL;
    memcpy(refs, receiver->refs, sizeof(refs));
    for (i = 0; i < transaction->object_count; i++) {
        __u32 offset = transaction->object_offsets[i];
        __u64 id;
        if ((offset & 3) || offset > transaction->size || transaction->size - offset < 12 ||
            (i && (offset < previous || offset - previous < 12))) return -EINVAL;
        if (get_unaligned_le32(transaction->data + offset) != MINI_BINDER_OBJECT_LOCAL) return -EINVAL;
        id = get_unaligned_le32(transaction->data + offset + 4);
        id |= (__u64)get_unaligned_le32(transaction->data + offset + 8) << 32;
        for (j = 0; j < ARRAY_SIZE(nodes); j++)
            if (nodes[j].owner == sender_file && nodes[j].cookie == id) break;
        if (j == ARRAY_SIZE(nodes)) return -ENOENT;
        id = nodes[j].id;
        for (k = 0; k < ARRAY_SIZE(refs); k++) if (refs[k] == id) break;
        if (k == ARRAY_SIZE(refs)) {
            for (k = 0; k < ARRAY_SIZE(refs); k++) if (!refs[k]) break;
            if (k == ARRAY_SIZE(refs)) return -ENOSPC;
            refs[k] = id;
        }
        put_unaligned_le32(MINI_BINDER_OBJECT_HANDLE, transaction->data + offset);
        put_unaligned_le32(k + 1, transaction->data + offset + 4);
        put_unaligned_le32(0, transaction->data + offset + 8);
        previous = offset;
    }
    memcpy(receiver->refs, refs, sizeof(refs));
    return 0;
}

/* Called with state_lock held; publish refs only after every object validates. */
static long translate_reply(struct mini_binder_file *ctx,
                            struct mini_binder_transaction_data *reply,
                            struct file *owner)
{
    __u64 refs[MINI_BINDER_MAX_OBJECTS];
    unsigned int i, j, k;
    __u32 previous = 0;

    if (reply->object_count > MINI_BINDER_MAX_OBJECTS) return -EINVAL;
    memcpy(refs, ctx->refs, sizeof(refs));
    for (i = 0; i < reply->object_count; i++) {
        __u32 offset = reply->object_offsets[i];
        __u64 cookie;
        if ((offset & 3) || offset > reply->size || reply->size - offset < 12 ||
            (i && (offset < previous || offset - previous < 12))) return -EINVAL;
        if (get_unaligned_le32(reply->data + offset) != MINI_BINDER_OBJECT_LOCAL) return -EINVAL;
        cookie = get_unaligned_le32(reply->data + offset + 4);
        cookie |= (__u64)get_unaligned_le32(reply->data + offset + 8) << 32;
        for (j = 0; j < ARRAY_SIZE(nodes); j++)
            if (nodes[j].owner == owner && nodes[j].cookie == cookie) break;
        if (j == ARRAY_SIZE(nodes)) return -ENOENT;
        for (k = 0; k < ARRAY_SIZE(refs); k++) if (refs[k] == nodes[j].id) break;
        if (k == ARRAY_SIZE(refs)) {
            for (k = 0; k < ARRAY_SIZE(refs); k++) if (!refs[k]) break;
            if (k == ARRAY_SIZE(refs)) return -ENOSPC;
            refs[k] = nodes[j].id;
        }
        put_unaligned_le32(MINI_BINDER_OBJECT_HANDLE, reply->data + offset);
        put_unaligned_le32(k + 1, reply->data + offset + 4);
        put_unaligned_le32(0, reply->data + offset + 8);
        previous = offset;
    }
    memcpy(ctx->refs, refs, sizeof(refs));
    return 0;
}

static long create_node(struct file *file, void __user *arg)
{
    struct mini_binder_node_create create;
    unsigned int i, free_index = ARRAY_SIZE(nodes);
    long ret = 0;

    if (copy_from_user(&create, arg, sizeof(create))) return -EFAULT;
    if (!create.cookie) return -EINVAL;
    mutex_lock(&state_lock);
    if (!((struct mini_binder_file *)file->private_data)->is_server) { ret = -EPERM; goto out; }
    for (i = 0; i < ARRAY_SIZE(nodes); i++) {
        if (nodes[i].owner == file && nodes[i].cookie == create.cookie) { ret = -EEXIST; goto out; }
        if (!nodes[i].owner && free_index == ARRAY_SIZE(nodes)) free_index = i;
    }
    if (free_index == ARRAY_SIZE(nodes)) { ret = -ENOSPC; goto out; }
    i = free_index;
    if (next_node_id == U64_MAX) { ret = -EOVERFLOW; goto out; }
    create.node_id = next_node_id + 1;
    if (copy_to_user(arg, &create, sizeof(create))) { ret = -EFAULT; goto out; }
    nodes[i].id = ++next_node_id;
    nodes[i].cookie = create.cookie;
    nodes[i].owner = file;
out:
    mutex_unlock(&state_lock);
    return ret;
}

static int mini_binder_open(struct inode *inode, struct file *file)
{
    struct mini_binder_file *ctx;

    (void)inode;
    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    file->private_data = ctx;
    return 0;
}

static long register_server(struct file *file)
{
    struct mini_binder_file *ctx = file->private_data;
    void *buffer = vmalloc_user(PAGE_SIZE);
    long ret = 0;

    if (!buffer)
        return -ENOMEM;
    mutex_lock(&state_lock);
    if (ctx->is_server || ctx->buffer)
        ret = -EBUSY;
    else {
        ctx->buffer = buffer;
        WRITE_ONCE(ctx->is_server, true);
        if (!server_file) WRITE_ONCE(server_file, file);
    }
    mutex_unlock(&state_lock);
    if (ret)
        vfree(buffer);
    return ret;
}

static void server_vma_close(struct vm_area_struct *vma)
{
    struct mini_binder_file *ctx = vma->vm_private_data;
    bool cleared = false;
    spin_lock(&mapping_lock);
    if (ctx->server_vma == vma) {
        ctx->server_vma = NULL;
        cleared = true;
    }
    spin_unlock(&mapping_lock);
    if (cleared)
        wake_up_interruptible_all(&server_wait);
}

static bool server_mapping_valid(struct mini_binder_file *ctx, struct mm_struct *mm)
{
    bool valid;
    spin_lock(&mapping_lock);
    valid = ctx->server_vma && ctx->server_vma->vm_mm == mm;
    spin_unlock(&mapping_lock);
    return valid;
}

static int server_vma_mremap(struct vm_area_struct *vma)
{
    (void)vma;
    return -EINVAL;
}

static const struct vm_operations_struct server_vm_ops = {
    .close = server_vma_close,
    .mremap = server_vma_mremap,
};

static int mini_binder_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct mini_binder_file *ctx = file->private_data;
    int ret;

    if (vma->vm_pgoff || vma->vm_end - vma->vm_start != PAGE_SIZE ||
        !(vma->vm_flags & VM_MAYSHARE) || (vma->vm_flags & (VM_WRITE | VM_EXEC)))
        return -EINVAL;
    mutex_lock(&state_lock);
    if (ctx->is_server) {
        bool mapped;
        spin_lock(&mapping_lock);
        mapped = ctx->server_vma != NULL;
        spin_unlock(&mapping_lock);
        if (mapped) {
            mutex_unlock(&state_lock);
            return -EBUSY;
        }
    }
    if (!ctx->buffer)
        ctx->buffer = vmalloc_user(PAGE_SIZE);
    if (!ctx->buffer) {
        ret = -ENOMEM;
    } else {
        vm_flags_clear(vma, VM_MAYWRITE | VM_MAYEXEC);
        vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
        ret = remap_vmalloc_range(vma, ctx->buffer, 0);
        if (!ret && ctx->is_server) {
            vm_flags_set(vma, VM_DONTCOPY);
            vma->vm_ops = &server_vm_ops;
            vma->vm_private_data = ctx;
            spin_lock(&mapping_lock);
            ctx->server_vma = vma;
            spin_unlock(&mapping_lock);
        }
    }
    mutex_unlock(&state_lock);
    return ret;
}

static long call_server(struct file *file, void __user *arg)
{
    struct mini_binder_file *client_ctx = file->private_data;
    struct mini_binder_transaction_data transaction;
    struct mini_binder_file *server_ctx;
    struct file *receiver;
    struct transaction_slot *slot = NULL;
    unsigned int i;
    long ret;

    if (copy_from_user(&transaction, arg, sizeof(transaction)))
        return -EFAULT;
    if (transaction.size > MINI_BINDER_MAX_DATA)
        return -EMSGSIZE;
    mutex_lock(&state_lock);
    if (!client_ctx->buffer) {
        ret = -ENXIO;
        goto unlock_state;
    }
    if (client_ctx->reply_ready || client_ctx->inflight) {
        ret = -EBUSY;
        goto unlock_state;
    }
    if (next_id == U64_MAX) {
        ret = -EOVERFLOW;
        goto unlock_state;
    }
    if (transaction.target_handle) {
        if (transaction.target_node || transaction.target_handle > ARRAY_SIZE(client_ctx->refs) ||
            !client_ctx->refs[transaction.target_handle - 1]) { ret = -ENOENT; goto unlock_state; }
        transaction.target_node = client_ctx->refs[transaction.target_handle - 1];
    }
    if (transaction.target_node) {
        for (i = 0; i < ARRAY_SIZE(nodes); i++)
            if (nodes[i].owner && nodes[i].id == transaction.target_node)
                break;
        if (i == ARRAY_SIZE(nodes)) { ret = -ENOENT; goto unlock_state; }
        transaction.object_cookie = nodes[i].cookie;
        receiver = nodes[i].owner;
    } else {
        transaction.object_cookie = 0;
        receiver = server_file;
    }
    if (!receiver) { ret = -ENOTCONN; goto unlock_state; }
    if (receiver == file) { ret = -EINVAL; goto unlock_state; }
    server_ctx = receiver->private_data;
    spin_lock(&mapping_lock);
    if (!server_ctx->server_vma) {
        spin_unlock(&mapping_lock);
        ret = -ENXIO;
        goto unlock_state;
    }
    spin_unlock(&mapping_lock);
    for (i = 0; i < SLOT_COUNT; i++) {
        if (slots[i].state == RPC_IDLE) {
            slot = &slots[i];
            break;
        }
    }
    if (!slot) {
        ret = -EBUSY;
        goto unlock_state;
    }
    ret = translate_request(file, server_ctx, &transaction);
    if (ret) goto unlock_state;
    slot->request.id = ++next_id;
    memcpy((char *)server_ctx->buffer + i * MINI_BINDER_MAX_DATA, transaction.data, transaction.size);
    slot->request.size = transaction.size;
    slot->request.code = transaction.code;
    slot->request.target_node = transaction.target_node;
    slot->request.object_cookie = transaction.object_cookie;
    slot->request.object_count = transaction.object_count;
    memcpy(slot->request.object_offsets, transaction.object_offsets, sizeof(transaction.object_offsets));
    WRITE_ONCE(slot->receiver, receiver);
    client_ctx->inflight = true;
    WRITE_ONCE(slot->state, RPC_PENDING);
    mutex_unlock(&state_lock);
    wake_up_interruptible_all(&server_wait);

    ret = wait_event_interruptible(client_wait,
            READ_ONCE(slot->state) == RPC_DONE || READ_ONCE(slot->state) == RPC_FAILED);
    mutex_lock(&state_lock);
    if (!ret && slot->state == RPC_DONE) {
        ret = translate_reply(client_ctx, &slot->reply, slot->receiver);
        if (ret) goto finish_call;
        transaction.size = slot->reply.size;
        transaction.status = slot->reply.status;
        transaction.object_count = slot->reply.object_count;
        memcpy(transaction.object_offsets, slot->reply.object_offsets, sizeof(transaction.object_offsets));
        memcpy(client_ctx->buffer, slot->reply.data, slot->reply.size);
        transaction.object_cookie = 0;
        client_ctx->pending_reply = transaction;
        client_ctx->reply_ready = true;
        client_ctx->reply_delivered = false;
    } else if (!ret) {
        ret = -EPIPE;
    }
finish_call:
    if (ret && slot->state == RPC_PROCESSING)
        WRITE_ONCE(slot->state, RPC_CANCELLED);
    else
        WRITE_ONCE(slot->state, RPC_IDLE);
    client_ctx->inflight = false;
    mutex_unlock(&state_lock);
    return ret;

unlock_state:
    mutex_unlock(&state_lock);
    return ret;
}

static long release_reply(struct file *file)
{
    struct mini_binder_file *ctx = file->private_data;
    long ret = 0;

    mutex_lock(&state_lock);
    if (!ctx->reply_ready)
        ret = -ENOENT;
    else
        ctx->reply_ready = false;
    mutex_unlock(&state_lock);
    return ret;
}

static bool pending_request(struct file *file)
{
    unsigned int i;
    for (i = 0; i < SLOT_COUNT; i++)
        if (READ_ONCE(slots[i].state) == RPC_PENDING && READ_ONCE(slots[i].receiver) == file)
            return true;
    return false;
}

static long receive_request(struct file *file, void __user *arg)
{
    long ret;
    unsigned int i;
    unsigned long user_base;

    for (;;) {
        mutex_lock(&state_lock);
        if (!((struct mini_binder_file *)file->private_data)->is_server) {
            mutex_unlock(&state_lock);
            return -EPERM;
        }
        {
            struct mini_binder_file *ctx = file->private_data;
            spin_lock(&mapping_lock);
            if (!ctx->server_vma || ctx->server_vma->vm_mm != current->mm) {
                spin_unlock(&mapping_lock);
                mutex_unlock(&state_lock);
                return -ENXIO;
            }
            user_base = ctx->server_vma->vm_start;
            spin_unlock(&mapping_lock);
        }
        for (i = 0; i < SLOT_COUNT; i++) {
            struct mini_binder_transaction_data delivered = {0};
            struct transaction_slot *slot = &slots[i];
            if (slot->state != RPC_PENDING || slot->receiver != file)
                continue;

            delivered.id = slot->request.id;
            delivered.data_address = user_base + i * MINI_BINDER_MAX_DATA;
            delivered.size = slot->request.size;
            delivered.code = slot->request.code;
            delivered.target_node = slot->request.target_node;
            delivered.object_cookie = slot->request.object_cookie;
            delivered.object_count = slot->request.object_count;
            memcpy(delivered.object_offsets, slot->request.object_offsets, sizeof(delivered.object_offsets));

            WRITE_ONCE(slot->state, RPC_PROCESSING);
            if (copy_to_user(arg, &delivered, sizeof(delivered))) {
                WRITE_ONCE(slot->state, RPC_FAILED);
                mutex_unlock(&state_lock);
                wake_up_interruptible(&client_wait);
                return -EFAULT;
            }
            mutex_unlock(&state_lock);
            return 0;
        }
        mutex_unlock(&state_lock);
        ret = wait_event_interruptible(server_wait,
                pending_request(file) || !READ_ONCE(((struct mini_binder_file *)file->private_data)->is_server) ||
                !server_mapping_valid(file->private_data, current->mm));
        if (ret)
            return ret;
    }
}

static long send_reply(struct file *file, void __user *arg)
{
    struct mini_binder_transaction_data incoming_transaction;
    __u64 id;
    __s32 status;
    long ret = 0;
    struct transaction_slot *slot = NULL;
    unsigned int i;

    if (copy_from_user(&incoming_transaction, arg, sizeof(incoming_transaction)))
        return -EFAULT;
    if (incoming_transaction.size > MINI_BINDER_MAX_DATA)
        return -EMSGSIZE;
    id = incoming_transaction.id;
    status = incoming_transaction.status;
    mutex_lock(&state_lock);
    for (i = 0; i < SLOT_COUNT; i++) {
        if (slots[i].state != RPC_IDLE && slots[i].request.id == id) {
            slot = &slots[i];
            break;
        }
    }
    if (!((struct mini_binder_file *)file->private_data)->is_server)
        ret = -EPERM;
    else if (!slot)
        ret = -ENOENT;
    else if (slot->receiver != file)
        ret = -EPERM;
    else if (slot->state == RPC_CANCELLED) {
        WRITE_ONCE(slot->state, RPC_IDLE);
        ret = -ECANCELED;
    }
    else if (slot->state != RPC_PROCESSING)
        ret = -ENOENT;
    else if (status > 0)
        ret = -EINVAL;
    else {
        slot->reply = incoming_transaction;
        WRITE_ONCE(slot->state, RPC_DONE);
    }
    mutex_unlock(&state_lock);
    if (!ret)
        wake_up_interruptible(&client_wait);
    return ret;
}

static long write_command(struct file *file, void __user *arg)
{
    struct mini_binder_command __user *write_cmd = arg;
    __u32 command;
    if (get_user(command, &write_cmd->cmd))
        return -EFAULT;
    switch (command) {
    case MINI_BC_TRANSACTION:
        return call_server(file, &write_cmd->transaction);
    case MINI_BC_REPLY:
        return send_reply(file, &write_cmd->transaction);
    case MINI_BC_FREE_REPLY:
        return release_reply(file);
    default:
        return -EINVAL;
    }
}

static long read_command(struct file *file, void __user *arg)
{
    struct mini_binder_command __user *read_cmd = arg;
    struct mini_binder_file *ctx = file->private_data;
    struct mini_binder_command reply = {.cmd = MINI_BR_REPLY};
    long ret = 0;

    if (READ_ONCE(ctx->is_server) && !READ_ONCE(ctx->reply_ready)) {
        if (put_user(MINI_BR_TRANSACTION, &read_cmd->cmd))
            return -EFAULT;
        return receive_request(file, &read_cmd->transaction);
    }
    mutex_lock(&state_lock);
    if (!ctx->reply_ready || ctx->reply_delivered) {
        ret = -ENOENT;
    } else {
        reply.transaction = ctx->pending_reply;
        if (copy_to_user(read_cmd, &reply, sizeof(reply)))
            ret = -EFAULT;
        else
            ctx->reply_delivered = true;
    }
    mutex_unlock(&state_lock);
    return ret;
}

static long mini_binder_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    __u32 version = MINI_BINDER_PROTOCOL_VERSION;
    void __user *user_arg = (void __user *)arg;

    switch (cmd) {
    case MINI_BINDER_GET_VERSION:
        return copy_to_user(user_arg, &version, sizeof(version)) ? -EFAULT : 0;
    case MINI_BINDER_REGISTER_SERVER:
        return register_server(file);
    case MINI_BINDER_CREATE_NODE:
        return create_node(file, user_arg);
    case MINI_BINDER_WRITE:
        return write_command(file, user_arg);
    case MINI_BINDER_READ:
        return read_command(file, user_arg);
    default:
        return -ENOTTY;
    }
}

static int mini_binder_release(struct inode *inode, struct file *file)
{
    struct mini_binder_file *ctx = file->private_data;
    bool wake = false;
    unsigned int i;

    (void)inode;
    mutex_lock(&state_lock);
    if (ctx->is_server) {
        if (server_file == file) WRITE_ONCE(server_file, NULL);
        for (i = 0; i < ARRAY_SIZE(nodes); i++)
            if (nodes[i].owner == file) memset(&nodes[i], 0, sizeof(nodes[i]));
        for (i = 0; i < SLOT_COUNT; i++) {
            if (slots[i].receiver != file) continue;
            if (slots[i].state == RPC_PENDING || slots[i].state == RPC_PROCESSING || slots[i].state == RPC_DONE) {
                WRITE_ONCE(slots[i].state, RPC_FAILED);
                wake = true;
            } else if (slots[i].state == RPC_CANCELLED) {
                WRITE_ONCE(slots[i].state, RPC_IDLE);
            }
        }
    }
    mutex_unlock(&state_lock);
    if (wake)
        wake_up_interruptible(&client_wait);
    vfree(ctx->buffer);
    kfree(ctx);
    return 0;
}

static const struct file_operations mini_binder_fops = {
    .owner = THIS_MODULE,
    .open = mini_binder_open,
    .release = mini_binder_release,
    .mmap = mini_binder_mmap,
    .unlocked_ioctl = mini_binder_ioctl,
    .llseek = no_llseek,
};

static struct miscdevice mini_binder_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "mini_binder",
    .fops = &mini_binder_fops,
    .mode = 0666,
};

static int __init mini_binder_init(void)
{
    int ret = misc_register(&mini_binder_device);

    if (ret)
        return ret;
    pr_info("mini_binder: registered\n");
    return 0;
}

static void __exit mini_binder_exit(void)
{
    misc_deregister(&mini_binder_device);
    pr_info("mini_binder: unregistered\n");
}

module_init(mini_binder_init);
module_exit(mini_binder_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BinderABC synchronous RPC device");
