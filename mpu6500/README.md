# Raspberry Pi 4B MPU6500 I2C / IIO Lab

当前使用 regmap 管理寄存器，完成身份检查、复位、初始化和配置回读，
并注册 IIO 设备。用户态可通过 sysfs 按需读取七个原始通道和换算参数。
尚未实现缓冲区、触发器、中断或用户态采集程序。
GPIO 实验仍暂停在 ../gpio/。

## 硬件

SDA -> BCM GPIO2（物理第 3 脚），SCL -> BCM GPIO3（物理第 5 脚）。
I2C 总线 1，地址 0x68。身份寄存器 0x75 稳定返回 0x70，符合 MPU6500；
不再按 MPU6050 的 0x68 身份值处理。仅凭身份值不保证芯片真伪或测量精度。
VCC/GND 已连接，INT 暂不使用。验证记录见 docs/hardware-validation.md。

## 文件

- driver/mpu6500_i2c.c：I2C driver、regmap、IIO 通道与读回调、匹配表。
- driver/mpu6500.h：当前使用的寄存器与身份值定义。
- driver/Makefile：模块产物 driver/build/mpu6500_lab.ko。
- dts/mpu6500-overlay.dts：I2C1 下地址为 0x68 的设备节点。
- dts/Makefile：产物 dts/build/mpu6500-overlay.dtbo。
- docs/i2c-driver.md：驱动流程、绑定和测试方法。
- docs/initialization.md：初始化顺序、配置表、数据解析与实测记录。
- docs/iio.md：IIO 属性、单位、换算、生命周期和验证记录。
- userspace/、scripts/、benchmark/：预留后续采集、脚本和性能验证。

## 编译与加载

以下命令在 mpu6500/ 目录执行，先确保 I2C1 已启用。

```bash
make -C driver
make -C dts
sudo modprobe regmap-i2c
sudo modprobe industrialio
sudo insmod driver/build/mpu6500_lab.ko
sudo dtoverlay -d "$PWD/dts/build" mpu6500-overlay
readlink -f /sys/bus/i2c/devices/1-0068/driver
sudo dmesg | tail -n 15
```

预期 driver 指向 /sys/bus/i2c/drivers/mpu6500_lab，日志包含：

```text
mpu6500_lab 1-0068: probe: bus=1 address=0x68 WHO_AM_I=0x70
mpu6500_lab 1-0068: initialized: accel=+/-2g gyro=+/-250dps rate=100Hz DLPF=3
mpu6500_lab 1-0068: sample raw: accel=(...) temp=... gyro=(...)
```

也支持先加载 Overlay 后加载模块。不要重复加载已存在的模块或 Overlay。
最终验收后两者已保留加载。IIO name 为 mpu6500_lab；iio:deviceN 的编号
可能变化，读取时按 name 查找。没有自定义 /dev/mpu6500 字符设备。

## 用户态按需读取

```bash
for d in /sys/bus/iio/devices/iio:device*; do
    [ "$(cat "$d/name")" = "mpu6500_lab" ] && IIO_DEV="$d" && break
done
cat "$IIO_DEV/in_accel_x_raw"
cat "$IIO_DEV/in_accel_scale"
cat "$IIO_DEV/in_anglvel_z_raw"
cat "$IIO_DEV/in_anglvel_scale"
cat "$IIO_DEV/in_temp_raw"
cat "$IIO_DEV/in_temp_scale"
cat "$IIO_DEV/in_temp_offset"
```

加速度单位 m/s²，角速度 rad/s，温度 (raw + offset) * scale 为毫摄氏度。
各轴零偏尚未校准，完整说明见 docs/iio.md。

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

解绑/remove 会将芯片置于休眠，WHO_AM_I 仍可读取。若需要手工采集数据，
解绑后先执行 sudo i2cset -y 1 0x68 0x6b 0x01 b，并等待至少 100 ms，
再用 i2ctransfer 读取；重新绑定会复位并重新初始化。
不要用 -f 强制绕过驱动占用。

## 后续

下一步加入 IIO 缓冲区与连续采集。当前 100 Hz 是芯片内部数据更新率，
每次读取 raw 属性都发起一次 14 字节采集，依次读取多个属性不保证同一帧。
设备树使用实验 compatible heichi,mpu6500-lab，区别于上游标准
invensense,mpu6500；不修改系统驱动、不永久 blacklist、不配置开机加载。
