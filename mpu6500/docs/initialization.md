# 初始化与一帧原始数据读取

本阶段只在 probe 时读取一帧，不使用中断、FIFO、定时器或用户态接口。
100 Hz 表示配置的芯片内部输出更新率，不代表驱动每秒读取 100 帧。

## 执行顺序

1. 检查适配器支持 SMBus byte-data 读写和普通 I2C combined transfer。
2. 读取 WHO_AM_I=0x70，通过后才修改硬件。
3. 向 PWR_MGMT_1 写 0x80 复位，等待 100 ms，清除上一次驱动留下的配置。
4. 按下表写入电源、量程、滤波和分频设置。
5. 等待 100 ms，让时钟和传感器启动。
6. 回读七个配置寄存器，任何读取失败或值不符都使 probe 失败。
7. 从 0x3b 连续读取 14 字节，解析并打印一次原始样本。

初始化/采集失败后尝试写 PWR_MGMT_1=0x40 休眠，并返回原始错误码。
remove 时也请求休眠；休眠写入失败打印警告。身份检查失败不写配置。
复位会覆盖先前的传感器配置，此版本不保存或恢复旧配置。

## 固定配置

| 寄存器 | 地址 | 写入值 | 含义 |
| --- | --- | --- | --- |
| PWR_MGMT_1 | 0x6b | 0x01 | 正常工作，温度启用，自动选择最佳时钟源 |
| PWR_MGMT_2 | 0x6c | 0x00 | 启用加速度和陀螺仪全部六轴 |
| GYRO_CONFIG | 0x1b | 0x00 | ±250°/s，自检关闭，FCHOICE_B=0 |
| ACCEL_CONFIG | 0x1c | 0x00 | ±2g，自检关闭 |
| CONFIG | 0x1a | 0x03 | 陀螺仪 DLPF_CFG=3 |
| ACCEL_CONFIG_2 | 0x1d | 0x03 | 加速度滤波启用，A_DLPFCFG=3 |
| SMPLRT_DIV | 0x19 | 0x09 | 1 kHz / (1 + 9) = 100 Hz |

不在内核使用浮点运算，日志保留原始整数。加速度换算为 raw / 16384 g；
角速度换算为 raw / 131 °/s。温度先保留原始值，后续在用户态转换。

## 数据传输与格式

mpu6500_read_sample 使用 i2c_transfer 一次提交两个消息：
先写寄存器地址 0x3b，再通过 repeated START 读 14 字节。
这不是 SMBus 带长度字节的 block read。成功必须返回 2（完成两个消息）；
负数原样返回，非负但不足两个消息返回 -EIO，不解析不完整数据。

| 缓冲区字节 | 数据 |
| --- | --- |
| 0、1 | 加速度 X |
| 2、3 | 加速度 Y |
| 4、5 | 加速度 Z |
| 6、7 | 温度 |
| 8、9 | 角速度 X |
| 10、11 | 角速度 Y |
| 12、13 | 角速度 Z |

每项是高字节在前的二进制补码：先拼成 16 位数，再转换成 s16。
例如 ff 80 解析成 -128，不能把两个字节直接当成主机字节序的指针读取。

## 编译、查看与重新采样

在 mpu6500/ 目录执行：

```bash
make -C driver
# 已加载旧模块时，保留 overlay 并更新驱动：
sudo rmmod mpu6500_lab
sudo insmod driver/build/mpu6500_lab.ko
readlink -f /sys/bus/i2c/devices/1-0068/driver
sudo dmesg | tail -n 10
```

已加载当前模块时，解绑再绑定会重新初始化并打印一帧：

```bash
echo 1-0068 | sudo tee /sys/bus/i2c/drivers/mpu6500_lab/unbind > /dev/null
echo 1-0068 | sudo tee /sys/bus/i2c/drivers/mpu6500_lab/bind > /dev/null
sudo dmesg | tail -n 5
```

## 实测记录

内核 6.18.50+rpt-rpi-v8，make W=1 编译通过，无编译警告。
模块更新后绑定成功，七个配置值由 probe 全部回读通过。
第一帧日志：

```text
sample raw: accel=(2566,12610,10685) temp=1014 gyro=(867,844,-93)
```

加速度模长约 1.02g；角速度约 (6.62,6.44,-0.71) °/s。
没有确认此时模块是否完全静止，因此不能将这些值直接判为零偏或校准结果。

解绑后 i2cget 实测 PWR_MGMT_1=0x40，确认休眠；其余配置寄存器依次为
PWR_MGMT_2=00、GYRO_CONFIG=00、ACCEL_CONFIG=00、CONFIG=03、
ACCEL_CONFIG_2=03、SMPLRT_DIV=09。重新绑定成功，第二帧为：

```text
sample raw: accel=(2509,12837,10772) temp=1026 gyro=(874,860,-70)
```

最终保留模块和 overlay 加载。未实测持续采样率、运动响应或校准精度，
也未对真实硬件注入通信失败。后续提供 IIO 接口后进行静止和运动测试。

## 参考

- Linux I2C API：https://docs.kernel.org/i2c/writing-clients.html
- TDK MPU6500 产品及资料：https://product.tdk.com/en/search/sensor/mortion-inertial/imu/info?part_no=MPU-6500
- Linux 上游 MPU 驱动：https://github.com/torvalds/linux/tree/master/drivers/iio/imu/inv_mpu6050
