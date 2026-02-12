// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mymmap"

static struct page *shared_page;
static void *kaddr; /* kernel mapping for the page */

static int my_open(struct inode *inode, struct file *file)
{
	pr_info("%s: open\n", DEVICE_NAME);
	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
	pr_info("%s: release\n", DEVICE_NAME);
	return 0;
}

static ssize_t my_read(struct file *file, char __user *buf,
		       size_t count, loff_t *ppos)
{
	/* 只是方便觀察：讓你能用 read 讀到 page 內容 */
	size_t len = min_t(size_t, count, PAGE_SIZE);

	if (!kaddr)
		return -EINVAL;

	if (copy_to_user(buf, kaddr, len))
		return -EFAULT;

	pr_info("%s: read %zu bytes\n", DEVICE_NAME, len);
	return len;
}

static int my_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long pfn;

	if (!shared_page)
		return -EINVAL;

	if (vsize > PAGE_SIZE)
		return -EINVAL;

	/*
	 * 建議：mmap 的區域不要可 expand / dump
	 * （簡化示範用，不追求所有 flag 完整性）
	 */
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;

	pfn = page_to_pfn(shared_page);

	pr_info("%s: mmap vsize=%lu, user=[0x%lx-0x%lx], pfn=0x%lx\n",
		DEVICE_NAME, vsize, vma->vm_start, vma->vm_end, pfn);

	/*
	 * 把該 page 映射到 userspace。
	 * 注意：這是「直接映射 PFN」的最小模型。
	 */
	if (remap_pfn_range(vma,
			    vma->vm_start,
			    pfn,
			    vsize,
			    vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static const struct file_operations my_fops = {
	.owner   = THIS_MODULE,
	.open    = my_open,
	.release = my_release,
	.read    = my_read,
	.mmap    = my_mmap,
};

static struct miscdevice my_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DEVICE_NAME,
	.fops  = &my_fops,
};

static int __init my_init(void)
{
	int ret;

	shared_page = alloc_page(GFP_KERNEL);
	if (!shared_page)
		return -ENOMEM;

	/*
	 * 取得 kernel 可直接存取的 mapping。
	 * 這裡用 page_address（一般 lowmem 會有），夠示範用。
	 */
	kaddr = page_address(shared_page);
	if (!kaddr) {
		__free_page(shared_page);
		return -ENOMEM;
	}

	memset(kaddr, 0, PAGE_SIZE);
	snprintf((char *)kaddr, PAGE_SIZE,
		 "hello from kernel page! (mmap shared)\n");

	ret = misc_register(&my_miscdev);
	if (ret) {
		__free_page(shared_page);
		shared_page = NULL;
		kaddr = NULL;
		return ret;
	}

	pr_info("%s: registered as /dev/%s, shared_page=%px kaddr=%px\n",
		DEVICE_NAME, DEVICE_NAME, shared_page, kaddr);

	return 0;
}

static void __exit my_exit(void)
{
	misc_deregister(&my_miscdev);

	if (shared_page) {
		__free_page(shared_page);
		shared_page = NULL;
		kaddr = NULL;
	}

	pr_info("%s: removed\n", DEVICE_NAME);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lcwang");
MODULE_DESCRIPTION("Minimal mmap driver example (remap_pfn_range)");

