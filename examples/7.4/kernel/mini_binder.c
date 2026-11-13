// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <asm/unaligned.h>

#include <mini_binder.h>

enum rpc_state { RPC_IDLE, RPC_PENDING, RPC_PROCESSING, RPC_DONE, RPC_FAILED, RPC_CANCELLED, RPC_ASYNC_QUEUED };

struct mini_binder_ref {
    __u64 node_id;
};

struct mini_binder_fd_set {
    struct {
        struct file *file;
        __u32 offset;
    } entries[MINI_BINDER_MAX_OBJECTS];
    unsigned int count;
};

struct mini_binder_thread {
    struct transaction_slot *current_transaction;
    struct pid *pid;
    __u32 id;
    struct list_head todo;
    atomic_t todo_count;
    wait_queue_head_t wait;
    bool looper;
    bool inflight;
    bool reply_ready;
    bool reply_delivered;
    unsigned int reply_index;
    struct mini_binder_fd_set reply_fds;
    struct mini_binder_transaction_data pending_reply;
};

struct mini_binder_proc {
    struct file *file;
    void *buffer;
    struct vm_area_struct *mapping_vma;
    unsigned long reply_used;
    struct mini_binder_ref refs[MINI_BINDER_MAX_OBJECTS];
    struct mini_binder_thread threads[8];
    struct list_head todo;
    atomic_t pending_count;
};

struct transaction_slot {
    struct mini_binder_node *async_node;
    struct list_head work;
    struct list_head reply_work;
    struct file *receiver;
    struct mini_binder_thread *caller;
    struct mini_binder_proc *caller_proc;
    struct mini_binder_fd_set request_fds;
    struct mini_binder_fd_set reply_fds;
    enum rpc_state state;
    struct mini_binder_transaction_data request;
    struct mini_binder_transaction_data reply;
};

struct mini_binder_node {
    bool async_active;
    struct list_head async_todo;
    __u64 id;
    __u64 cookie;
    struct mini_binder_proc *owner;
};

#define SLOT_COUNT 2
static DEFINE_MUTEX(state_lock);
static DEFINE_SPINLOCK(mapping_lock);
static DECLARE_WAIT_QUEUE_HEAD(server_wait);
static struct file *server_file;
static struct transaction_slot slots[SLOT_COUNT];
static struct mini_binder_node nodes[8];
static __u64 next_node_id;

static long get_thread_id(struct mini_binder_proc *proc, __u32 *id)
{
    struct pid *pid = task_pid(current);
    unsigned int i, empty = ARRAY_SIZE(proc->threads);
    long ret = 0;

    mutex_lock(&state_lock);
    for (i = 0; i < ARRAY_SIZE(proc->threads); i++) {
        if (proc->threads[i].pid == pid) {
            *id = proc->threads[i].id;
            goto out;
        }
        if (!proc->threads[i].pid && empty == ARRAY_SIZE(proc->threads)) empty = i;
    }
    if (empty == ARRAY_SIZE(proc->threads)) {
        ret = -ENOSPC;
    } else {
        proc->threads[empty].pid = get_pid(pid);
        proc->threads[empty].id = empty + 1;
        INIT_LIST_HEAD(&proc->threads[empty].todo);
        atomic_set(&proc->threads[empty].todo_count, 0);
        init_waitqueue_head(&proc->threads[empty].wait);
        *id = empty + 1;
    }
out:
    mutex_unlock(&state_lock);
    return ret;
}

static void drop_fd_set(struct mini_binder_fd_set *fds)
{
    unsigned int i;
    for (i = 0; i < fds->count; i++) fput(fds->entries[i].file);
    fds->count = 0;
}

/* state_lock protects Node ordering, both queues and slot lifetime. */
static void queue_request_locked(struct transaction_slot *slot)
{
    struct mini_binder_proc *receiver = slot->receiver->private_data;
    WRITE_ONCE(slot->state, RPC_PENDING);
    list_add_tail(&slot->work, &receiver->todo);
    atomic_inc(&receiver->pending_count);
}

