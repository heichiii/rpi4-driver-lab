# GPIO LED Lab（暂停）

所有命令在 gpio/ 目录中执行；主项目现转向 ../mpu6050/。

当前完成 Phase 5：基于 Platform Driver、Device Tree 和 GPIO descriptor API
控制 LED，通过字符设备 /dev/rpi_gpio 写入 0/1，并读回 GPIO 逻辑状态。

## 文件结构

```text
driver/
  rpi_gpio_led.c              LED platform 驱动
  Makefile
dts/
  rpi-gpio-led-overlay.dts    LED 设备树 Overlay
  Makefile
userspace/
  ledctl.c                   控制 LED 并验证状态读回、EOF
  Makefile
docs/
  roadmap.md                 后续学习路线
```

各目录的编译产物均放在自己的 build/ 中，不提交到 Git。
早期 hello 模块及字符缓冲区回显测试已移除；roadmap 中保留历史学习步骤。

## 接线

BCM GPIO12（排针物理第 32 脚）-> 串联限流电阻 -> LED 正极，
LED 负极 -> GND。设备树采用 active high，加载及移除设备时默认关闭。

## 编译

在项目根目录执行：

```bash
make -C driver
make -C dts
make -C userspace
```

得到：

- driver/build/rpi_gpio_led.ko
- dts/build/rpi-gpio-led-overlay.dtbo
- userspace/build/ledctl

## 加载与使用

```bash
sudo insmod driver/build/rpi_gpio_led.ko
sudo dtoverlay -d "$PWD/dts/build" rpi-gpio-led-overlay
sudo ./userspace/build/ledctl 1
sudo cat /dev/rpi_gpio
sudo ./userspace/build/ledctl 0
```

模块名为 rpi_gpio_led；platform 驱动名仍是 rpi_gpio，
设备树节点仍是 rpi-gpio-lab，compatible 仍是 heichi,rpi-gpio。

写入只接受 0、1 或其后带一个换行；读取返回 0\n 或 1\n。

```bash
echo 1 | sudo tee /dev/rpi_gpio > /dev/null
echo 0 | sudo tee /dev/rpi_gpio > /dev/null
pinctrl get 12
```

GPIO12 被驱动独占时，gpioget 请求该引脚会返回 busy。

## 解绑与重新绑定

```bash
echo rpi-gpio-lab | sudo tee /sys/bus/platform/drivers/rpi_gpio/unbind > /dev/null
echo rpi-gpio-lab | sudo tee /sys/bus/platform/drivers/rpi_gpio/bind > /dev/null
```

解绑关闭输出并释放 GPIO；已打开文件的有效读写返回 ENODEV。

## 卸载与清理

```bash
sudo dtoverlay -r rpi-gpio-led-overlay
sudo rmmod rpi_gpio_led
make -C driver clean
make -C dts clean
make -C userspace clean
```

重启后需要重新加载，本项目未修改启动配置。

## 后续

按 docs/roadmap.md 继续 GPIO 输入、中断、waitqueue 和 poll。
