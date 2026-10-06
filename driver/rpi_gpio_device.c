#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

/* Phase 3 only: describe a software test device without a Device Tree. */
static struct platform_device *rpi_gpio_device;

static int __init rpi_gpio_device_init(void)
{
    rpi_gpio_device = platform_device_register_simple("rpi_gpio",
                                                     PLATFORM_DEVID_NONE,
                                                     NULL, 0);
    if (IS_ERR(rpi_gpio_device))
        return PTR_ERR(rpi_gpio_device);
    pr_info("rpi_gpio_device: registered platform test device\n");
    return 0;
}

static void __exit rpi_gpio_device_exit(void)
{
    platform_device_unregister(rpi_gpio_device);
    pr_info("rpi_gpio_device: unregistered platform test device\n");
}

module_init(rpi_gpio_device_init);
module_exit(rpi_gpio_device_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gao Kailong");
MODULE_DESCRIPTION("Phase 3 software platform device for rpi_gpio");