static void finish_oneway_locked(struct transaction_slot *slot)
{
    struct mini_binder_node *node = slot->async_node;
    bool active = slot->state != RPC_ASYNC_QUEUED;
    if (slot->state == RPC_PENDING) {
        struct mini_binder_proc *receiver = slot->receiver->private_data;
        atomic_dec(&receiver->pending_count);
    }
    if (slot->state == RPC_PENDING || slot->state == RPC_ASYNC_QUEUED)
        list_del_init(&slot->work);
    drop_fd_set(&slot->request_fds);
    slot->async_node = NULL;
    slot->receiver = NULL;
    WRITE_ONCE(slot->state, RPC_IDLE);
    if (active) {
        node->async_active = false;
        if (!list_empty(&node->async_todo)) {
            struct transaction_slot *next = list_first_entry(&node->async_todo,
                                                              struct transaction_slot, work);
            list_del_init(&next->work);
            node->async_active = true;
            queue_request_locked(next);
        }
    }
    wake_up_interruptible_all(&server_wait);
}

static long release_request(struct file *file, struct mini_binder_thread *thread)
{
    struct transaction_slot *slot;
    long ret = 0;
    mutex_lock(&state_lock);
    slot = thread->current_transaction;
    if (!slot) ret = -ENOENT;
    else if (slot->receiver != file || !slot->async_node) ret = -EINVAL;
    else {
        thread->current_transaction = NULL;
        finish_oneway_locked(slot);
    }
    mutex_unlock(&state_lock);
    return ret;
}

static long mini_binder_thread_exit(struct mini_binder_proc *proc)
{
    struct pid *pid = NULL;
    unsigned int i;
    long ret = -ENOENT;

    mutex_lock(&state_lock);
    for (i = 0; i < ARRAY_SIZE(proc->threads); i++) {
        struct mini_binder_thread *thread = &proc->threads[i];
        if (thread->pid != task_pid(current)) continue;
        if (thread->inflight || !list_empty(&thread->todo)) {
            ret = -EBUSY;
            break;
        }
        if (thread->current_transaction) {
            struct transaction_slot *slot = thread->current_transaction;
            if (slot->async_node) {
                finish_oneway_locked(slot);
            } else if (slot->state == RPC_CANCELLED) {
                WRITE_ONCE(slot->state, RPC_IDLE);
            } else if (slot->state == RPC_PROCESSING) {
                WRITE_ONCE(slot->state, RPC_FAILED);
                wake_up_interruptible(&slot->caller->wait);
            }
            thread->current_transaction = NULL;
        }
        if (thread->reply_ready) {
            proc->reply_used &= ~(1UL << thread->reply_index);
            drop_fd_set(&thread->reply_fds);
            WRITE_ONCE(thread->reply_ready, false);
        }
        thread->reply_delivered = false;
        thread->looper = false;
        pid = thread->pid;
        thread->pid = NULL;
        ret = 0;
        break;
    }
    mutex_unlock(&state_lock);
    put_pid(pid);
    return ret;
}

/* Run in the sender's context and keep references until the receiver reads. */
static long translate_objects(struct file *sender_file,
                              struct mini_binder_proc *receiver,
                              struct mini_binder_transaction_data *transaction,
                              struct mini_binder_fd_set *fds)
{
    struct mini_binder_proc *sender = sender_file->private_data;
    struct mini_binder_ref refs[MINI_BINDER_MAX_OBJECTS];
    unsigned long created = 0;
    __u32 previous = 0;
    unsigned int i, j, k;
    long ret = 0;

