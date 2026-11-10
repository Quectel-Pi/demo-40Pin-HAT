# LC29H(XX) GPS/RTK HAT 适配说明 —— Quectel Pi M2（RK3576）

> 本文件对应一键脚本 **`M2_LC29H-GPS-RTK-HAT.sh`**（在 `LC29H(XX)-GPS-RTK-HAT/` 目录下执行）。
> 原厂资料只针对树莓派；本 HAT 走标准 UART，**不需要编译，也不需要安装额外依赖**。

## 1. 目录结构

```
demo-40Pin-HAT/
├── _common.sh                              一键脚本共用函数库（无需单独执行）
└── LC29H(XX)-GPS-RTK-HAT/
    ├── M2_LC29H-GPS-RTK-HAT.sh             ← 本板一键脚本
    └── README_M2.md                        本文件
```

> 原厂例程（`Lc29h_gps_rtk_hat_code/`）、原理图、协议手册与应用笔记不随仓库发布。

## 2. 硬件连接（40Pin）

> **安全提示**：接线或更换外设前请先断开开发板电源（拔掉 USB/电源），接好后再上电，避免带电插拔损坏外设或主板。

### 2.1 直扣 40Pin 后必须做的两件事

| 项 | 做法 | 说明 |
|---|---|---|
| **UART 跳线** | 两个黄色跳线帽**竖插**在丝印 **B** 档（J1 的 3-5、4-6 两对针） | A 档 = 板载 CP2102 USB-UART 直连模组；C 档 = USB-UART 直连 40Pin。横插无效 |
| **GNSS 天线** | L1/L5 双频有源天线接到板载 **IPEX 1 代**座（H3） | 不接天线能上电、TXD 灯也会闪，但永远不会定位 |

上电自检（不用接电脑）：PWR 红灯常亮 → TXD 绿灯约 1 Hz 闪（有 NMEA 输出）→ 定位后 PPS 灯 1 Hz 闪。

### 2.2 40Pin 占用（按原理图 `LC29H(XX)_GPS_RTK_HAT_Sch.pdf` 核对）

| 40Pin | 原理图信号 | HAT 用途 | M2 侧 |
|---|---|---|---|
| **Pin 2 / Pin 4** | `USB_5V` | **唯一供电**（整机 <40 mA @5V） | 5V |
| **Pin 8** | `H_TXD` → 模组 `RXD1` | UART 发送 | `/dev/ttyS1` TX |
| **Pin 10** | `H_RXD` ← 模组 `TXD1` | UART 接收 | `/dev/ttyS1` RX |
| **Pin 12** | `P18` ← `TIMEPULSE` | PPS 秒脉冲输出（可选） | GPIO87 / `gpiochip2` line23 |
| Pin 7 | `P4` → `WAKEUP` | 唤醒（可选，正常不用） | GPIO79 / `gpiochip2` line15 |
| Pin 3 / Pin 5 | `H_SDA` / `H_SCL`（NDC7002N 转到 2.8V） | 模组 I2C（可选） | I2C-3 |
| Pin 13 | `P27` → `WI/RES` | 模组保留脚，官方未开放 | — |
| Pin 1 / Pin 17 | 3.3V | **未使用**（HAT 自己由 5V 转 3.3V / 2.8V） | 不取电、不回灌 |
| Pin 6 / 9 / 14 / 20 / 25 / 30 / 34 / 39 | GND | 共地 | GND |

### 2.3 不用接的部分

- **板上 micro-USB 不要插**：USB VBUS 与 Pin2/4 是同一路 5V，同时供电会回灌 M2；跳线在 B 档时 USB 串口也没有接到模组。
- **轮速 / 方向输入**（板上 24V 光耦端子）：只有 LC29H(BA) 做 DR 航位推算才需要车速脉冲，普通定位不用接。
- **ML1220 电池**：可选。装了热启动约 1 s，不装冷启动约 26 s。
- **I2C（Pin3/Pin5）**：模组 `D_SEL` 悬空，默认就是 UART 模式，I2C 可不接。

电气上无需电平转换：模组 UART 是 2.8V + 二极管钳位，本来就是按 3.3V 树莓派设计的，M2 的 3.3V UART 可直连。

## 3. 快速使用

```bash
cd <Waveshare 所在目录>/LC29H(XX)-GPS-RTK-HAT
chmod +x M2_LC29H-GPS-RTK-HAT.sh    # 从 Windows 拷过去可能丢可执行位
sudo ./M2_LC29H-GPS-RTK-HAT.sh      # 默认抓 8 秒
sudo ./M2_LC29H-GPS-RTK-HAT.sh -t 5 # 指定抓取秒数
```

脚本只读不写：把 `/dev/ttyS1` 配成 115200 8N1，抓取 NMEA 0183，逐条校验校验和并解析 GGA，定位后
再用 `gpiomon` 检查 Pin12 上的 PPS 秒脉冲。

输出示例（本板实测）：

```
==> [2/3] 抓取 NMEA（3 秒）
    已抓取 4256 字节

==> [3/3] 校验 NMEA 并解析定位
    有效语句: 74 条（校验和正确），异常 0 条
    语句类型: GLL VTG GSA GSV GGA RMC
    定位状态: DGPS/SBAS（GGA quality=2），卫星 37 颗
    坐标: 30.123456 N, 120.123456 E（UTC 000000.000）
    海拔: 50.000 m

  ----------------------------------------------
  NMEA 数据（≥5 条）    : PASS
  GGA 定位语句          : PASS
  PPS 秒脉冲            : PASS（3 次上升沿/3 秒）
  ----------------------------------------------

RESULT : PASS - 模块已定位，串口与 PPS 均正常
```

