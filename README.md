# Raspberry Pi 4 Linux Driver Lab

当前工作：MPU6050 I2C / IIO 驱动项目。GPIO 实验暂停，保存在 gpio/。

```text
gpio/                  GPIO LED 实验，源码和原说明完整保留
mpu6050/
  driver/              I2C client 驱动，后续 core/regmap/IIO
  dts/                 MPU6050 Device Tree Overlay
  userspace/           用户态采集程序
  scripts/             构建、加载、卸载脚本
  docs/                接线、寄存器、架构及验证记录
  benchmark/           采样率、丢样、CPU 与抖动测试结果
docs/roadmap.md         整体学习路线
```

各子项目在自己的目录中构建，产物放在对应 build/ 中。
GPIO 操作前先 cd gpio，再按 gpio/README.md 执行。

MPU6050 当前处于硬件验证阶段，下一步见 mpu6050/README.md 和
mpu6050/docs/hardware-validation.md。尚未加入驱动或配置开机加载。
