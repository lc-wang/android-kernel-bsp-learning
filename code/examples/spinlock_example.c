// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

static spinlock_t my_lock;
static int counter = 0;

static int __init spinlock_example_init(void)
{
    unsigned long flags;

    spin_lock_init(&my_lock);

    pr_info("spinlock_example: init\n");

    spin_lock_irqsave(&my_lock, flags);
    counter++;
    pr_info("spinlock_example: counter = %d (locked)\n", counter);
    spin_unlock_irqrestore(&my_lock, flags);

    return 0;
}

static void __exit spinlock_example_exit(void)
{
    pr_info("spinlock_example: exit\n");
}

module_init(spinlock_example_init);
module_exit(spinlock_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Spinlock synchronization example");