    if (transaction->object_count > MINI_BINDER_MAX_OBJECTS) return -EINVAL;
    memcpy(refs, receiver->refs, sizeof(refs));
    for (i = 0; i < transaction->object_count; i++) {
        __u32 offset = transaction->object_offsets[i], handle, type;
        __u64 id;
        if ((offset & 3) || offset > transaction->size || transaction->size - offset < 12 ||
            (i && (offset < previous || offset - previous < 12))) { ret = -EINVAL; goto fail; }
        type = get_unaligned_le32(transaction->data + offset);
        if (type == MINI_BINDER_OBJECT_FD) {
            struct file *fd_file;
            if (get_unaligned_le32(transaction->data + offset + 8)) { ret = -EINVAL; goto fail; }
            fd_file = fget(get_unaligned_le32(transaction->data + offset + 4));
            if (!fd_file) { ret = -EBADF; goto fail; }
            fds->entries[fds->count].file = fd_file;
            fds->entries[fds->count].offset = offset + 4;
            fds->count++;
            previous = offset;
            continue;
        }
        if (type == MINI_BINDER_OBJECT_HANDLE) {
            if (get_unaligned_le32(transaction->data + offset + 8)) { ret = -EINVAL; goto fail; }
            handle = get_unaligned_le32(transaction->data + offset + 4);
            if (!handle || handle > ARRAY_SIZE(sender->refs) || !sender->refs[handle - 1].node_id) { ret = -ENOENT; goto fail; }
            id = sender->refs[handle - 1].node_id;
            for (j = 0; j < ARRAY_SIZE(nodes); j++)
                if (nodes[j].owner && nodes[j].id == id) break;
        } else if (type == MINI_BINDER_OBJECT_LOCAL) {
            __u64 cookie = get_unaligned_le32(transaction->data + offset + 4);
            cookie |= (__u64)get_unaligned_le32(transaction->data + offset + 8) << 32;
            for (j = 0; j < ARRAY_SIZE(nodes); j++)
                if (nodes[j].owner == sender && nodes[j].cookie == cookie) break;
            if (!cookie) { ret = -EINVAL; goto fail; }
            if (j == ARRAY_SIZE(nodes)) {
                for (j = 0; j < ARRAY_SIZE(nodes); j++)
                    if (!nodes[j].owner) break;
                if (j == ARRAY_SIZE(nodes)) { ret = -ENOSPC; goto fail; }
                if (next_node_id == U64_MAX) { ret = -EOVERFLOW; goto fail; }
                INIT_LIST_HEAD(&nodes[j].async_todo);
                nodes[j].id = ++next_node_id;
                nodes[j].cookie = cookie;
                nodes[j].owner = sender;
                created |= 1UL << j;
            }
            id = nodes[j].id;
        } else { ret = -EINVAL; goto fail; }
        if (j == ARRAY_SIZE(nodes)) { ret = -ENOENT; goto fail; }
        if (nodes[j].owner == receiver) {
            __u64 cookie = nodes[j].cookie;
            put_unaligned_le32(MINI_BINDER_OBJECT_LOCAL, transaction->data + offset);
            put_unaligned_le32((__u32)cookie, transaction->data + offset + 4);
            put_unaligned_le32((__u32)(cookie >> 32), transaction->data + offset + 8);
            previous = offset;
            continue;
        }
        for (k = 0; k < ARRAY_SIZE(refs); k++) if (refs[k].node_id == id) break;
        if (k == ARRAY_SIZE(refs)) {
            for (k = 0; k < ARRAY_SIZE(refs); k++) if (!refs[k].node_id) break;
            if (k == ARRAY_SIZE(refs)) { ret = -ENOSPC; goto fail; }
            refs[k].node_id = id;
        }
        put_unaligned_le32(MINI_BINDER_OBJECT_HANDLE, transaction->data + offset);
        put_unaligned_le32(k + 1, transaction->data + offset + 4);
        put_unaligned_le32(0, transaction->data + offset + 8);
        previous = offset;
    }
    memcpy(receiver->refs, refs, sizeof(refs));
    return 0;
fail:
    for (j = 0; j < ARRAY_SIZE(nodes); j++)
        if (created & (1UL << j)) memset(&nodes[j], 0, sizeof(nodes[j]));
    drop_fd_set(fds);
    return ret;
}

