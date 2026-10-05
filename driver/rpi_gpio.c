#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "rpi_gpio"
#define CLASS_NAME "rpi_gpio"
#define BUFFER_SIZE 128

static dev_t dev_num;
static struct cdev rpi_gpio_cdev;
static struct class *rpi_gpio_class;

static char kernel_buffer[BUFFER_SIZE];
static size_t data_size;
static DEFINE_MUTEX(buffer_lock);

static int rpi_gpio_open(struct inode *inode, struct file *file)
{
    return nonseekable_open(inode, file);
}

static ssize_t rpi_gpio_read(struct file *file, char __user *buffer,
                             size_t count, loff_t *position)
{
    ssize_t ret;

    if (mutex_lock_interruptible(&buffer_lock))
        return -ERESTARTSYS;

    ret = simple_read_from_buffer(buffer, count, position,
                                  kernel_buffer, data_size);
    mutex_unlock(&buffer_lock);
    return ret;
}

/* Each successful write replaces the buffer; no GPIO hardware is accessed. */
static ssize_t rpi_gpio_write(struct file *file, const char __user *buffer,
                              size_t count, loff_t *position)
{
    char temporary_buffer[BUFFER_SIZE];

    if (!count)
        return 0;
    if (count > BUFFER_SIZE)
        return -EMSGSIZE;
    if (copy_from_user(temporary_buffer, buffer, count))
        return -EFAULT;
    if (mutex_lock_interruptible(&buffer_lock))
        return -ERESTARTSYS;

    memcpy(kernel_buffer, temporary_buffer, count);
    data_size = count;
    *position = 0;

    mutex_unlock(&buffer_lock);
    return count;
}

static const struct file_operations rpi_gpio_fops = {
    .owner = THIS_MODULE,
    .open = rpi_gpio_open,
    .read = rpi_gpio_read,
    .write = rpi_gpio_write,
};

static int __init rpi_gpio_init(void)
{
    struct device *device;
    int ret;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&rpi_gpio_cdev, &rpi_gpio_fops);
    rpi_gpio_cdev.owner = THIS_MODULE;
    ret = cdev_add(&rpi_gpio_cdev, dev_num, 1);
    if (ret)
        goto unregister_region;

    rpi_gpio_class = class_create(CLASS_NAME);
    if (IS_ERR(rpi_gpio_class)) {
        ret = PTR_ERR(rpi_gpio_class);
        goto delete_cdev;
    }

    device = device_create(rpi_gpio_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        goto destroy_class;
    }

    pr_info("rpi_gpio: registered major=%u minor=%u\n",
            MAJOR(dev_num), MINOR(dev_num));
    return 0;

 destroy_class:
    class_destroy(rpi_gpio_class);
 delete_cdev:
    cdev_del(&rpi_gpio_cdev);
 unregister_region:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit rpi_gpio_exit(void)
{
    device_destroy(rpi_gpio_class, dev_num);
    class_destroy(rpi_gpio_class);
    cdev_del(&rpi_gpio_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("rpi_gpio: unregistered\n");
}

module_init(rpi_gpio_init);
module_exit(rpi_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gao Kailong");
MODULE_DESCRIPTION("Raspberry Pi 4 buffered character device lab");
