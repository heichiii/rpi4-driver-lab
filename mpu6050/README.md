# Raspberry Pi 4B MPU6050 I2C / IIO Driver

## 当前阶段

Phase 8：验证硬件与 I2C 总线。目录已建立，尚未实现驱动。
接线：SDA -> BCM GPIO2（物理第 3 脚），SCL -> BCM GPIO3（物理第 5 脚），
VCC、GND 已连接。供电与上拉需适配树莓派的 3.3V GPIO 电平。
INT 暂不使用。AD0 决定 I2C 地址：低电平 0x68，高电平 0x69。

## 下一步

1. 启用 I2C1，确认 /dev/i2c-1 出现。
2. 检查 0x68 / 0x69 是否响应。
3. 读取 WHO_AM_I（0x75），预期 0x68。
4. 记录硬件验证结果，再学习唤醒、配置和原始数据寄存器。

详细命令见 docs/hardware-validation.md。

## 后续代码命名

- driver/mpu6050_i2c.c：I2C 总线接口、probe/remove。
- driver/mpu6050_core.c：后续寄存器、regmap、IIO 核心。
- driver/mpu6050.h：共享结构和寄存器定义。
- dts/mpu6050-overlay.dts：设备描述。
- userspace/mpu6050_reader.c：后续 IIO 连续采集程序。

这些是后续计划，当前不创建空驱动或虚假的编译目标。
最终数据接口使用 IIO；硬件验证阶段使用 i2c-tools。
