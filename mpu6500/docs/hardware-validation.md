# 硬件验证记录

原模块按 MPU6050 购入/命名，但 WHO_AM_I 稳定为 0x70，符合 MPU6500。
项目因此改名 mpu6500；不将身份值不符简单忽略。型号仍可用芯片丝印核对。

## 接线与总线

SDA -> BCM GPIO2（物理第 3 脚），SCL -> BCM GPIO3（物理第 5 脚）。
VCC/GND 已连接，供电和 SDA/SCL 上拉应适配 3.3V 电平；INT 暂未使用。
/dev/i2c-1 已出现，地址 0x68 的读取成功。

## 用户提供的寄存器结果

- WHO_AM_I (0x75)：多次读取 0x70。
- PWR_MGMT_1 (0x6B)：0x01，未休眠。
- PWR_MGMT_2 (0x6C)：0x00，六个轴未禁用。
- GYRO_CONFIG (0x1B)：0x00，量程 +/-250 deg/s。
- ACCEL_CONFIG (0x1C)：0x00，量程 +/-2 g。
- 从 0x3B 连续读 14 字节成功，多个样本有明显变化。

高字节在前，每轴组合为有符号 16 位补码：
加速度 g = raw / 16384；角速度 deg/s = raw / 131。
14 字节依次为 AX、AY、AZ、温度、GX、GY、GZ，各占两个字节。
这完成通信和原始数据的初步验证，未完成零偏校准、精度或采样率测试。

## 手动复查

先确保设备未被内核驱动绑定（解绑方法见 ../README.md）：

```bash
sudo i2cget -y 1 0x68 0x75 b
sudo i2cget -y 1 0x68 0x6b b
sudo i2cget -y 1 0x68 0x6c b
sudo i2ctransfer -y 1 w1@0x68 0x3b r14
```

当前驱动已绑定，直接运行 i2cget 会 busy，不使用 -f 绕过。
验证加载顺序时，上游 IIO 驱动曾短暂绑定，可能改变运行配置；当前第一版
实验驱动只验证身份，不保证电源/量程仍等于上述早期结果。
下一阶段应明确初始化需要的电源、量程和采样配置。

参考：
https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Register-Map2.pdf
https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6500-datasheet2.pdf
