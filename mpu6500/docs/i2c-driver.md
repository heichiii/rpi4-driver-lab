# 第一版 I2C client 驱动

## 流程

```text
设备树 &i2c1 / imu@68
  -> I2C core 创建 i2c_client（1-0068）
  -> compatible = heichi,mpu6500-lab 匹配
  -> mpu6500_probe(client)
  -> 检查 SMBus byte-data 读取能力
  -> i2c_smbus_read_byte_data(client, 0x75)
  -> 0x70：成功绑定；读失败或身份不符：返回负错误码
```

client 表示一个具体 I2C 设备；client->adapter 指向所在总线，
client->addr 是从设备地址。它由 I2C core 创建，驱动不自行扫描总线。
与 GPIO 的 platform_driver 不同，本例使用 struct i2c_driver。

of_device_id 表用于设备树匹配；i2c_device_id 表用于传统 I2C 名称匹配；
MODULE_DEVICE_TABLE 导出对应模块别名。module_i2c_driver 包装注册与注销。

## Overlay

& i2c1 的 phandle 引用通过 -@ 编译产生的 fixups 在加载时解析。
父总线 #address-cells=1、#size-cells=0，故 reg=<0x68> 代表一个 I2C 地址，
不是内存地址或寄存器偏移。imu@68 的单元地址与 reg 对应。
I2C1 必须先启用；Overlay 不重复修改父总线 status，以免产生动态移除警告。

使用 heichi,mpu6500-lab 而非仅 heichi,mpu6500，是因为 I2C core 会从
compatible 提取设备类型，系统驱动可能按 mpu6500 名称回退匹配。
-lab 后缀避免这种冲突，因此 Overlay 先加载也不会被系统 IIO 驱动接管。
这是学习项目的私有 compatible，不是上游标准 binding。

## 第一版的边界

probe 只读取身份，无配置写入，无设备私有状态或动态资源。
remove 记录解绑，无需释放尚未申请的资源。
模块加载成功不代表设备 probe 成功，应检查 sysfs 的 driver 链接与日志。
本阶段没有用户态采集接口，后续使用 IIO，不新建字符设备。

## 验证记录

在 6.18.50+rpt-rpi-v8 上驱动与 DTS 编译通过。
验证：先驱动/先 Overlay 两种加载顺序、WHO_AM_I=0x70、绑定关系、
unbind/bind、Overlay 移除后 1-0068 消失、模块卸载/重新加载。
身份不符和 I2C 读取失败均有明确错误处理，未对真实芯片注入故障。
开发验证过程中系统自带 inv-mpu6050-i2c 曾按名称回退绑定，随后已移除
该设备、卸载该总线驱动，修正实验 compatible；最终由本实验驱动绑定。

参考：https://docs.kernel.org/i2c/writing-clients.html
