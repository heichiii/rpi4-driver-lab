# 总体路线

```text
阶段 0：树莓派环境搭建
↓
阶段 1：Linux Kernel Module
↓
阶段 2：字符设备
↓
阶段 3：Platform Driver + Device Tree
↓
阶段 4：GPIO LED / Button
↓
阶段 5：GPIO IRQ + waitqueue + poll
↓
GPIO 项目结束
========================
阶段 6：I2C 基础 + MPU6050 硬件验证
↓
阶段 7：I2C Client Driver
↓
阶段 8：Device Tree + MPU6050 probe
↓
阶段 9：寄存器读写 + 数据采集
↓
阶段 10：regmap
↓
阶段 11：IIO subsystem
↓
阶段 12：MPU6050 Data Ready IRQ
↓
阶段 13：IIO Triggered Buffer
↓
阶段 14：性能分析 / README / 简历
```

建议周期：

```text
GPIO 前置：3~5 天
MPU6050 主项目：2~3 周
整理 + 面试准备：3~5 天
```

---

# Phase 0：把树莓派变成开发机

## 0.1 烧系统

电脑安装 Raspberry Pi Imager。

选择：

```text
Raspberry Pi 4

Raspberry Pi OS Lite 64-bit
```

配置：

```text
hostname: rpi4
username: dev
SSH: enable
Wi-Fi: 填好
```

烧录 TF 卡。

插进树莓派，上电。

---

## 0.2 SSH 登录

先尝试：

```bash
ssh dev@rpi4.local
```

如果不行，通过路由器查 IP：

```bash
ssh dev@192.168.x.x
```

最终做到：

```text
PC
 │
SSH
 ↓
Raspberry Pi
```

---

## 0.3 配置网线直连

推荐：

```text
PC eth0: 192.168.50.1

Pi eth0: 192.168.50.2
```

以后：

```bash
ssh dev@192.168.50.2
```

树莓派 Wi-Fi 保留：

```text
wlan0 → Internet
```

最终：

```text
Internet
   ↑
 Wi-Fi
   │
Raspberry Pi
   │
Ethernet
   │
PC
```

---

## 0.4 安装开发工具

树莓派：

```bash
sudo apt update

sudo apt install -y \
    build-essential \
    git \
    vim \
    gdb \
    device-tree-compiler \
    i2c-tools \
    strace \
    tree
```

检查：

```bash
gcc --version
make --version
dtc --version
```

---

## 0.5 安装 Kernel Headers

检查：

```bash
uname -r
```

再：

```bash
ls /lib/modules/$(uname -r)/build
```

如果存在，就可以编译 out-of-tree module。

这一步是所有后续工作的基础。

---

# Phase 1：Hello Kernel Module

目标：

> 第一次让自己的 C 代码运行在 Linux Kernel 里。

项目：

```text
hello.c
```

实现：

```c
module_init()
module_exit()
```

编译：

```bash
make
```

产生：

```text
hello.ko
```

加载：

```bash
sudo insmod hello.ko
```

查看：

```bash
dmesg | tail
```

卸载：

```bash
sudo rmmod hello
```

你必须理解：

```text
.ko 是什么
insmod 做什么
module_init 什么时候调用
module_exit 什么时候调用
```

### 完成标准

```bash
make
sudo insmod hello.ko
sudo rmmod hello
```

全部正常。

建议时间：

**半天。**

---

# Phase 2：字符设备

目标：

```text
/dev/rpi_gpio
```

实现：

```text
open
read
write
release
```

学习：

```c
struct file_operations
```

以及：

```text
alloc_chrdev_region
cdev_init
cdev_add

class_create
device_create

copy_to_user
copy_from_user
```

最终：

```bash
echo hello > /dev/rpi_gpio

cat /dev/rpi_gpio
```

能得到：

```text
hello
```

再写一个用户态：

```text
char_test.c
```

实现：

```c
open()
write()
read()
close()
```

然后：

```bash
strace ./char_test
```

观察：

```text
openat
write
read
close
```