static long reserve_fds(struct mini_binder_fd_set *fds, void *buffer, int *reserved)
{
    unsigned int i;
    for (i = 0; i < fds->count; i++) {
        reserved[i] = get_unused_fd_flags(O_CLOEXEC);
        if (reserved[i] < 0) {
            long ret = reserved[i];
            while (i) put_unused_fd(reserved[--i]);
            return ret;
        }
        put_unaligned_le32(reserved[i], (char *)buffer + fds->entries[i].offset);
    }
    return 0;
}

static void release_reserved_fds(struct mini_binder_fd_set *fds, const int *reserved)
{
    unsigned int i;
    for (i = 0; i < fds->count; i++) put_unused_fd(reserved[i]);
}

static void install_fds(struct mini_binder_fd_set *fds, const int *reserved)
{
    unsigned int i;
    for (i = 0; i < fds->count; i++) fd_install(reserved[i], fds->entries[i].file);
    fds->count = 0;
}

static int mini_binder_open(struct inode *inode, struct file *file)
{
    struct mini_binder_proc *ctx;

    (void)inode;
    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;
    ctx->file = file;
    INIT_LIST_HEAD(&ctx->todo);
    atomic_set(&ctx->pending_count, 0);
    file->private_data = ctx;
    return 0;
}

static void mapping_vma_close(struct vm_area_struct *vma)
{
    struct mini_binder_proc *ctx = vma->vm_private_data;
    bool cleared = false;
    spin_lock(&mapping_lock);
    if (ctx->mapping_vma == vma) {
        ctx->mapping_vma = NULL;
        cleared = true;
    }
    spin_unlock(&mapping_lock);
    if (cleared)
        wake_up_interruptible_all(&server_wait);
}

static bool mapping_valid(struct mini_binder_proc *ctx, struct mm_struct *mm)
{
    bool valid;
    spin_lock(&mapping_lock);
    valid = ctx->mapping_vma && ctx->mapping_vma->vm_mm == mm;
    spin_unlock(&mapping_lock);
    return valid;
}

static int mapping_vma_mremap(struct vm_area_struct *vma)
{
    (void)vma;
    return -EINVAL;
}

static const struct vm_operations_struct mapping_vm_ops = {
    .close = mapping_vma_close,
    .mremap = mapping_vma_mremap,
};

static int mini_binder_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct mini_binder_proc *ctx = file->private_data;
    int ret;

    if (vma->vm_pgoff || vma->vm_end - vma->vm_start != PAGE_SIZE ||
        !(vma->vm_flags & VM_MAYSHARE) || (vma->vm_flags & (VM_WRITE | VM_EXEC)))
        return -EINVAL;
    mutex_lock(&state_lock);
    {
        bool mapped;
        spin_lock(&mapping_lock);
        mapped = ctx->mapping_vma != NULL;
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
        if (!ret) {
            vm_flags_set(vma, VM_DONTCOPY);
            vma->vm_ops = &mapping_vm_ops;
            vma->vm_private_data = ctx;
            spin_lock(&mapping_lock);
            ctx->mapping_vma = vma;
            spin_unlock(&mapping_lock);
        }
    }
    mutex_unlock(&state_lock);
    return ret;
}

