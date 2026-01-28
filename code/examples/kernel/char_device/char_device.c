// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mychardev"
#define CLASS_NAME  "mychar"

static dev_t devno;
static struct cdev my_cdev;
static struct class *my_class;

static char buffer[128];
static size_t buffer_size;

/* ================= file operations ================= */

static int my_open(struct inode *inode, struct file *file)
{
	pr_info("mychardev: open\n");
	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
	pr_info("mychardev: release\n");
	return 0;
}

static ssize_t my_read(struct file *file,
		       char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	size_t len;

	len = min(count, buffer_size);

	if (copy_to_user(buf, buffer, len))
		return -EFAULT;

	pr_info("mychardev: read %zu bytes\n", len);
	return len;
}

static ssize_t my_write(struct file *file,
			const char __user *buf,
			size_t count,
			loff_t *ppos)
{
	size_t len;

	len = min(count, sizeof(buffer));

	if (copy_from_user(buffer, buf, len))
		return -EFAULT;

	buffer_size = len;

	pr_info("mychardev: write %zu bytes\n", len);
	return len;
}

static long my_ioctl(struct file *file,
		     unsigned int cmd,
		     unsigned long arg)
{
	pr_info("mychardev: ioctl cmd=0x%x\n", cmd);
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

/* ================= module init ================= */

static int __init my_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
	if (ret)
		return ret;

	cdev_init(&my_cdev, &my_fops);
	my_cdev.owner = THIS_MODULE;

	ret = cdev_add(&my_cdev, devno, 1);
	if (ret)
		goto err_cdev;

	my_class = class_create(CLASS_NAME);
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		goto err_class;
	}

	device_create(my_class, NULL, devno, NULL, DEVICE_NAME);

	pr_info("mychardev: registered major=%d minor=%d\n",
		MAJOR(devno), MINOR(devno));
	return 0;

err_class:
	cdev_del(&my_cdev);
err_cdev:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static void __exit my_exit(void)
{
	device_destroy(my_class, devno);
	class_destroy(my_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(devno, 1);

	pr_info("mychardev: removed\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lcwang");
MODULE_DESCRIPTION("Minimal character device example");

