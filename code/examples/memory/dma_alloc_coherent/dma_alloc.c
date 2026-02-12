// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#define BUF_SIZE 4096

static void *cpu_addr;
static dma_addr_t dma_addr;

static int __init dma_example_init(void)
{
	struct device *dev = NULL;

	/*
	 * 注意：
	 * 這裡為了示範，使用 NULL device
	 * 真實 driver 一定要用 &pdev->dev
	 */
	cpu_addr = dma_alloc_coherent(dev,
				      BUF_SIZE,
				      &dma_addr,
				      GFP_KERNEL);
	if (!cpu_addr)
		return -ENOMEM;

	pr_info("dma_alloc_coherent:\n");
	pr_info("  cpu_addr = %px\n", cpu_addr);
	pr_info("  dma_addr = %pad\n", &dma_addr);

	return 0;
}

static void __exit dma_example_exit(void)
{
	struct device *dev = NULL;

	dma_free_coherent(dev, BUF_SIZE, cpu_addr, dma_addr);
	pr_info("dma memory freed\n");
}

module_init(dma_example_init);
module_exit(dma_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lcwang");
MODULE_DESCRIPTION("dma_alloc_coherent example");

