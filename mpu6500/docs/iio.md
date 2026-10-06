# regmap 与 IIO 按需读取

驱动保持单模块 mpu6500_lab.ko，设备树 compatible 仍为 heichi,mpu6500-lab。
固定量程 ±2g/±250°/s，内部更新率 100 Hz，当前只支持 INDIO_DIRECT_MODE。

## 寄存器访问

devm_regmap_init_i2c 创建 8 位寄存器地址、8 位值的 regmap。
REGCACHE_NONE 表示不缓存寄存器，每次读取都访问硬件。初始化使用
regmap_write/regmap_read，样本使用 regmap_bulk_read 从 0x3b 读取 14 字节。
驱动不再直接调用 SMBus API 或自行构造 i2c_msg，因此能力检查只要求
I2C_FUNC_I2C。regmap_bulk_read 成功返回 0，与 i2c_transfer 的消息数不同。

## IIO 通道与属性

不要固定使用 iio:device0，按 name=mpu6500_lab 找设备：

```bash
IIO_DEV=
for d in /sys/bus/iio/devices/iio:device*; do
    [ -f "$d/name" ] || continue
    if [ "$(cat "$d/name")" = mpu6500_lab ]; then
        IIO_DEV="$d"
        break
    fi
done
[ -n "$IIO_DEV" ] && cat "$IIO_DEV/in_accel_x_raw"
```

| 属性 | 含义 |
| --- | --- |
| in_accel_x_raw、in_accel_y_raw、in_accel_z_raw | 三轴有符号加速度原始值 |
| in_accel_scale | m/s² 每计数，约 0.000598550 |
| in_anglvel_x_raw、in_anglvel_y_raw、in_anglvel_z_raw | 三轴有符号角速度原始值 |
| in_anglvel_scale | rad/s 每计数，约 0.000133231 |
| in_temp_raw | 有符号芯片温度原始值 |
| in_temp_scale | 毫摄氏度每计数，约 2.995177763 |
| in_temp_offset | 加在原始温度上的偏移，7011.270000 |
| sampling_frequency | 芯片内部更新率，100 Hz |

这些属性均只读。本阶段不能通过 sysfs 修改量程或采样率。
IIO 的 scale 单位与先前手工用 g、°/s 换算的单位不同：

```text
加速度 m/s² = raw * in_accel_scale
角速度 rad/s = raw * in_anglvel_scale
温度 °C = (raw + in_temp_offset) * in_temp_scale / 1000
```

温度等价于 raw / 333.87 + 21 °C，是芯片内部温度。
加速度 scale 采用 9.80665/16384，角速度采用 pi/(180*131) 并四舍五入到
纳单位。IIO 使用整数、分数或定点返回值，不在内核执行浮点计算。

read_raw 回调处理 RAW、SCALE、OFFSET、SAMP_FREQ；未知请求返回 -EINVAL。
每次 RAW 请求都用 mutex 串行化一次完整 14 字节读取，再取所请求的通道。
因此依次 cat 三轴是三次独立读取，不保证三个值属于同一帧。
需要同步七通道样本时，应在后续实现 IIO buffer；当前没有 buffer、trigger
或二进制样本读取功能。sampling_frequency 不代表用户态轮询速率。

## 资源与解绑

资源登记顺序：IIO 对象及私有状态 -> regmap -> 休眠动作 -> IIO 注册。
设备解绑或 probe 失败时，devm 按相反顺序释放：先注销已注册的 IIO 接口，
再执行休眠动作，随后释放 regmap 和私有状态。不需要显式 remove 回调。
身份校验通过后才登记休眠动作，身份不匹配时不会写传感器。
读取 I2C 失败会从 raw 属性返回错误，不返回之前的缓存样本。

insmod 不自动加载依赖，所以新启动后先执行：

```bash
sudo modprobe regmap-i2c
sudo modprobe industrialio
sudo insmod driver/build/mpu6500_lab.ko
```

Overlay 加载方式不变，见 ../README.md。不要重复加载已有模块。

## 验证记录

6.18.50+rpt-rpi-v8 上 make -C mpu6500/driver W=1 通过，无编译警告。
实际 IIO 设备为 iio:device0，父设备为 I2C1 的 1-0068。
七个 raw 属性、所有 scale/offset、sampling_frequency 均可读。
连续三次读取数据更新，示例：

```text
accel=(2377,12655,11011) gyro=(822,921,-63) temp_raw=1298
accel_scale=0.000598550 anglvel_scale=0.000133231
温度约 24.89°C，加速度模长约 1.034g
```

角速度仍有偏移，未确认静止和校准；依次读取各轴不是同步快照。
四线程并发执行共 40 次 raw 读取，全部成功。
unbind 后 IIO 设备消失，i2cget 实测 PWR_MGMT_1=0x40；bind 后设备重新出现，
raw 读取恢复。最终保留模块和 Overlay 加载。未实测缓冲区、运动响应或校准。

参考：
- IIO core：https://docs.kernel.org/driver-api/iio/core.html
- MPU6500 数据手册：https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6500-datasheet2.pdf
