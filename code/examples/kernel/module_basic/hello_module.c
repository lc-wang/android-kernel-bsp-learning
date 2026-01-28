// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal Linux kernel module lifecycle example
 *
 * This file demonstrates the absolute minimum module behavior
 * used by all Linux kernel drivers.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lcwang");
MODULE_DESCRIPTION("Minimal kernel module lifecycle example");
MODULE_VERSION("1.0");

static int __init hello_init(void)
{
	pr_info("hello_module: init\n");
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("hello_module: exit\n");
}

module_init(hello_init);
module_exit(hello_exit);

