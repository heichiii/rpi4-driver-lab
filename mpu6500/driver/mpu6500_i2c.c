#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

#include "mpu6500.h"

/* First milestone: bind only after a successful, read-only identity check. */
static int mpu6500_probe(struct i2c_client *client)
{
    int identity;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_READ_BYTE_DATA))
        return dev_err_probe(&client->dev, -EOPNOTSUPP,
                             "adapter cannot read byte-data registers\n");

    identity = i2c_smbus_read_byte_data(client, MPU6500_REG_WHO_AM_I);
    if (identity < 0)
        return dev_err_probe(&client->dev, identity, "WHO_AM_I read failed\n");
    if (identity != MPU6500_WHO_AM_I_VALUE)
        return dev_err_probe(&client->dev, -ENODEV,
                             "WHO_AM_I mismatch: got 0x%02x, expected 0x%02x\n",
                             identity, MPU6500_WHO_AM_I_VALUE);

    dev_info(&client->dev, "probe: bus=%d address=0x%02x WHO_AM_I=0x%02x\n",
             client->adapter->nr, client->addr, identity);
    return 0;
}

static void mpu6500_remove(struct i2c_client *client)
{
    /* No configuration writes or device resources exist in this milestone. */
    dev_info(&client->dev, "remove: device unbound\n");
}

/* Lab-specific compatible avoids binding the upstream IIO driver by accident. */
static const struct of_device_id mpu6500_of_match[] = {
    { .compatible = "heichi,mpu6500-lab" },
    { }
};
MODULE_DEVICE_TABLE(of, mpu6500_of_match);

static const struct i2c_device_id mpu6500_ids[] = {
    { "mpu6500_lab", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, mpu6500_ids);

static struct i2c_driver mpu6500_driver = {
    .driver = {
        .name = "mpu6500_lab",
        .of_match_table = mpu6500_of_match,
    },
    .probe = mpu6500_probe,
    .remove = mpu6500_remove,
    .id_table = mpu6500_ids,
};
module_i2c_driver(mpu6500_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gao Kailong");
MODULE_DESCRIPTION("MPU6500 lab I2C identity and binding driver");