static long call_server(struct file *file, struct mini_binder_thread *thread, void __user *arg)
{
    struct mini_binder_proc *client_ctx = file->private_data;
    struct mini_binder_transaction_data transaction;
    struct mini_binder_proc *server_ctx;
    struct file *receiver;
    struct transaction_slot *slot = NULL;
    struct mini_binder_node *target = NULL;
    bool oneway;
    unsigned int i, reply_index = 0;
    long ret;

    if (copy_from_user(&transaction, arg, sizeof(transaction)))
        return -EFAULT;
    if (transaction.size > MINI_BINDER_MAX_DATA)
        return -EMSGSIZE;
    if (transaction.flags & ~MINI_TF_ONE_WAY) return -EINVAL;
    oneway = transaction.flags & MINI_TF_ONE_WAY;
    /* The management endpoint remains a synchronous teaching control channel. */
    if (oneway && !transaction.target_handle) return -EINVAL;
    mutex_lock(&state_lock);
    if (!client_ctx->buffer || !mapping_valid(client_ctx, current->mm)) {
        ret = -ENXIO;
        goto unlock_state;
    }
    if (thread->reply_ready || thread->inflight) {
        ret = -EBUSY;
        goto unlock_state;
    }
    if (!oneway) {
        for (reply_index = 0; reply_index < SLOT_COUNT; reply_index++)
            if (!(client_ctx->reply_used & (1UL << reply_index))) break;
        if (reply_index == SLOT_COUNT) { ret = -EBUSY; goto unlock_state; }
    }
    if (transaction.target_handle) {
        __u64 node_id;
        if (transaction.target_handle > ARRAY_SIZE(client_ctx->refs) ||
            !client_ctx->refs[transaction.target_handle - 1].node_id) { ret = -ENOENT; goto unlock_state; }
        node_id = client_ctx->refs[transaction.target_handle - 1].node_id;
        for (i = 0; i < ARRAY_SIZE(nodes); i++)
            if (nodes[i].owner && nodes[i].id == node_id) break;
        if (i == ARRAY_SIZE(nodes)) { ret = -ENOENT; goto unlock_state; }
        target = &nodes[i];
        transaction.object_cookie = nodes[i].cookie;
        receiver = nodes[i].owner->file;
    } else {
        transaction.object_cookie = 0;
        receiver = server_file;
    }
    if (!receiver) { ret = -ENOTCONN; goto unlock_state; }
    if (receiver == file) { ret = -EINVAL; goto unlock_state; }
    server_ctx = receiver->private_data;
    spin_lock(&mapping_lock);
    if (!server_ctx->mapping_vma) {
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
    ret = translate_objects(file, server_ctx, &transaction, &slot->request_fds);
    if (ret) goto unlock_state;
    memcpy((char *)server_ctx->buffer + i * MINI_BINDER_MAX_DATA, transaction.data, transaction.size);
    slot->request.size = transaction.size;
    slot->request.code = transaction.code;
    slot->request.flags = transaction.flags;
    slot->request.object_cookie = transaction.object_cookie;
    slot->request.object_count = transaction.object_count;
    memcpy(slot->request.object_offsets, transaction.object_offsets, sizeof(transaction.object_offsets));
    WRITE_ONCE(slot->receiver, receiver);
    slot->async_node = oneway ? target : NULL;
    if (oneway) {
        /* Accepted work must not retain pointers into the sending thread/proc. */
        slot->caller = NULL;
        slot->caller_proc = NULL;
        if (target->async_active) {
            WRITE_ONCE(slot->state, RPC_ASYNC_QUEUED);
            list_add_tail(&slot->work, &target->async_todo);
        } else {
            target->async_active = true;
            queue_request_locked(slot);
        }
    } else {
        slot->caller = thread;
        slot->caller_proc = client_ctx;
        client_ctx->reply_used |= 1UL << reply_index;
        thread->reply_index = reply_index;
        thread->inflight = true;
        WRITE_ONCE(slot->state, RPC_PENDING);
        list_add_tail(&slot->work, &server_ctx->todo);
        atomic_inc(&server_ctx->pending_count);
    }
    mutex_unlock(&state_lock);
    wake_up_interruptible_all(&server_wait);

    if (oneway) return 0; // Submission is complete; no business reply will arrive.

    ret = wait_event_interruptible(thread->wait,
            atomic_read(&thread->todo_count) || READ_ONCE(slot->state) == RPC_FAILED);
    mutex_lock(&state_lock);
    if (slot->state == RPC_DONE) {
        list_del_init(&slot->reply_work);
        atomic_dec(&thread->todo_count);
    }
    if (!ret && slot->state == RPC_DONE) {
        transaction.size = slot->reply.size;
        transaction.status = slot->reply.status;
        transaction.object_count = slot->reply.object_count;
        memcpy(transaction.object_offsets, slot->reply.object_offsets, sizeof(transaction.object_offsets));
        transaction.reply_offset = (thread->reply_index + SLOT_COUNT) * MINI_BINDER_MAX_DATA;
        memcpy((char *)client_ctx->buffer + transaction.reply_offset, slot->reply.data, slot->reply.size);
        transaction.object_cookie = 0;
        thread->reply_fds = slot->reply_fds;
        slot->reply_fds.count = 0;
        thread->pending_reply = transaction;
        WRITE_ONCE(thread->reply_ready, true);
        thread->reply_delivered = false;
    } else if (!ret) {
        ret = -EPIPE;
    }
    if (slot->state == RPC_PENDING) {
        struct mini_binder_proc *receiver_ctx = slot->receiver->private_data;
        list_del_init(&slot->work);
        atomic_dec(&receiver_ctx->pending_count);
        drop_fd_set(&slot->request_fds);
    }
    if (ret && slot->state == RPC_PROCESSING)
        WRITE_ONCE(slot->state, RPC_CANCELLED);
    else
        WRITE_ONCE(slot->state, RPC_IDLE);
    if (!thread->reply_ready) {
        drop_fd_set(&slot->reply_fds);
        client_ctx->reply_used &= ~(1UL << thread->reply_index);
    }
    thread->inflight = false;
    mutex_unlock(&state_lock);
    return ret;

unlock_state:
    mutex_unlock(&state_lock);
    return ret;
}

static long release_reply(struct file *file, struct mini_binder_thread *thread)
{
    struct mini_binder_proc *ctx = file->private_data;
    long ret = 0;

    mutex_lock(&state_lock);
    if (!thread->reply_ready)
        ret = -ENOENT;
    else {
        ctx->reply_used &= ~(1UL << thread->reply_index);
        drop_fd_set(&thread->reply_fds);
        WRITE_ONCE(thread->reply_ready, false);
    }
    mutex_unlock(&state_lock);
    return ret;
}

static long receive_request(struct file *file, struct mini_binder_thread *thread, void __user *arg)
{
    long ret;
    unsigned int i;
    unsigned long user_base;
    struct mini_binder_proc *ctx = file->private_data;

    mutex_lock(&state_lock);
    if (thread->current_transaction) {
        mutex_unlock(&state_lock);
        return -EBUSY;
    }
    {
        spin_lock(&mapping_lock);
        if (!ctx->mapping_vma || ctx->mapping_vma->vm_mm != current->mm) {
            spin_unlock(&mapping_lock);
            mutex_unlock(&state_lock);
            return -ENXIO;
        }
        user_base = ctx->mapping_vma->vm_start;
        spin_unlock(&mapping_lock);
    }
    if (!list_empty(&ctx->todo)) {
        struct mini_binder_command delivered = {.cmd = MINI_BR_TRANSACTION};
        struct transaction_slot *slot = list_first_entry(&ctx->todo, struct transaction_slot, work);
        int reserved[MINI_BINDER_MAX_OBJECTS];
        i = slot - slots;
        list_del_init(&slot->work);
        atomic_dec(&ctx->pending_count);
        WRITE_ONCE(slot->state, RPC_PROCESSING);

        delivered.transaction.data_address = user_base + i * MINI_BINDER_MAX_DATA;
        delivered.transaction.size = slot->request.size;
        delivered.transaction.code = slot->request.code;
        delivered.transaction.flags = slot->request.flags;
        delivered.transaction.object_cookie = slot->request.object_cookie;
        delivered.transaction.object_count = slot->request.object_count;
        memcpy(delivered.transaction.object_offsets, slot->request.object_offsets, sizeof(delivered.transaction.object_offsets));

        ret = reserve_fds(&slot->request_fds, (char *)ctx->buffer + i * MINI_BINDER_MAX_DATA, reserved);
        if (ret) {
            drop_fd_set(&slot->request_fds);
            if (slot->async_node) finish_oneway_locked(slot);
            else WRITE_ONCE(slot->state, RPC_FAILED);
            if (slot->caller) wake_up_interruptible(&slot->caller->wait);
            mutex_unlock(&state_lock);
            return ret;
        }
        WRITE_ONCE(slot->state, RPC_PROCESSING);
        if (copy_to_user(arg, &delivered, sizeof(delivered))) {
            release_reserved_fds(&slot->request_fds, reserved);
            drop_fd_set(&slot->request_fds);
            if (slot->async_node) finish_oneway_locked(slot);
            else WRITE_ONCE(slot->state, RPC_FAILED);
            if (slot->caller) wake_up_interruptible(&slot->caller->wait);
            mutex_unlock(&state_lock);
            return -EFAULT;
        }
        install_fds(&slot->request_fds, reserved);
        thread->current_transaction = slot;
        mutex_unlock(&state_lock);
        return 0;
    }
    mutex_unlock(&state_lock);
    return -EAGAIN;
}

static long send_reply(struct file *file, struct mini_binder_thread *thread, void __user *arg)
{
    struct mini_binder_transaction_data incoming_transaction;
    __s32 status;
    long ret = 0;
    struct transaction_slot *slot = NULL;

    if (copy_from_user(&incoming_transaction, arg, sizeof(incoming_transaction)))
        return -EFAULT;
    if (incoming_transaction.size > MINI_BINDER_MAX_DATA)
        return -EMSGSIZE;
    status = incoming_transaction.status;
    mutex_lock(&state_lock);
    slot = thread->current_transaction;
    if (!slot)
        ret = -ENOENT;
    else if (slot->receiver != file)
        ret = -EPERM;
    else if (slot->async_node)
        ret = -EINVAL;
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
        ret = translate_objects(file, slot->caller_proc, &slot->reply, &slot->reply_fds);
        if (!ret) {
            list_add_tail(&slot->reply_work, &slot->caller->todo);
            atomic_inc(&slot->caller->todo_count);
            WRITE_ONCE(slot->state, RPC_DONE);
            wake_up_interruptible(&slot->caller->wait);
        }
    }
    if (!ret || ret == -ECANCELED)
        thread->current_transaction = NULL;
    mutex_unlock(&state_lock);
    return ret;
}