### 你真正要理解

```text
Userspace
   │
 syscall
   ↓
VFS
   │
file_operations
   ↓
Driver
```

建议时间：

**1 天。**

---

# Phase 3：Linux Device Model

这是第一个关键阶段。

不要永远把设备初始化全塞进：

```c
module_init()
```

改成：

```c
struct platform_driver
```

实现：

```text
probe()
remove()
```

学习：

```text
device
driver
bus
match
probe
```

核心认识：

```text
Device：
板子上有什么硬件

Driver：
怎么驱动这类硬件
```

最终结构：

```text
Linux Platform Bus
     │
Device ←→ Driver
           │
           ↓
         probe()
```

建议时间：

**半天~1 天。**

---

# Phase 4：Device Tree

这是第二个关键阶段。

你要理解：

```text
DTS
 ↓
DTB / DT Overlay
 ↓
Linux Kernel
```

写：

```text
rpi-gpio-overlay.dts
```

例如：

```dts
rpi_gpio {
    compatible = "yourname,rpi-gpio";

    led-gpios = <...>;
    button-gpios = <...>;
};
```

驱动：

```c
static const struct of_device_id ...
```

通过：

```text
compatible
```

实现：

```text
DT node
↓
match
↓
platform_driver
↓
probe
```

至少理解：

```text
compatible
reg
interrupts
gpios
status
phandle
```

不用一次把整个 Device Tree 学完。

建议时间：

**1 天。**

---

# Phase 5：GPIO 输出

现在才接硬件。

准备：

- LED
- 220Ω~1kΩ 电阻
- 杜邦线

例如：

```text
GPIO17 → resistor → LED → GND
```

驱动使用：

```c
devm_gpiod_get()
```

不要把 GPIO number 硬编码在驱动业务逻辑里。

控制：

```bash
echo 1 > /dev/rpi_gpio
```

LED：

```text
ON
```

然后：

```bash
echo 0 > /dev/rpi_gpio
```

LED：

```text
OFF
```

你现在应该能解释：

```text
write()
↓
file_operations
↓
gpiod_set_value()
↓
GPIO controller
↓
LED
```

建议时间：

**半天。**

---

# Phase 6：GPIO Input + IRQ

接一个按钮：

```text
GPIO27
  │
Button
  │
GND
```

第一步先：

```c
gpiod_get_value()
```

确认：

```text
按下 / 松开
```

能检测。

然后：

```c
gpiod_to_irq()
```

注册：

```c
devm_request_irq()
```

按一下按钮：

```bash
cat /proc/interrupts
```

IRQ counter 增长。

### 必须理解

```text
polling
vs
interrupt
```

以及：

```text
GPIO
↓
GPIO controller
↓
IRQ
↓
Linux IRQ subsystem
↓
handler
```

建议时间：

**1 天。**

---

# Phase 7：waitqueue + poll

GPIO 项目最后一关。

加入：

```c
wait_queue_head_t
```

IRQ：

```text
event_pending = 1
wake_up_interruptible()
```

用户：

```c
poll()
```

结果：

```text
./button_test

waiting...
```

按按钮：

```text
button event
```

形成：

```text
Button
↓
IRQ
↓
Driver
↓
waitqueue
↓
poll
↓
Userspace
```

### GPIO 项目到这里结束

不要再扩：

```text
PWM
mmap
复杂 ioctl
复杂 sysfs
```

够了。

你从 GPIO 项目获得：

```text
Kernel Module
Character Device
VFS
Platform Driver
Device Tree
GPIO subsystem
IRQ
waitqueue
poll
```

这些就是你进入 MPU6050 的基础。

---

# Phase 8：MPU6050 硬件验证

现在开始主项目。

连接：

```text
MPU6050         Raspberry Pi 4B

VCC       →     3.3V
GND       →     GND
SDA       →     GPIO2 / SDA
SCL       →     GPIO3 / SCL
```

INT 暂时不要接。

开启 I2C。

然后：

```bash
ls /dev/i2c-*
```

应该看到：

