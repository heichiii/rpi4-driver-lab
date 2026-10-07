#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/iio/iio.h>

#include "mpu6500.h"

struct mpu6500_state {
    struct i2c_client *client;
    struct regmap *regmap;
    struct mutex lock;
};

static const struct regmap_config mpu6500_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 0x7f,
    /* Always access hardware, including changing sensor output registers. */
    .cache_type = REGCACHE_NONE,
};

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

static int mpu6500_initialize(struct mpu6500_state *state)
{
    struct i2c_client *client = state->client;
    unsigned int value;
    int ret;
    size_t i;

    ret = regmap_write(state->regmap, MPU6500_REG_PWR_MGMT_1,
                                   MPU6500_DEVICE_RESET);
    if (ret < 0)
        return dev_err_probe(&client->dev, ret, "device reset failed\n");
    msleep(100); // Allow reset to complete before writing configuration registers.

    for (i = 0; i < ARRAY_SIZE(mpu6500_config); i++) {
        ret = regmap_write(state->regmap, mpu6500_config[i].reg,
                                       mpu6500_config[i].value);
        if (ret < 0)
            return dev_err_probe(&client->dev, ret,
                                 "write register 0x%02x failed\n",
                                 mpu6500_config[i].reg);
    }

    /* Allow clock, accelerometer and gyroscope startup before sampling. */
    msleep(100);
    for (i = 0; i < ARRAY_SIZE(mpu6500_config); i++) {
        ret = regmap_read(state->regmap, mpu6500_config[i].reg, &value);
        if (ret < 0)
            return dev_err_probe(&client->dev, ret,
                                 "readback register 0x%02x failed\n",
                                 mpu6500_config[i].reg);
        if (value != mpu6500_config[i].value)
            return dev_err_probe(&client->dev, -EIO,
                                 "register 0x%02x: got 0x%02x, expected 0x%02x\n",
                                 mpu6500_config[i].reg, value,
                                 mpu6500_config[i].value);
    }
    return 0;
}

static s16 mpu6500_decode_axis(const u8 *data)
{
    /* Registers contain signed 16-bit, high byte first. */
    return (s16)(((u16)data[0] << 8) | data[1]);
}

static int mpu6500_read_sample(struct mpu6500_state *state,
                              struct mpu6500_sample *sample)
{
    u8 data[MPU6500_SAMPLE_BYTES];
    int ret;
    int i;

    ret = regmap_bulk_read(state->regmap, MPU6500_REG_ACCEL_XOUT_H,
                           data, sizeof(data));
    if (ret)
        return ret;

    for (i = 0; i < 3; i++) {
        sample->accel[i] = mpu6500_decode_axis(&data[2 * i]);
        sample->gyro[i] = mpu6500_decode_axis(&data[8 + 2 * i]);
    }
    sample->temperature = mpu6500_decode_axis(&data[6]);
    return 0;
}

/* 写电源管理寄存器，使芯片休眠 */
static void mpu6500_sleep(void *data)
{
    struct mpu6500_state *state = data;
    struct i2c_client *client = state->client;
    int ret;

    ret = regmap_write(state->regmap, MPU6500_REG_PWR_MGMT_1,
                                   MPU6500_SLEEP);
    if (ret < 0)
        dev_warn(&client->dev, "could not put device to sleep: %d\n", ret);
}

static int mpu6500_read_raw(struct iio_dev *indio_dev,
                           const struct iio_chan_spec *chan,
                           int *val, int *val2, long mask)
{
    struct mpu6500_state *state = iio_priv(indio_dev);
    struct mpu6500_sample sample;
    int ret;

    switch (mask) {
    case IIO_CHAN_INFO_RAW:
        mutex_lock(&state->lock);
        ret = mpu6500_read_sample(state, &sample);
        mutex_unlock(&state->lock);
        if (ret)
            return ret;
        switch (chan->type) {
        case IIO_ACCEL:
            *val = sample.accel[chan->address];
            break;
        case IIO_ANGL_VEL:
            *val = sample.gyro[chan->address];
            break;
        case IIO_TEMP:
            *val = sample.temperature;
            break;
        default:
            return -EINVAL;
        }
        return IIO_VAL_INT;
    case IIO_CHAN_INFO_SCALE:
        switch (chan->type) {
        case IIO_ACCEL:
            /* 9.80665 / 16384 m/s^2 per count. */
            *val = 980665;
            *val2 = 1638400000;
            return IIO_VAL_FRACTIONAL;
        case IIO_ANGL_VEL:
            /* pi / (180 * 131) rad/s per count, rounded to nanounits. */
            *val = 0;
            *val2 = 133231;
            return IIO_VAL_INT_PLUS_NANO;
        case IIO_TEMP:
            /* 1000 / 333.87 millidegrees Celsius per count. */
            *val = 100000;
            *val2 = 33387;
            return IIO_VAL_FRACTIONAL;
        default:
            return -EINVAL;
        }
    case IIO_CHAN_INFO_OFFSET:
        if (chan->type != IIO_TEMP)
            return -EINVAL;
        /* Temperature = raw / 333.87 + 21 degrees Celsius. */
        *val = 7011;
        *val2 = 270000;
        return IIO_VAL_INT_PLUS_MICRO;
    case IIO_CHAN_INFO_SAMP_FREQ:
        *val = 100;
        return IIO_VAL_INT;
    default:
        return -EINVAL;
    }
}