判定规则：

- **PASS**：抓取期内至少 5 条校验和正确的 NMEA，且其中含 GGA。
- **定位状态**只作展示：`quality=0` 表示未定位（室内、天线没接好或冷启动未完成），不影响串口链路判定。
- **PPS**：定位后 3 秒内至少 2 个上升沿算 PASS；未定位或系统没有 `gpiomon` 时显示 SKIP。

GGA 定位质量：`0` 未定位、`1` GPS 单点、`2` DGPS/SBAS、`4` RTK 固定解、`5` RTK 浮动解、`6` 推算定位。

## 4. 本板实测情况

| 项 | 值 |
|---|---|
| 板型 / 系统 | Quectel PI M2 RK3576 Board / Debian 13 |
| 串口设备 | `/dev/ttyS1`（Pin8 TX / Pin10 RX），开机即在，无需 `qpi-config` |
| 模块默认参数 | 115200 8N1，NMEA 0183 输出 1 Hz |
| 抓取结果 | 3 秒 4256 字节 / 74 条语句，校验和异常 0 条 |
| 定位 | GGA `quality=2`（DGPS/SBAS），36～37 颗星，HDOP 0.58～0.72 |
| PPS | `gpiochip2` line23（Pin12 / GPIO87），1 Hz，高电平约 100 ms |
| 依赖 | 仅系统自带 `stty`、`dd`、`awk`（mawk）、`gpiomon`（libgpiod v2），无额外安装 |

## 5. 板型参数如何下发

本 HAT 没有需要下发的 GPIO 参数，脚本顶部只有三个常量：

| 常量 | 值 | 说明 |
|---|---|---|
| `DEV` | `/dev/ttyS1` | M2 40Pin UART（Pin8/Pin10），可用环境变量 `LC29H_UART_DEV` 覆盖 |
| `PPS_CHIP` / `PPS_LINE` | `gpiochip2` / `23` | Pin12 = GPIO87，按 `gpiochip = GPIO/32`、`line = GPIO%32` 换算 |
| `BAUD` | `115200` | 模块出厂默认值 |

例如临时换到别的串口：

```bash
LC29H_UART_DEV=/dev/ttyS3 sudo ./M2_LC29H-GPS-RTK-HAT.sh
```

## 6. 原厂例程能否直接用

原厂只提供树莓派例程，M2 上都不能直接跑，所以一键脚本不依赖它们：

| 原厂例程 | 问题 |
|---|---|
| `c/`（千寻 AGNSS Demo） | `Makefile` 用 `arm-linux-gnueabihf-g++` + `-lwiringPi`；`libs/*.so` 是 **32 位 ARM EABI5**，aarch64 无法链接/加载；还需要千寻位置账号；串口写死 `/dev/ttyS0` + 460800 |
| `python/rtk_rover/` | 依赖 `pyserial`、`pynmeagps`（板上未预装），设备写死 `/dev/ttyS0`，且需要 NTRIP caster 账号 |
| `python/coordinate_converter/` | 通过 `gps3` 读 gpsd socket，不是直接读串口；若已装 gpsd 才可用 |

如果要在 M2 上跑 `rtk_rover`，需自行安装 `pyserial` / `pynmeagps` 并把 `main.py` 里的 `/dev/ttyS0` 改成
`/dev/ttyS1`。协议与命令集见同目录 `Quectel_LC29H&LC79H_Series_GNSS_Protocol_Specification_V1.1.pdf`。

## 7. 排错

| 现象 | 排查 |
|---|---|
| 抓不到任何数据 | 1) 跳线帽是否**竖插在 B 档**（A 档时 UART 走板载 USB-UART）；2) 板上 micro-USB 是否插着；3) 排针是否插紧、PWR 红灯是否常亮 |
| 有数据但全部校验失败 | 模块波特率被改过（默认 115200）；或串口上有其它设备在发数据 |
| 有 NMEA 但 `quality=0` | 天线没接 / 朝内；把天线放到窗外或室外，冷启动最长约 26 s |
| `PPS 秒脉冲: FAIL` | 未定位时 PPS 本来就没有输出；已定位仍 FAIL 时检查 Pin12 是否被其它驱动占用 |
| `PPS 秒脉冲: SKIP` | 系统没有 `gpiomon`（libgpiod v2），或 GPIO line 被占用 |
| `/dev/ttyS1` 不存在 | 确认镜像的 40Pin UART 已使能；本板 Debian 镜像默认可用 |

## 8. Android / 推板说明

Android 上没有 bash，请用 `sh` 执行；并且**必须推送整个 `demo-40Pin-HAT` 目录**（脚本要 source 上一层的
`_common.sh`，只推单个脚本会报错）：

```bash
adb push demo-40Pin-HAT /data/local/tmp/
adb shell
su
sh "/data/local/tmp/demo-40Pin-HAT/LC29H(XX)-GPS-RTK-HAT/M2_LC29H-GPS-RTK-HAT.sh"
```

Linux/Debian 板请把 `demo-40Pin-HAT` 解压到 `/root` 或 `/home/<用户>` 等持久可写目录，不要放在 `/tmp`。
