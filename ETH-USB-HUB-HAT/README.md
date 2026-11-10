# ETH/USB HUB HAT（FE1.1S + RTL8152B）—— Quectel Pi M2

USB 2.0 集线器 + 10/100M 以太网扩展板，为开发板增加 1 个 RJ45 网口和
3 个 USB-A 口。40Pin 排针只取 5V/GND，全部数据走板子背面的 Micro-USB
上行口，因此不占用任何 GPIO，也不需要驱动或设备树改动。

本 HAT 没有配套脚本，在 Quectel Pi M2 上已验证可用；原厂资料不随仓库发布。

## 1. 目录内容

| 文件 | 说明 |
|---|---|
| `README.md` | 本文件：硬件结构、接线与测速方法 |

> 原厂原理图与用户手册不随仓库发布。

## 2. 硬件结构

- **FE1.1S**：USB 2.0 四口 HUB。Port1~Port3 引到 USB1~USB3（USB-A），
  Port4 内部接 RTL8152B。
- **RTL8152B**：USB 转 10/100M 以太网控制器，经 TS8121A 网络变压器接 RJ45。
- **5V 电源轨**：上行 Micro-USB 的 VBUS、40Pin 的 Pin2/Pin4、三个 USB-A 的
  VBUS 直连，板上没有二极管或电源开关。
- **指示灯**：PWR、ACT、D1~D3，详见第 5 节。
- 40Pin 上除 5V/GND 外的引脚全部悬空，不与其他 HAT 抢引脚。

## 3. 硬件连接（M2）

### 3.1 接线

| HAT | M2 | 说明 |
|---|---|---|
| 40Pin 排针 | 40Pin 排针 | 直接扣上，仅提供 5V/GND 和机械固定 |
| 背面 `USB`（Micro-USB） | USB Host（USB-A） | USB-A 公转 Micro-USB 公线，数据通路，必须接 |
| `ETH`（RJ45） | 网线 / 交换机 | 10/100M 自适应 |
| `USB1`~`USB3` | USB 设备 | 兼容 USB 2.0 / 1.1 |

步骤：

1. 断开开发板电源；
2. 把 HAT 直接扣到 M2 的 40Pin 排针上（标准 2.54mm 2×20，插法与其他 HAT 相同）；
3. 用 USB-A 公转 Micro-USB 公线，连接 M2 的 USB Host 口与 HAT 背面的 `USB` 口；
4. 接网线、USB 设备，最后上电。

HAT 按树莓派 Zero 尺寸设计（65mm × 30.2mm，3.0mm 固定孔），比标准 HAT
短一截；40Pin 为标准 2.54mm 2×20，插接方式与其他 HAT 完全一致。

### 3.2 供电注意

- 5V 是直通的：**不要再从 40Pin 额外接入 5V 电源**，否则会和主机 USB 的
  VBUS 互相倒灌。
- 只接上行 USB 线时，HAT 也能由主机 VBUS 供电，40Pin 可以不接。
- 3 个 USB 口共享主机端口电流，移动硬盘等大功率设备建议接带源 HUB。
- 接线或更换外设前先断开开发板电源。

## 4. 使用（Linux 免驱）

内核自带 `hub` 和 `r8152` 驱动，插上即可用，无需编译驱动或修改设备树。

```bash
lsusb                          # 应看到 FE1.1S HUB 和 0bda:8152 RTL8152B
dmesg | grep -iE "hub|r8152"   # 确认 HUB 枚举、r8152 注册出网口
ip link                        # 新增一个以太网接口（eth0 / enx...）
ethtool eth0                   # 查看 Link detected 和速率（100Mb/s）
```

接口名以 `ip link` 实际输出为准，下面用 `eth0` 代指。

配置网络按系统自带方式即可：

```bash
udhcpc -i eth0                 # 或 dhclient eth0 / nmcli / 系统网络管理器
ip addr show eth0
ping -I eth0 8.8.8.8
```

Android 镜像需内核开启 `CONFIG_USB_RTL8152`；没有该驱动时 HUB 仍可用，
但网口不会出现。可先用 `lsusb` 确认 `0bda:8152` 是否枚举。

## 5. 指示灯

| 标识 | 含义 | 驱动来源 |
|---|---|---|
| `PWR` | 5V 电源指示 | 5V 经 R21 限流 |
| `ACT` | 网口 link / 活动 | RTL8152B 的 LED0 |
| `D1`~`D3` | USB1~USB3 状态 | FE1.1S 的 LED1/LED2 |

原理图中 D1、D3 共用 HUB 的 LED1 信号，D2 单独用 LED2，因此 D1 和 D3
可能同时亮，属正常现象。

## 6. 验证

本 HAT 已在 Quectel Pi M2 上验证通过。复测时按以下项目判定：

| 验证项 | 命令 | 判定 |
|---|---|---|
| HUB 枚举 | `lsusb` | 出现 FE1.1S HUB 设备 |
| 网卡枚举 | `lsusb` / `ip link` | 出现 `0bda:8152`，并新增以太网接口 |
| 驱动加载 | `dmesg \| grep r8152` | 有 r8152 注册日志 |
| 网口链路 | `ethtool eth0` | `Link detected: yes`，速率 100Mb/s |
| 网络通信 | `udhcpc -i eth0` / `ping` | 能获取地址或 ping 通 |
| USB 扩展 | 依次插 U 盘等设备 | 设备枚举，对应 D1~D3 点亮 |

没有 `ethtool` 时，可用 `cat /sys/class/net/eth0/speed` 和
`cat /sys/class/net/eth0/carrier` 代替（`carrier` 为 1 表示已建立链路）。

## 7. 常见问题

| 现象 | 排查 |
|---|---|
| 三个 USB 口和网口都不识别 | 上行 Micro-USB 线没接，或用了只能充电、没有数据线的线 |
| 只识别 HUB，没有网口 | 内核缺 `r8152` 驱动；`lsusb` 看是否有 `0bda:8152` |
| 网口无 link | 换网线/交换机端口；`ethtool` 看 `Link detected`，检查是否百兆协商 |
| 插 USB 设备后掉线 | 主机端口供电不足，改用带源 HUB 或减少同时接入的设备 |
| D1、D3 同时亮 | 两者共用 HUB 的 LED1 信号，正常 |
| 网口只能跑到 100M | RTL8152B 是百兆控制器，不会协商到千兆，属正常 |

## 8. 参考资料

- `ETH_USB_HUB_HAT-user-manual-cn.pdf`：微雪官方用户手册
- `ETH_USB_HUB_HAT.pdf`：原理图
- Waveshare Wiki：<https://www.waveshare.net/wiki/ETH/USB_HUB_HAT>
- 产品页面：<https://www.waveshare.net/shop/ETH-USB-HUB-HAT.htm>
