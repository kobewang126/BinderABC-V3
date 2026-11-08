// SPDX-License-Identifier: GPL-2.0-only
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <mini_binder.h>

static long mini_binder_ioctl(struct file *file, unsigned int cmd,
                              unsigned long arg)
{
    __u32 version = MINI_BINDER_PROTOCOL_VERSION;

    (void)file;
    switch (cmd) {
    case MINI_BINDER_GET_VERSION:
        if (copy_to_user((void __user *)arg, &version, sizeof(version)))
            return -EFAULT;
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations mini_binder_fops = {
    .owner = THIS_MODULE,
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
MODULE_DESCRIPTION("BinderABC minimal character device");
