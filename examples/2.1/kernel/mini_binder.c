// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include <mini_binder.h>

enum rpc_state { RPC_IDLE, RPC_PENDING, RPC_PROCESSING, RPC_DONE, RPC_FAILED, RPC_CANCELLED };

struct mini_binder_file {
    void *buffer;
};

static DEFINE_MUTEX(state_lock);
static DEFINE_MUTEX(call_lock);
static DECLARE_WAIT_QUEUE_HEAD(server_wait);
static DECLARE_WAIT_QUEUE_HEAD(client_wait);
static DECLARE_WAIT_QUEUE_HEAD(slot_wait);
static struct file *server_file;
static enum rpc_state state = RPC_IDLE;
static struct mini_binder_request request;
static struct mini_binder_reply reply;
static __u64 next_id;

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
    if (server_file)
        ret = -EBUSY;
    else {
        ctx->buffer = buffer;
        WRITE_ONCE(server_file, file);
    }
    mutex_unlock(&state_lock);
    if (ret)
        vfree(buffer);
    return ret;
}

static int mini_binder_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct mini_binder_file *ctx = file->private_data;
    int ret;

    if (vma->vm_pgoff || vma->vm_end - vma->vm_start != PAGE_SIZE ||
        !(vma->vm_flags & VM_MAYSHARE) || (vma->vm_flags & (VM_WRITE | VM_EXEC)))
        return -EINVAL;
    mutex_lock(&state_lock);
    if (server_file != file || !ctx->buffer) {
        ret = -EPERM;
    } else {
        vm_flags_clear(vma, VM_MAYWRITE | VM_MAYEXEC);
        vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
        ret = remap_vmalloc_range(vma, ctx->buffer, 0);
    }
    mutex_unlock(&state_lock);
    return ret;
}

static long call_server(struct file *file, void __user *arg)
{
    struct mini_binder_call call;
    struct mini_binder_payload payload;
    struct mini_binder_file *server_ctx;
    long ret;

    if (copy_from_user(&call, arg, sizeof(call)))
        return -EFAULT;
    ret = mutex_lock_interruptible(&call_lock);
    if (ret)
        return ret;
    ret = wait_event_interruptible(slot_wait,
            READ_ONCE(state) == RPC_IDLE || !READ_ONCE(server_file));
    if (ret) {
        mutex_unlock(&call_lock);
        return ret;
    }
    mutex_lock(&state_lock);
    if (!server_file) {
        ret = -ENOTCONN;
        goto unlock_state;
    }
    if (server_file == file) {
        ret = -EINVAL;
        goto unlock_state;
    }
    if (next_id == U64_MAX) {
        ret = -EOVERFLOW;
        goto unlock_state;
    }
    server_ctx = server_file->private_data;
    payload.a = call.a;
    payload.b = call.b;
    memcpy(server_ctx->buffer, &payload, sizeof(payload));
    request.id = ++next_id;
    request.size = sizeof(payload);
    WRITE_ONCE(state, RPC_PENDING);
    mutex_unlock(&state_lock);
    wake_up_interruptible(&server_wait);

    ret = wait_event_interruptible(client_wait,
            READ_ONCE(state) == RPC_DONE || READ_ONCE(state) == RPC_FAILED);
    mutex_lock(&state_lock);
    if (!ret && state == RPC_DONE) {
        call.result = reply.result;
        call.status = reply.status;
    } else if (!ret) {
        ret = -EPIPE;
    }
    if (ret && state == RPC_PROCESSING)
        WRITE_ONCE(state, RPC_CANCELLED);
    else
        WRITE_ONCE(state, RPC_IDLE);
    mutex_unlock(&state_lock);
    wake_up_interruptible(&slot_wait);
    if (!ret && copy_to_user(arg, &call, sizeof(call)))
        ret = -EFAULT;
    mutex_unlock(&call_lock);
    return ret;

unlock_state:
    mutex_unlock(&state_lock);
    mutex_unlock(&call_lock);
    return ret;
}

static long receive_request(struct file *file, void __user *arg)
{
    long ret;

    for (;;) {
        mutex_lock(&state_lock);
        if (server_file != file) {
            mutex_unlock(&state_lock);
            return -EPERM;
        }
        if (state == RPC_PENDING) {
            WRITE_ONCE(state, RPC_PROCESSING);
            if (copy_to_user(arg, &request, sizeof(request))) {
                WRITE_ONCE(state, RPC_FAILED);
                mutex_unlock(&state_lock);
                wake_up_interruptible(&client_wait);
                return -EFAULT;
            }
            mutex_unlock(&state_lock);
            return 0;
        }
        mutex_unlock(&state_lock);
        ret = wait_event_interruptible(server_wait,
                READ_ONCE(state) == RPC_PENDING || READ_ONCE(server_file) != file);
        if (ret)
            return ret;
    }
}

static long send_reply(struct file *file, void __user *arg)
{
    struct mini_binder_reply incoming;
    long ret = 0;
    bool release_slot = false;

    if (copy_from_user(&incoming, arg, sizeof(incoming)))
        return -EFAULT;
    mutex_lock(&state_lock);
    if (server_file != file)
        ret = -EPERM;
    else if (state == RPC_CANCELLED && incoming.id == request.id) {
        WRITE_ONCE(state, RPC_IDLE);
        release_slot = true;
        ret = -ECANCELED;
    }
    else if (state != RPC_PROCESSING || incoming.id != request.id)
        ret = -ENOENT;
    else if (incoming.status > 0)
        ret = -EINVAL;
    else {
        reply = incoming;
        WRITE_ONCE(state, RPC_DONE);
    }
    mutex_unlock(&state_lock);
    if (!ret)
        wake_up_interruptible(&client_wait);
    if (release_slot)
        wake_up_interruptible(&slot_wait);
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
    case MINI_BINDER_CALL:
        return call_server(file, user_arg);
    case MINI_BINDER_RECEIVE:
        return receive_request(file, user_arg);
    case MINI_BINDER_REPLY:
        return send_reply(file, user_arg);
    default:
        return -ENOTTY;
    }
}

static int mini_binder_release(struct inode *inode, struct file *file)
{
    struct mini_binder_file *ctx = file->private_data;
    bool wake = false;

    (void)inode;
    mutex_lock(&state_lock);
    if (server_file == file) {
        WRITE_ONCE(server_file, NULL);
        if (state == RPC_PENDING || state == RPC_PROCESSING) {
            WRITE_ONCE(state, RPC_FAILED);
            wake = true;
        } else if (state == RPC_CANCELLED) {
            WRITE_ONCE(state, RPC_IDLE);
        }
    }
    mutex_unlock(&state_lock);
    if (wake)
        wake_up_interruptible(&client_wait);
    wake_up_interruptible(&slot_wait);
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
