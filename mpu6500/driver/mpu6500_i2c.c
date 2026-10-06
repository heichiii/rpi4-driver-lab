#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

#include "mpu6500.h"

/* Fixed configuration for this lab; reset first to remove inherited settings. */
static const struct {
    u8 reg;
    u8 value;
} mpu6500_config[] = {
    { MPU6500_REG_PWR_MGMT_1, MPU6500_CLOCK_AUTO },
    { MPU6500_REG_PWR_MGMT_2, 0x00 }, /* Enable all six axes. */
    { MPU6500_REG_GYRO_CONFIG, 0x00 }, /* +/-250 dps, DLPF enabled. */
    { MPU6500_REG_ACCEL_CONFIG, 0x00 }, /* +/-2 g, self-test disabled. */
    { MPU6500_REG_CONFIG, MPU6500_DLPF_CFG },
    { MPU6500_REG_ACCEL_CONFIG_2, MPU6500_DLPF_CFG },
    { MPU6500_REG_SMPLRT_DIV, MPU6500_SAMPLE_RATE_DIV },
};

static int mpu6500_initialize(struct i2c_client *client)
{
    int ret;
    size_t i;

    ret = i2c_smbus_write_byte_data(client, MPU6500_REG_PWR_MGMT_1,
                                   MPU6500_DEVICE_RESET);
    if (ret < 0)
        return dev_err_probe(&client->dev, ret, "device reset failed\n");
    msleep(100);

    for (i = 0; i < ARRAY_SIZE(mpu6500_config); i++) {
        ret = i2c_smbus_write_byte_data(client, mpu6500_config[i].reg,
                                       mpu6500_config[i].value);
        if (ret < 0)
            return dev_err_probe(&client->dev, ret,
                                 "write register 0x%02x failed\n",
                                 mpu6500_config[i].reg);
    }

    /* Allow clock, accelerometer and gyroscope startup before sampling. */
    msleep(100);
    for (i = 0; i < ARRAY_SIZE(mpu6500_config); i++) {
        ret = i2c_smbus_read_byte_data(client, mpu6500_config[i].reg);
        if (ret < 0)
            return dev_err_probe(&client->dev, ret,
                                 "readback register 0x%02x failed\n",
                                 mpu6500_config[i].reg);
        if (ret != mpu6500_config[i].value)
            return dev_err_probe(&client->dev, -EIO,
                                 "register 0x%02x: got 0x%02x, expected 0x%02x\n",
                                 mpu6500_config[i].reg, ret,
                                 mpu6500_config[i].value);
    }
    return 0;
}

static s16 mpu6500_decode_axis(const u8 *data)
{
    /* Registers contain signed 16-bit, high byte first. */
    return (s16)(((u16)data[0] << 8) | data[1]);
}

static int mpu6500_read_sample(struct i2c_client *client,
                              struct mpu6500_sample *sample)
{
    u8 reg = MPU6500_REG_ACCEL_XOUT_H;
    u8 data[MPU6500_SAMPLE_BYTES];
    struct i2c_msg messages[] = {
        {
            .addr = client->addr,
            .len = sizeof(reg),
            .buf = &reg,
        },
        {
            .addr = client->addr,
            .flags = I2C_M_RD,
            .len = sizeof(data),
            .buf = data,
        },
    };
    int ret;
    int i;

    /* One combined transaction: register pointer, repeated START, 14 bytes. */
    ret = i2c_transfer(client->adapter, messages, ARRAY_SIZE(messages));
    if (ret < 0)
        return ret;
    if (ret != ARRAY_SIZE(messages))
        return -EIO;

    for (i = 0; i < 3; i++) {
        sample->accel[i] = mpu6500_decode_axis(&data[2 * i]);
        sample->gyro[i] = mpu6500_decode_axis(&data[8 + 2 * i]);
    }
    sample->temperature = mpu6500_decode_axis(&data[6]);
    return 0;
}

static void mpu6500_sleep(struct i2c_client *client)
{
    int ret;

    ret = i2c_smbus_write_byte_data(client, MPU6500_REG_PWR_MGMT_1,
                                   MPU6500_SLEEP);
    if (ret < 0)
        dev_warn(&client->dev, "could not put device to sleep: %d\n", ret);
}

static int mpu6500_probe(struct i2c_client *client)
{
    struct mpu6500_sample sample;
    int identity;
    int ret;

    if (!i2c_check_functionality(client->adapter,
                                 I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_I2C))
        return dev_err_probe(&client->dev, -EOPNOTSUPP,
                             "adapter needs byte-data read/write and I2C transfers\n");

    identity = i2c_smbus_read_byte_data(client, MPU6500_REG_WHO_AM_I);
    if (identity < 0)
        return dev_err_probe(&client->dev, identity, "WHO_AM_I read failed\n");
    if (identity != MPU6500_WHO_AM_I_VALUE)
        return dev_err_probe(&client->dev, -ENODEV,
                             "WHO_AM_I mismatch: got 0x%02x, expected 0x%02x\n",
                             identity, MPU6500_WHO_AM_I_VALUE);

    ret = mpu6500_initialize(client);
    if (ret)
        goto err_sleep;

    ret = mpu6500_read_sample(client, &sample);
    if (ret) {
        dev_err_probe(&client->dev, ret, "initial sample read failed\n");
        goto err_sleep;
    }

    dev_info(&client->dev, "probe: bus=%d address=0x%02x WHO_AM_I=0x%02x\n",
             client->adapter->nr, client->addr, identity);
    dev_info(&client->dev, "initialized: accel=+/-2g gyro=+/-250dps rate=100Hz DLPF=3\n");
    dev_info(&client->dev,
             "sample raw: accel=(%d,%d,%d) temp=%d gyro=(%d,%d,%d)\n",
             sample.accel[0], sample.accel[1], sample.accel[2],
             sample.temperature, sample.gyro[0], sample.gyro[1], sample.gyro[2]);
    return 0;

err_sleep:
    mpu6500_sleep(client);
    return ret;
}

static void mpu6500_remove(struct i2c_client *client)
{
    mpu6500_sleep(client);
    dev_info(&client->dev, "remove: device unbound, sleep requested\n");
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
MODULE_DESCRIPTION("MPU6500 lab I2C initialization and raw sample driver");