static long write_command(struct file *file, struct mini_binder_thread *thread, void __user *arg)
{
    struct mini_binder_command __user *write_cmd = arg;
    __u32 command;
    if (get_user(command, &write_cmd->cmd))
        return -EFAULT;
    switch (command) {
    case MINI_BC_TRANSACTION:
        return call_server(file, thread, &write_cmd->transaction);
    case MINI_BC_REPLY:
        return send_reply(file, thread, &write_cmd->transaction);
    case MINI_BC_FREE_REPLY:
        return release_reply(file, thread);
    case MINI_BC_FREE_BUFFER:
        return release_request(file, thread);
    case MINI_BC_ENTER_LOOPER:
    case MINI_BC_EXIT_LOOPER: {
        long ret = 0;
        mutex_lock(&state_lock);
        if (command == MINI_BC_ENTER_LOOPER) {
            if (thread->looper) ret = -EALREADY;
            else thread->looper = true;
        } else {
            if (!thread->looper) ret = -EINVAL;
            else thread->looper = false;
        }
        mutex_unlock(&state_lock);
        return ret;
    }
    default:
        return -EINVAL;
    }
}

static long read_command(struct file *file, struct mini_binder_thread *thread, void __user *arg)
{
    struct mini_binder_proc *ctx = file->private_data;
    struct mini_binder_command reply = {.cmd = MINI_BR_REPLY};
    int reserved[MINI_BINDER_MAX_OBJECTS];
    long ret;
    bool looper;

    for (;;) {
        mutex_lock(&state_lock);
        if (!mapping_valid(ctx, current->mm)) {
            mutex_unlock(&state_lock);
            return -ENXIO;
        }
        if (thread->reply_ready && !thread->reply_delivered) {
            reply.transaction = thread->pending_reply;
            ret = reserve_fds(&thread->reply_fds,
                              (char *)ctx->buffer + reply.transaction.reply_offset, reserved);
            if (!ret && copy_to_user(arg, &reply, sizeof(reply))) {
                release_reserved_fds(&thread->reply_fds, reserved);
                ret = -EFAULT;
            } else if (!ret) {
                install_fds(&thread->reply_fds, reserved);
                thread->reply_delivered = true;
            }
            mutex_unlock(&state_lock);
            return ret;
        }
        looper = thread->looper;
        mutex_unlock(&state_lock);
        if (looper) {
            ret = receive_request(file, thread, arg);
            if (ret != -EAGAIN) return ret;
        }
        ret = wait_event_interruptible(server_wait,
                (READ_ONCE(thread->reply_ready) && !READ_ONCE(thread->reply_delivered)) ||
                (looper && atomic_read(&ctx->pending_count)) || !mapping_valid(ctx, current->mm));
        if (ret) return ret;
    }
}

