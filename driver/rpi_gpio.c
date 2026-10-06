#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "rpi_gpio"
#define CLASS_NAME "rpi_gpio"
#define BUFFER_SIZE 128

struct rpi_gpio_data {
    struct cdev cdev;
    struct device char_dev;
    struct class *class;
    struct mutex lock;
    bool online;
    char buffer[BUFFER_SIZE];
    size_t data_size;
};

/* cdev and open files hold references to char_dev until they are finished. */
static void rpi_gpio_device_release(struct device *device)
{
    struct rpi_gpio_data *data =
        container_of(device, struct rpi_gpio_data, char_dev);

    kfree(data);
}

static int rpi_gpio_open(struct inode *inode, struct file *file)
{
    struct rpi_gpio_data *data =
        container_of(inode->i_cdev, struct rpi_gpio_data, cdev);
    int ret;

    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;
    if (!data->online) {
        ret = -ENODEV;
        goto unlock;
    }
    ret = nonseekable_open(inode, file);
    if (!ret) {
        get_device(&data->char_dev);
        file->private_data = data;
    }
unlock:
    mutex_unlock(&data->lock);
    return ret;
}

static int rpi_gpio_release(struct inode *inode, struct file *file)
{
    struct rpi_gpio_data *data = file->private_data;

    put_device(&data->char_dev);
    return 0;
}

static ssize_t rpi_gpio_read(struct file *file, char __user *buffer,
                            size_t count, loff_t *position)
{
    struct rpi_gpio_data *data = file->private_data;
    ssize_t ret;

    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;
    if (!data->online)
        ret = -ENODEV;
    else
        ret = simple_read_from_buffer(buffer, count, position,
                                      data->buffer, data->data_size);
    mutex_unlock(&data->lock);
    return ret;
}

/* Each write replaces the buffer; actual GPIO control comes in a later phase. */
static ssize_t rpi_gpio_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *position)
{
    struct rpi_gpio_data *data = file->private_data;
    char temporary_buffer[BUFFER_SIZE];
    ssize_t ret;

    if (count > BUFFER_SIZE)
        return -EMSGSIZE;
    if (copy_from_user(temporary_buffer, buffer, count))
        return -EFAULT;
    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;
    if (!data->online) {
        ret = -ENODEV;
        goto unlock;
    }
    if (count) {
        memcpy(data->buffer, temporary_buffer, count);
        data->data_size = count;
        *position = 0;
    }
    ret = count;
unlock:
    mutex_unlock(&data->lock);
    return ret;
}

static const struct file_operations rpi_gpio_fops = {
    .owner = THIS_MODULE,
    .open = rpi_gpio_open,
    .release = rpi_gpio_release,
    .read = rpi_gpio_read,
    .write = rpi_gpio_write,
};

static int rpi_gpio_probe(struct platform_device *pdev)
{
    struct rpi_gpio_data *data;
    int ret;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    mutex_init(&data->lock);
    device_initialize(&data->char_dev);
    data->char_dev.release = rpi_gpio_device_release;
    data->char_dev.parent = &pdev->dev;

    ret = alloc_chrdev_region(&data->char_dev.devt, 0, 1, DEVICE_NAME);
    if (ret)
        goto put_device;
    data->class = class_create(CLASS_NAME);
    if (IS_ERR(data->class)) {
        ret = PTR_ERR(data->class);
        goto unregister_region;
    }
    data->char_dev.class = data->class;
    ret = dev_set_name(&data->char_dev, DEVICE_NAME);
    if (ret)
        goto destroy_class;

    cdev_init(&data->cdev, &rpi_gpio_fops);
    data->cdev.owner = THIS_MODULE;
    data->online = true;
    /* This helper links the lifetimes of the embedded cdev and device. */
    ret = cdev_device_add(&data->cdev, &data->char_dev);
    if (ret) {
        mutex_lock(&data->lock);
        data->online = false;
        mutex_unlock(&data->lock);
        goto destroy_class;
    }
    platform_set_drvdata(pdev, data);
    dev_info(&pdev->dev, "probe: /dev/%s major=%u minor=%u\n",
             DEVICE_NAME, MAJOR(data->char_dev.devt),
             MINOR(data->char_dev.devt));
    return 0;

destroy_class:
    class_destroy(data->class);
unregister_region:
    unregister_chrdev_region(data->char_dev.devt, 1);
put_device:
    put_device(&data->char_dev);
    return ret;
}

static void rpi_gpio_remove(struct platform_device *pdev)
{
    struct rpi_gpio_data *data = platform_get_drvdata(pdev);

    mutex_lock(&data->lock);
    data->online = false;
    mutex_unlock(&data->lock);
    cdev_device_del(&data->cdev, &data->char_dev);
    class_destroy(data->class);
    unregister_chrdev_region(data->char_dev.devt, 1);
    platform_set_drvdata(pdev, NULL);
    dev_info(&pdev->dev, "remove: character device unregistered\n");
    /* Existing open files retain data and return -ENODEV on further I/O. */
    put_device(&data->char_dev);
}

static struct platform_driver rpi_gpio_driver = {
    .probe = rpi_gpio_probe,
    .remove = rpi_gpio_remove,
    .driver = {
        .name = DEVICE_NAME,
    },
};

module_platform_driver(rpi_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gao Kailong");
MODULE_DESCRIPTION("Raspberry Pi 4 platform character device lab");
MODULE_ALIAS("platform:" DEVICE_NAME);
