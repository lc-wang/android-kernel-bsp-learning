// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static DECLARE_COMPLETION(my_comp);
static struct task_struct *worker;

static int worker_thread(void *data)
{
    pr_info("Worker: doing work...\n");
    msleep(2000);
    complete(&my_comp);
    pr_info("Worker: completed\n");
    return 0;
}

static int __init completion_example_init(void)
{
    pr_info("completion_example: init\n");

    worker = kthread_run(worker_thread, NULL, "comp_worker");
    if (IS_ERR(worker))
        return PTR_ERR(worker);

    pr_info("Main: waiting for worker...\n");
    wait_for_completion(&my_comp);
    pr_info("Main: worker done!\n");

    return 0;
}

static void __exit completion_example_exit(void)
{
    if (worker)
        kthread_stop(worker);

    pr_info("completion_example: exit\n");
}

module_init(completion_example_init);
module_exit(completion_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Completion synchronization example");
