#ifndef MPU6500_LAB_H
#define MPU6500_LAB_H
#include <linux/types.h>

#define MPU6500_REG_SMPLRT_DIV       0x19
#define MPU6500_REG_CONFIG           0x1a
#define MPU6500_REG_GYRO_CONFIG      0x1b
#define MPU6500_REG_ACCEL_CONFIG     0x1c
#define MPU6500_REG_ACCEL_CONFIG_2   0x1d
#define MPU6500_REG_ACCEL_XOUT_H     0x3b
#define MPU6500_REG_PWR_MGMT_1       0x6b
#define MPU6500_REG_PWR_MGMT_2       0x6c
#define MPU6500_REG_WHO_AM_I         0x75
#define MPU6500_WHO_AM_I_VALUE       0x70

#define MPU6500_DEVICE_RESET        0x80
#define MPU6500_SLEEP               0x40
#define MPU6500_CLOCK_AUTO          0x01
#define MPU6500_DLPF_CFG            0x03
#define MPU6500_SAMPLE_RATE_DIV     9 /* 1000 / (1 + 9) = 100 Hz */
#define MPU6500_SAMPLE_BYTES        14

struct mpu6500_sample {
    s16 accel[3];
    s16 temperature;
    s16 gyro[3];
};

#endif