static long mini_binder_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    __u32 version = MINI_BINDER_PROTOCOL_VERSION;
    __u32 thread_id;
    long ret;
    struct mini_binder_thread *thread = NULL;
    void __user *user_arg = (void __user *)arg;

    if (cmd == MINI_BINDER_THREAD_EXIT)
        return mini_binder_thread_exit(file->private_data);
    if (cmd == MINI_BINDER_WRITE || cmd == MINI_BINDER_READ || cmd == MINI_BINDER_GET_THREAD_ID) {
        ret = get_thread_id(file->private_data, &thread_id);
        if (ret) return ret;
        if (cmd == MINI_BINDER_GET_THREAD_ID)
            return copy_to_user(user_arg, &thread_id, sizeof(thread_id)) ? -EFAULT : 0;
        thread = &((struct mini_binder_proc *)file->private_data)->threads[thread_id - 1];
    }
    switch (cmd) {
    case MINI_BINDER_GET_VERSION:
        return copy_to_user(user_arg, &version, sizeof(version)) ? -EFAULT : 0;
    case MINI_BINDER_REGISTER_MANAGER: {
        long ret = 0;
        mutex_lock(&state_lock);
        if (server_file) ret = -EBUSY;
        else WRITE_ONCE(server_file, file);
        mutex_unlock(&state_lock);
        return ret;
    }
    case MINI_BINDER_WRITE:
        return write_command(file, thread, user_arg);
    case MINI_BINDER_READ:
        return read_command(file, thread, user_arg);
    default:
        return -ENOTTY;
    }
}

