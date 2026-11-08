// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include <mini_binder.h>

enum rpc_state { IDLE, PENDING, PROCESSING, DONE, FAILED, CANCELLED };

static DEFINE_MUTEX(lock);
static DECLARE_WAIT_QUEUE_HEAD(server_wait);
static DECLARE_WAIT_QUEUE_HEAD(client_wait);
static struct file *server_file;
static enum rpc_state state = IDLE;
static struct mini_binder_request request;
static struct mini_binder_reply reply;

static long register_server(struct file *file)
{
    long ret = 0;

    mutex_lock(&lock);
    if (server_file)
        ret = -EBUSY;
    else
        server_file = file;
    mutex_unlock(&lock);
    return ret;
}

static long call_server(struct file *file, void __user *arg)
{
    struct mini_binder_call call;
    long ret;

    if (copy_from_user(&call, arg, sizeof(call)))
        return -EFAULT;
    mutex_lock(&lock);
    if (!server_file)
        ret = -ENOTCONN;
    else if (server_file == file)
        ret = -EINVAL;
    else if (state != IDLE)
        ret = -EBUSY;
    else {
        request.a = call.a;
        request.b = call.b;
        WRITE_ONCE(state, PENDING);
        ret = 0;
    }
    mutex_unlock(&lock);
    if (ret)
        return ret;

    wake_up_interruptible(&server_wait);
    ret = wait_event_interruptible(client_wait,
            READ_ONCE(state) == DONE || READ_ONCE(state) == FAILED);

    mutex_lock(&lock);
    if (!ret && state == DONE) {
        call.result = reply.result;
        call.status = reply.status;
    } else if (!ret) {
        ret = -EPIPE;
    }
    if (ret && state == PROCESSING)
        WRITE_ONCE(state, CANCELLED);
    else
        WRITE_ONCE(state, IDLE);
    mutex_unlock(&lock);

    if (!ret && copy_to_user(arg, &call, sizeof(call)))
        ret = -EFAULT;
    return ret;
}

static long receive_request(struct file *file, void __user *arg)
{
    long ret;

    for (;;) {
        mutex_lock(&lock);
        if (server_file != file) {
            mutex_unlock(&lock);
            return -EPERM;
        }
        if (state == PENDING) {
            WRITE_ONCE(state, PROCESSING);
            if (copy_to_user(arg, &request, sizeof(request))) {
                WRITE_ONCE(state, FAILED);
                mutex_unlock(&lock);
                wake_up_interruptible(&client_wait);
                return -EFAULT;
            }
            mutex_unlock(&lock);
            return 0;
        }
        mutex_unlock(&lock);
        ret = wait_event_interruptible(server_wait, READ_ONCE(state) == PENDING);
        if (ret)
            return ret;
    }
}

static long send_reply(struct file *file, void __user *arg)
{
    struct mini_binder_reply incoming;
    long ret;

    if (copy_from_user(&incoming, arg, sizeof(incoming)))
        return -EFAULT;
    mutex_lock(&lock);
    if (server_file != file)
        ret = -EPERM;
    else if (state == CANCELLED) {
        WRITE_ONCE(state, IDLE);
        ret = -ECANCELED;
    } else if (state != PROCESSING)
        ret = -EINVAL;
    else if (incoming.status > 0)
        ret = -EINVAL;
    else {
        reply = incoming;
        WRITE_ONCE(state, DONE);
        ret = 0;
    }
    mutex_unlock(&lock);
    if (!ret)
        wake_up_interruptible(&client_wait);
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
    bool wake = false;

    (void)inode;
    mutex_lock(&lock);
    if (server_file == file) {
        server_file = NULL;
        if (state == PENDING || state == PROCESSING) {
            WRITE_ONCE(state, FAILED);
            wake = true;
        } else if (state == CANCELLED) {
            WRITE_ONCE(state, IDLE);
        }
    }
    mutex_unlock(&lock);
    if (wake)
        wake_up_interruptible(&client_wait);
    return 0;
}

static const struct file_operations mini_binder_fops = {
    .owner = THIS_MODULE,
    .release = mini_binder_release,
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
MODULE_DESCRIPTION("BinderABC single-client synchronous RPC");