#define MPU6500_AXIS_CHANNEL(_type, _axis, _index) { \
    .type = (_type), \
    .modified = 1, \
    .channel2 = (_axis), \
    .address = (_index), \
    .info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
    .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
    .info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ), \
}

static const struct iio_chan_spec mpu6500_channels[] = {
    MPU6500_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_X, 0),
    MPU6500_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_Y, 1),
    MPU6500_AXIS_CHANNEL(IIO_ACCEL, IIO_MOD_Z, 2),
    MPU6500_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_X, 0),
    MPU6500_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Y, 1),
    MPU6500_AXIS_CHANNEL(IIO_ANGL_VEL, IIO_MOD_Z, 2),
    {
        .type = IIO_TEMP,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
                              BIT(IIO_CHAN_INFO_SCALE) |
                              BIT(IIO_CHAN_INFO_OFFSET),
        .info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
    },
};

static const struct iio_info mpu6500_iio_info = {
    .read_raw = mpu6500_read_raw,
};

static int mpu6500_probe(struct i2c_client *client)
{
    struct mpu6500_sample sample;
    struct mpu6500_state *state;
    struct iio_dev *indio_dev;
    unsigned int identity;
    int ret;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))  //MPU6500 所在的 I²C 控制器，是否支持普通 I²C 传输
        return dev_err_probe(&client->dev, -EOPNOTSUPP,"adapter needs I2C transfers for regmap\n");

    // Allocate IIO device with private state structure.
    indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*state));
    if (!indio_dev)
        return -ENOMEM;
    state = iio_priv(indio_dev);//get private data pointer from IIO device structure
    
    //state client
    state->client = client;

    //state lock
    mutex_init(&state->lock);

    //state regmap
    state->regmap = devm_regmap_init_i2c(client, &mpu6500_regmap_config);
    if (IS_ERR(state->regmap))
        return dev_err_probe(&client->dev, PTR_ERR(state->regmap),
                             "regmap initialization failed\n");
    // check WHO_AM_I register
    ret = regmap_read(state->regmap, MPU6500_REG_WHO_AM_I, &identity);
    if (ret)
        return dev_err_probe(&client->dev, ret, "WHO_AM_I read failed\n");
    if (identity != MPU6500_WHO_AM_I_VALUE)
        return dev_err_probe(&client->dev, -ENODEV,
                             "WHO_AM_I mismatch: got 0x%02x, expected 0x%02x\n",
                             identity, MPU6500_WHO_AM_I_VALUE);

    /* Registered before IIO: devres unregisters IIO before sleeping hardware. */
    ret = devm_add_action_or_reset(&client->dev, mpu6500_sleep, state);
    if (ret)
        return ret;
    ret = mpu6500_initialize(state);
    if (ret)
        return ret;
    ret = mpu6500_read_sample(state, &sample);
    if (ret)
        return dev_err_probe(&client->dev, ret, "initial sample read failed\n");

    indio_dev->name = "mpu6500_lab";
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->info = &mpu6500_iio_info;
    indio_dev->channels = mpu6500_channels;
    indio_dev->num_channels = ARRAY_SIZE(mpu6500_channels);
    ret = devm_iio_device_register(&client->dev, indio_dev);
    if (ret)
        return dev_err_probe(&client->dev, ret, "IIO registration failed\n");

    dev_info(&client->dev, "probe: bus=%d address=0x%02x WHO_AM_I=0x%02x\n",
             client->adapter->nr, client->addr, identity);
    dev_info(&client->dev, "initialized: accel=+/-2g gyro=+/-250dps rate=100Hz DLPF=3\n");
    dev_info(&client->dev,
             "sample raw: accel=(%d,%d,%d) temp=%d gyro=(%d,%d,%d)\n",
             sample.accel[0], sample.accel[1], sample.accel[2],
             sample.temperature, sample.gyro[0], sample.gyro[1], sample.gyro[2]);
    dev_info(&client->dev, "IIO direct-read device registered\n");
    return 0;
}

/* Lab-specific compatible avoids binding the upstream IIO driver by accident. */
// match for device tree compatible string
static const struct of_device_id mpu6500_of_match[] = {
    { .compatible = "heichi,mpu6500-lab" },
    { }
};
MODULE_DEVICE_TABLE(of, mpu6500_of_match);

// static const struct i2c_device_id mpu6500_ids[] = {
//     { "mpu6500_lab", 0 },
//     { }
// };
// MODULE_DEVICE_TABLE(i2c, mpu6500_ids);

static struct i2c_driver mpu6500_driver = {
    .driver = {
        .name = "mpu6500_lab",
        .of_match_table = mpu6500_of_match,
    },
    .probe = mpu6500_probe,
    // .id_table = mpu6500_ids,
};
module_i2c_driver(mpu6500_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gao Kailong");
MODULE_DESCRIPTION("MPU6500 lab regmap and IIO direct-read driver");