```text
/dev/i2c-1
```

运行：

```bash
sudo i2cdetect -y 1
```

预期：

```text
0x68
```

或者 AD0 改变后：

```text
0x69
```

### 这一阶段只有一个目标

> 证明硬件和 I2C 总线是好的。

如果 `i2cdetect` 看不到 MPU6050，就不要继续写驱动。

建议时间：

**半天。**

---

# Phase 9：理解 MPU6050

把 datasheet 打开。

至少搞懂这些寄存器：

```text
WHO_AM_I

PWR_MGMT_1

SMPLRT_DIV
CONFIG

GYRO_CONFIG
ACCEL_CONFIG

ACCEL_XOUT_H
ACCEL_XOUT_L

TEMP_OUT_H
TEMP_OUT_L

GYRO_XOUT_H
...
```

理解：

```text
power on
↓
sleep
↓
write PWR_MGMT_1
↓
wake
↓
configure
↓
read sensor data
```

先用：

```text
i2cget
i2cset
```

手动验证寄存器。

### 完成标准

你能解释：

```text
WHO_AM_I 为什么存在？

为什么需要 PWR_MGMT_1？

为什么一个 axis 是两个寄存器？

为什么要处理大端组合？
```

建议时间：

**1 天。**

---

# Phase 10：写 I2C Client Driver

现在开始真正的 MPU6050 驱动。

不使用：

```text
platform_driver
```

而是：

```c
struct i2c_driver
```

学习：

```text
i2c_adapter
i2c_client
i2c_driver
```

关系：

```text
BCM2711 I2C Controller
      ↓
I2C Adapter
      ↓
MPU6050 Client
      ↓
Your I2C Driver
```

实现：

```text
probe
remove
```

建议时间：

**1 天。**

---

# Phase 11：MPU6050 Device Tree

写：

```text
mpu6050-overlay.dts
```

类似：

```dts
&i2c1 {
    status = "okay";

    mpu6050@68 {
        compatible = "yourname,mpu6050";
        reg = <0x68>;
    };
};
```

驱动：

```c
of_device_id
```

形成：

```text
Device Tree
↓
I2C core 创建设备
↓
compatible match
↓
i2c_driver
↓
probe()
```

### 验收

```bash
dmesg
```

看到：

```text
mpu6050: probe
```

建议时间：

**半天~1 天。**

---

# Phase 12：第一版寄存器驱动

在：

```text
probe()
```

里：

```text
读取 WHO_AM_I
↓
验证
↓
退出 sleep
↓
配置 accel
↓
配置 gyro
```

然后读取：

```text
ax
ay
az

temperature

gx
gy
gz
```

第一版可以直接使用：

```text
i2c_smbus_*
```

先跑起来。

### 注意

这时可以：

```text
pr_info
```

输出少量数据。

但不要把 `dmesg` 当最终数据接口。

建议时间：

**1 天。**

---

# Phase 13：引入 regmap

然后重构。

设备私有结构：

```c
struct mpu6050_data {
    struct device *dev;
    struct regmap *regmap;
    struct mutex lock;
};
```

初始化：

```c
devm_regmap_init_i2c()
```

使用：

```text
regmap_read
regmap_write
regmap_bulk_read
```

MPU6050 很适合：

```c
regmap_bulk_read()
```

一次读取：

```text
14 bytes
```

即：

```text
accel 6
temp 2
gyro 6
```

这是一个很好的工程点。

建议时间：

**1 天。**

---

# Phase 14：接入 Linux IIO

这是主项目最重要阶段之一。

不要最终做：

```text
/dev/mpu6050
```

而是使用：

```text
Industrial I/O subsystem
```

注册：

```c
struct iio_dev
```

建立 channels：

```text
Accel X
Accel Y
Accel Z

Angular Velocity X
Y
Z

Temperature
```

实现：

```text
read_raw()
```

最终：

```bash
ls /sys/bus/iio/devices/
```

出现：

```text
iio:device0
```

例如：

```bash
cat /sys/bus/iio/devices/iio:device0/in_accel_x_raw
```

