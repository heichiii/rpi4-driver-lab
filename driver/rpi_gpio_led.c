#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
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

struct rpi_gpio_data {
    struct cdev cdev;
    struct device char_dev;
    struct class *class;
    struct mutex lock;
    bool online;
    struct gpio_desc *led;
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

/* Return the logical GPIO level as "0\n" or "1\n". */
static ssize_t rpi_gpio_read(struct file *file, char __user *buffer,
                            size_t count, loff_t *position)
{
    struct rpi_gpio_data *data = file->private_data;
    char state[2];
    ssize_t ret;
    int value;

    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;
    if (!data->online) {
        ret = -ENODEV;
        goto unlock;
    }
    value = gpiod_get_value_cansleep(data->led);
    if (value < 0) {
        ret = value;
        goto unlock;
    }
    state[0] = value ? '1' : '0';
    state[1] = '\n';
    ret = simple_read_from_buffer(buffer, count, position, state, sizeof(state));
unlock:
    mutex_unlock(&data->lock);
    return ret;
}

/* Accept 0/1, optionally followed by the newline added by echo. */
static ssize_t rpi_gpio_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *position)
{
    struct rpi_gpio_data *data = file->private_data;
    char command[2];
    ssize_t ret;

    if (!count)
        return 0;
    if (count > sizeof(command))
        return -EMSGSIZE;
    if (copy_from_user(command, buffer, count))
        return -EFAULT;
    if ((command[0] != '0' && command[0] != '1') ||
        (count == 2 && command[1] != '\n'))
        return -EINVAL;
    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;
    if (!data->online) {
        ret = -ENODEV;
        goto unlock;
    }
    gpiod_set_value_cansleep(data->led, command[0] == '1');
    *position = 0;
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

    /* "led" maps to led-gpios; initialize logically OFF. */
    data->led = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
    if (IS_ERR(data->led)) {
        ret = dev_err_probe(&pdev->dev, PTR_ERR(data->led),
                            "failed to acquire LED GPIO\n");
        goto put_device;
    }

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
        gpiod_set_value_cansleep(data->led, 0);
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
    gpiod_set_value_cansleep(data->led, 0);
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

static const struct of_device_id rpi_gpio_of_match[] = {
    { .compatible = "heichi,rpi-gpio" },
    { }
};
MODULE_DEVICE_TABLE(of, rpi_gpio_of_match);

static struct platform_driver rpi_gpio_driver = {
    .probe = rpi_gpio_probe,
    .remove = rpi_gpio_remove,
    .driver = {
        .name = DEVICE_NAME,
        .of_match_table = rpi_gpio_of_match,
    },
};

module_platform_driver(rpi_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gao Kailong");
MODULE_DESCRIPTION("Raspberry Pi 4 device-tree GPIO LED driver");
MODULE_ALIAS("platform:" DEVICE_NAME);
