# MPU6050 硬件验证

## 已确认与待确认

SDA 接 BCM GPIO2（物理第 3 脚），SCL 接 BCM GPIO3（物理第 5 脚）。
用户已连接 VCC 和 GND；确认电源及模块 SDA/SCL 上拉是 3.3V 电平。
暂不连接 INT。AD0 低电平使用 0x68，高电平使用 0x69。

本次只读检查：i2c-tools 已安装，i2c_dev 已加载。
/dev/i2c-1 不存在；/boot/firmware/config.txt 中 i2c_arm=on 被注释。
已有 i2c-20 / i2c-21 不是本实验需要的 GPIO2/3 接口。
尚未执行传感器扫描或寄存器读取，尚未修改启动配置。

## 1. 启用 I2C1

```bash
sudo raspi-config
```

进入 Interface Options -> I2C -> Enable，完成后重启：

```bash
sudo reboot
```

重启会断开 SSH，重新连接后：

```bash
ls -l /dev/i2c-1
i2cdetect -l
```

如果 /dev/i2c-1 不存在，先解决总线启用问题。

## 2. 验证地址

只检查 MPU6050 的两个候选地址：

```bash
sudo i2cdetect -y 1 0x68 0x69
```

预期 68 或 69；-- 表示没有响应。UU 表示该地址已由内核驱动占用，
先检查占用的驱动，不使用 -f 强制访问。

## 3. 读取身份寄存器

扫描得到 0x68 时：

```bash
sudo i2cget -y 1 0x68 0x75 b
```

如果地址是 0x69，把命令的地址参数改为 0x69。
WHO_AM_I 的预期值仍然是 0x68，AD0 不改变这个寄存器的值。

## 验收标准

/dev/i2c-1 存在、设备响应地址明确、WHO_AM_I 返回 0x68。
未达到这些条件前，不开始写 I2C client 驱动。

## 后续阶段

先读取 PWR_MGMT_1（0x6B）并理解 sleep、时钟源，再配置采样率和量程，
验证从 ACCEL_XOUT_H（0x3B）开始的连续 14 字节数据。
之后实现 I2C 驱动、Device Tree、regmap、IIO、IRQ 和 triggered buffer。

参考：
https://www.raspberrypi.com/documentation/computers/configuration.html
https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map.pdf