能够返回数据。

建议时间：

**2~3 天。**

---

# Phase 15：Scale / Sampling Frequency

下一步实现：

```text
RAW
SCALE
```

例如 accelerometer：

```text
±2g
±4g
±8g
±16g
```

gyro：

```text
±250
±500
±1000
±2000 °/s
```

再支持：

```text
sampling_frequency
```

让用户配置采样率。

你要真正理解：

```text
raw sensor value
+
scale
=
physical value
```

建议时间：

**1~2 天。**

---

# Phase 16：MPU6050 Data Ready IRQ

现在把：

```text
INT
```

接到树莓派某个 GPIO。

Device Tree 描述 IRQ。

MPU6050 配置：

```text
DATA_RDY interrupt
```

然后：

```text
MPU6050 sample ready
↓
INT
↓
Raspberry Pi GPIO
↓
IRQ
↓
Linux Driver
```

推荐使用：

```text
threaded IRQ
```

这里你前面 GPIO 项目学到的 IRQ 正好全部复用。

建议时间：

**1~2 天。**

---

# Phase 17：IIO Triggered Buffer

这是项目的核心亮点。

从：

```text
用户每次读 sysfs
```

升级成：

```text
MPU6050
↓
Data Ready IRQ
↓
IIO Trigger
↓
Triggered Buffer
↓
continuous samples
↓
Userspace
```

数据类似：

```text
timestamp
ax ay az
gx gy gz
```

支持连续采集。

做到这里，项目已经有非常不错的简历价值。

建议时间：

**2~3 天。**

---

# Phase 18：Userspace Reader

自己写：

```text
mpu6050_reader.c
```

读取 IIO buffer。

输出：

```text
timestamp,ax,ay,az,gx,gy,gz
```

例如：

```text
123456789,123,-53,16000,2,8,-5
...
```

保存：

```text
imu.csv
```

后面可以 Python 画：

```text
acceleration vs time
gyro vs time
```

建议时间：

**1 天。**

---

# Phase 19：并发与错误处理

现在回来做工程质量。

考虑：

```text
sysfs read
configuration write
IRQ
buffer read
```

可能同时发生。

使用：

```text
mutex
```

处理：

```text
race condition
```

错误处理至少覆盖：

```text
WHO_AM_I mismatch

I2C read failure

I2C write failure

IRQ registration failure

invalid configuration
```

理解常用：

```text
-ENODEV
-EIO
-EINVAL
-ENOMEM
```

建议时间：

**1 天。**

---

# Phase 20：性能测试

现在项目开始从“能跑”变成“有分析”。

测试：

```text
100 Hz
200 Hz
500 Hz
```

统计：

```text
实际采样率
sample loss
IRQ count
CPU usage
sampling jitter
```

工具：

```text
/proc/interrupts
perf
ftrace
```

尤其关注：

```text
timestamp[n+1] - timestamp[n]
```

例如理论：

```text
100 Hz
→ 10 ms
```

实际：

```text
9.9 ms
10.1 ms
10.0 ms
...
```

可以画 histogram。

建议时间：

**1~2 天。**

---

# Phase 21：整理 GitHub

最终建议仓库：

```text
rpi4-mpu6050-driver/
│
├── README.md
│
├── driver/
│   ├── mpu6050_core.c
│   ├── mpu6050_i2c.c
│   └── Makefile
│
├── dts/
│   └── mpu6050-overlay.dts
│
├── userspace/
│   └── mpu6050_reader.c
│
├── scripts/
│   ├── build.sh
│   ├── load.sh
│   └── unload.sh
│
├── docs/
│   ├── architecture.md
│   ├── device-tree.md
│   ├── iio.md
│   └── interrupt.md
│
└── benchmark/
    ├── results.csv
    └── README.md
```

---

# Phase 22：README 必须包括

不要只写：

```text
make
insmod
```

至少：

