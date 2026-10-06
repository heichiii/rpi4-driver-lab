# Raspberry Pi 4 Linux Driver Lab

当前工作：MPU6500 I2C / IIO 驱动项目。GPIO 实验暂停，保存在 gpio/。

```text
gpio/                  GPIO LED 实验，源码和原说明完整保留
mpu6500/
  driver/              I2C client 驱动，后续 core/regmap/IIO
  dts/                 MPU6500 Device Tree Overlay
  userspace/           用户态采集程序
  scripts/             构建、加载、卸载脚本
  docs/                接线、寄存器、架构及验证记录
  benchmark/           采样率、丢样、CPU 与抖动测试结果
docs/roadmap.md         整体学习路线
```

各子项目在自己的目录中构建，产物放在对应 build/ 中。
GPIO 操作前先 cd gpio，再按 gpio/README.md 执行。

MPU6500 已完成硬件初步验证及第一版 I2C 驱动、设备树绑定。
操作方法见 mpu6500/README.md；下一步为初始化与数据采集。
尚未实现 IIO 或配置开机加载。