static int mini_binder_release(struct inode *inode, struct file *file)
{
    struct mini_binder_proc *ctx = file->private_data;
    unsigned int i;

    (void)inode;
    mutex_lock(&state_lock);
    {
        for (i = 0; i < SLOT_COUNT; i++) {
            if (slots[i].receiver == file && slots[i].async_node)
                finish_oneway_locked(&slots[i]);
        }
        if (server_file == file) WRITE_ONCE(server_file, NULL);
        for (i = 0; i < ARRAY_SIZE(nodes); i++)
            if (nodes[i].owner == ctx) memset(&nodes[i], 0, sizeof(nodes[i]));
        for (i = 0; i < SLOT_COUNT; i++) {
            if (slots[i].receiver != file) continue;
            if (slots[i].state == RPC_PENDING) {
                list_del_init(&slots[i].work);
                atomic_dec(&ctx->pending_count);
                drop_fd_set(&slots[i].request_fds);
            }
            if (slots[i].state == RPC_DONE) {
                list_del_init(&slots[i].reply_work);
                atomic_dec(&slots[i].caller->todo_count);
                drop_fd_set(&slots[i].reply_fds);
            }
            if (slots[i].state == RPC_PENDING || slots[i].state == RPC_PROCESSING || slots[i].state == RPC_DONE) {
                WRITE_ONCE(slots[i].state, RPC_FAILED);
                wake_up_interruptible(&slots[i].caller->wait);
            } else if (slots[i].state == RPC_CANCELLED) {
                WRITE_ONCE(slots[i].state, RPC_IDLE);
            }
        }
    }
    mutex_unlock(&state_lock);
    for (i = 0; i < ARRAY_SIZE(ctx->threads); i++) {
        drop_fd_set(&ctx->threads[i].reply_fds);
        put_pid(ctx->threads[i].pid);
    }
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
