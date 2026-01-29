// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mymisc"

static char buffer[128];
static size_t buffer_size;

/* ================= file operations ================= */

static int my_open(struct inode *inode, struct file *file)
{
	pr_info("mymisc: open\n");
	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
	pr_info("mymisc: release\n");
	return 0;
}

static ssize_t my_read(struct file *file,
		       char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	size_t len = min(count, buffer_size);

	if (copy_to_user(buf, buffer, len))
		return -EFAULT;

	pr_info("mymisc: read %zu bytes\n", len);
	return len;
}

static ssize_t my_write(struct file *file,
			const char __user *buf,
			size_t count,
			loff_t *ppos)
{
	size_t len = min(count, sizeof(buffer));

	if (copy_from_user(buffer, buf, len))
		return -EFAULT;

	buffer_size = len;

	pr_info("mymisc: write %zu bytes\n", len);
	return len;
}

static long my_ioctl(struct file *file,
		     unsigned int cmd,
		     unsigned long arg)
{
	pr_info("mymisc: ioctl cmd=0x%x\n", cmd);
	return 0;
}

static const struct file_operations my_fops = {
	.owner          = THIS_MODULE,
	.open           = my_open,
	.release        = my_release,
	.read           = my_read,
	.write          = my_write,
	.unlocked_ioctl = my_ioctl,
};

/* ================= miscdevice ================= */

static struct miscdevice my_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DEVICE_NAME,
	.fops  = &my_fops,
};

/* ================= module init ================= */

static int __init my_init(void)
{
	int ret;

	ret = misc_register(&my_miscdev);
	if (ret)
		return ret;

	pr_info("mymisc: registered as /dev/%s\n", DEVICE_NAME);
	return 0;
}

static void __exit my_exit(void)
{
	misc_deregister(&my_miscdev);
	pr_info("mymisc: removed\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lcwang");
MODULE_DESCRIPTION("Minimal miscdevice example");

