# Raspberry Pi 4B MPU6500 I2C / IIO Lab

当前完成第一版 I2C client 驱动：设备树创建设备，probe 读取并校验
WHO_AM_I，成功后绑定。尚未实现寄存器配置、数据接口、regmap 或 IIO。
GPIO 实验仍暂停在 ../gpio/。

## 硬件

SDA -> BCM GPIO2（物理第 3 脚），SCL -> BCM GPIO3（物理第 5 脚）。
I2C 总线 1，地址 0x68。身份寄存器 0x75 稳定返回 0x70，符合 MPU6500；
不再按 MPU6050 的 0x68 身份值处理。仅凭身份值不保证芯片真伪或测量精度。
VCC/GND 已连接，INT 暂不使用。验证记录见 docs/hardware-validation.md。

## 文件

- driver/mpu6500_i2c.c：I2C driver、probe/remove、匹配表。
- driver/mpu6500.h：当前使用的寄存器与身份值定义。
- driver/Makefile：模块产物 driver/build/mpu6500_lab.ko。
- dts/mpu6500-overlay.dts：I2C1 下地址为 0x68 的设备节点。
- dts/Makefile：产物 dts/build/mpu6500-overlay.dtbo。
- docs/i2c-driver.md：驱动流程、绑定和测试方法。
- userspace/、scripts/、benchmark/：预留后续采集、脚本和性能验证。

## 编译与加载

以下命令在 mpu6500/ 目录执行，先确保 I2C1 已启用。

```bash
make -C driver
make -C dts
sudo insmod driver/build/mpu6500_lab.ko
sudo dtoverlay -d "$PWD/dts/build" mpu6500-overlay
readlink -f /sys/bus/i2c/devices/1-0068/driver
sudo dmesg | tail -n 15
```

预期 driver 指向 /sys/bus/i2c/drivers/mpu6500_lab，日志包含：

```text
mpu6500_lab 1-0068: probe: bus=1 address=0x68 WHO_AM_I=0x70
```

也支持先加载 Overlay 后加载模块。不要重复加载已存在的模块或 Overlay。
最终验收后两者已保留加载。本阶段不会创建 /dev/mpu6500 或 IIO 设备。

## 卸载

```bash
sudo dtoverlay -r mpu6500-overlay
sudo rmmod mpu6500_lab
```

## 暂时使用 i2c-tools

绑定后驱动独占设备地址，i2cget 会提示 busy。先解绑，再读，最后重新绑定：

```bash
echo 1-0068 | sudo tee /sys/bus/i2c/drivers/mpu6500_lab/unbind > /dev/null
sudo i2cget -y 1 0x68 0x75 b
echo 1-0068 | sudo tee /sys/bus/i2c/drivers/mpu6500_lab/bind > /dev/null
```

不要用 -f 强制绕过驱动占用。

## 后续

下一步加入电源/量程/采样配置与连续 14 字节读取，再迁移到 regmap、IIO。
设备树使用实验 compatible heichi,mpu6500-lab，区别于上游标准
invensense,mpu6500；不修改系统驱动、不永久 blacklist、不配置开机加载。
