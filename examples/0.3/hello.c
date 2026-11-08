// SPDX-License-Identifier: GPL-2.0-only
#include <linux/init.h>
#include <linux/module.h>

static int __init binderabc_hello_init(void)
{
    pr_info("binderabc_hello: loaded\n");
    return 0;
}

static void __exit binderabc_hello_exit(void)
{
    pr_info("binderabc_hello: unloaded\n");
}

module_init(binderabc_hello_init);
module_exit(binderabc_hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BinderABC");
MODULE_DESCRIPTION("BinderABC introductory kernel module");
