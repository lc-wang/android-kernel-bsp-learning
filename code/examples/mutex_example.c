// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static DEFINE_MUTEX(my_mutex);
static int shared_data = 0;
static struct task_struct *thread1, *thread2;

static int worker_thread(void *data)
{
    int id = *(int *)data;
    int i;

    for (i = 0; i < 5; i++) {
        mutex_lock(&my_mutex);
        shared_data++;
        pr_info("Thread %d: shared_data = %d\n", id, shared_data);
        msleep(100);
        mutex_unlock(&my_mutex);
    }

    return 0;
}

static int __init mutex_example_init(void)
{
    static int id1 = 1, id2 = 2;

    pr_info("mutex_example: init\n");
    thread1 = kthread_run(worker_thread, &id1, "mutex_thread1");
    thread2 = kthread_run(worker_thread, &id2, "mutex_thread2");

    return 0;
}

static void __exit mutex_example_exit(void)
{
    if (thread1)
        kthread_stop(thread1);
    if (thread2)
        kthread_stop(thread2);

    pr_info("mutex_example: exit\n");
}

module_init(mutex_example_init);
module_exit(mutex_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Mutex synchronization example");