```text
1. 项目简介
2. Hardware
3. Wiring
4. Architecture
5. Device Tree
6. Driver Probe Flow
7. IIO channels
8. IRQ workflow
9. Buffer workflow
10. Benchmark
11. Problems encountered
12. Build & Run
```

架构图：

```text
Userspace
   │
IIO interface
   ↓
IIO Core
   ↓
MPU6050 Driver
   │
regmap
   ↓
I2C Core
   ↓
BCM2711 I2C
   ↓
MPU6050
```

IRQ：

```text
MPU6050 DATA READY
       ↓
      INT
       ↓
GPIO Controller
       ↓
Linux IRQ
       ↓
IIO Trigger
       ↓
Triggered Buffer
```

---

# Phase 23：Git Commit 路线

最好形成：

```text
m0: add hello kernel module

m1: add character device interface

m2: convert gpio driver to platform driver

m3: add gpio device tree overlay

m4: add gpio led control

m5: add gpio button interrupt

m6: add waitqueue and poll support

mpu0: verify mpu6050 over i2c

mpu1: add basic i2c driver

mpu2: add device tree binding

mpu3: initialize mpu6050 registers

mpu4: migrate register access to regmap

mpu5: add iio device and channels

mpu6: add scale and sample frequency

mpu7: add data-ready irq

mpu8: add iio triggered buffer

mpu9: add userspace acquisition tool

mpu10: add performance benchmarks
```

这样的 history 很适合展示成长过程。

---

# Phase 24：简历怎么写

GPIO 不需要占太大篇幅。

主项目写：

> **Raspberry Pi 4B MPU6050 Linux IIO Driver**
>
> - 基于 Linux I2C Framework 与 Device Tree 开发 MPU6050 六轴 IMU 驱动，实现设备自动匹配、初始化和配置。
> - 使用 Regmap 封装寄存器访问，通过 bulk read 完成加速度、陀螺仪及温度连续寄存器批量采集。
> - 接入 Linux IIO subsystem，实现 accelerometer、gyroscope、temperature channel 以及 raw/scale/sampling-frequency 接口。
> - 基于 MPU6050 DATA READY 中断和 IIO triggered buffer 实现事件驱动的连续采样，并加入 timestamp。
> - 使用 `/proc/interrupts`、perf、ftrace 对不同采样频率下的 IRQ、CPU 占用及采样抖动进行分析。

GPIO 项目可以浓缩成：

> 实现 Raspberry Pi GPIO Platform Driver，覆盖字符设备、Device Tree、GPIO IRQ、waitqueue 与 poll 事件通知。

---

# 最终你需要掌握的知识树

```text
Linux Kernel
├── Module
├── VFS
│   └── file_operations
├── Device Model
│   ├── device
│   ├── driver
│   └── bus
├── Device Tree
├── GPIO
├── IRQ
├── Waitqueue
├── poll
├── I2C
│   ├── adapter
│   ├── client
│   └── driver
├── regmap
├── IIO
│   ├── channels
│   ├── raw
│   ├── scale
│   ├── trigger
│   └── buffer
├── concurrency
└── perf / ftrace
```

---

# 时间压缩版

如果你目标是尽快找实习，我建议：

| 时间 | 内容 |
|---|---|
| Day 1 | Pi 环境 + hello.ko |
| Day 2 | 字符设备 |
| Day 3 | Platform Driver + DT |
| Day 4 | GPIO LED + Button |
| Day 5 | IRQ + waitqueue + poll |
| Day 6 | MPU6050 接线 + I2C 验证 |
| Day 7 | MPU6050 寄存器 |
| Day 8-9 | I2C Driver + DT |
| Day 10 | regmap |
| Day 11-13 | IIO |
| Day 14-15 | DATA READY IRQ |
| Day 16-18 | IIO triggered buffer |
| Day 19 | userspace reader |
| Day 20 | benchmark + README |

也就是大约 **3 周**。

真正应该重点花时间的是：

**Device Tree → Driver Model → IRQ → I2C Driver → IIO → Triggered Buffer。**

GPIO 只是训练场，**MPU6050 才是你的简历主项目**。