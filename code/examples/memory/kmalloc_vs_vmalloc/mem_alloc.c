// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

static void *kmem;
static void *vmem;

static int __init mem_alloc_init(void)
{
	kmem = kmalloc(1024, GFP_KERNEL);
	if (!kmem)
		return -ENOMEM;

	vmem = vmalloc(1024 * 1024);
	if (!vmem) {
		kfree(kmem);
		return -ENOMEM;
	}

	pr_info("kmalloc addr=%px\n", kmem);
	pr_info("vmalloc addr=%px\n", vmem);

	return 0;
}

static void __exit mem_alloc_exit(void)
{
	vfree(vmem);
	kfree(kmem);

	pr_info("memory freed\n");
}

module_init(mem_alloc_init);
module_exit(mem_alloc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lcwang");
MODULE_DESCRIPTION("kmalloc vs vmalloc example");

