// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#define DRV_NAME "my_platform_dev"

/* ================= of match table ================= */

static const struct of_device_id my_of_match[] = {
	{ .compatible = "example,my-platform-dev" },
	{ }
};
MODULE_DEVICE_TABLE(of, my_of_match);

/* ================= platform driver ================= */

static int my_platform_probe(struct platform_device *pdev)
{
	pr_info("%s: probe called\n", DRV_NAME);
	pr_info("device name: %s\n", dev_name(&pdev->dev));
	return 0;
}

static int my_platform_remove(struct platform_device *pdev)
{
	pr_info("%s: remove called\n", DRV_NAME);
	return 0;
}

static struct platform_driver my_platform_driver = {
	.probe  = my_platform_probe,
	.remove = my_platform_remove,
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = my_of_match,
	},
};

/* ================= module init ================= */

static int __init my_init(void)
{
	pr_info("%s: init\n", DRV_NAME);
	return platform_driver_register(&my_platform_driver);
}

static void __exit my_exit(void)
{
	pr_info("%s: exit\n", DRV_NAME);
	platform_driver_unregister(&my_platform_driver);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lcwang");
MODULE_DESCRIPTION("Minimal platform bus driver example");

