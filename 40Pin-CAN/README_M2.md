# 40Pin-CAN：SN65HVD230 CAN Board × Quectel Pi M2

本目录提供 Waveshare SN65HVD230 CAN Board 在 Quectel Pi M2（RK3576）上的接线说明与单板回环测试脚本；
原厂资料与 LPC1768 / STM32 示例代码不随仓库发布。

## 1. 目录内容

| 文件 | 说明 |
|---|---|
| `M2_can-loopback.sh` | M2 单板 CAN 回环测试脚本 |
| `README_M2.md` | 本文件：接线、双板测试与排错 |

> 原厂原理图、数据手册与示例代码压缩包不随仓库发布。

## 2. 硬件说明

Waveshare SN65HVD230 CAN Board（SKU 3945）是一块 **3.3V CAN 收发器板**，只负责把控制器的 TTL 信号转换成差分 CANH/CANL，不包含 CAN 协议控制器。

- 供电：3.3V，逻辑电平 3.3V
- 速率：最高 1 Mbps，经典 CAN；CAN FD 数据段请限制在 1 Mbps 以内
- 板载 120Ω 终端电阻（CANH-CANL 之间）
- Rs 引脚经 10K 电阻接地，工作在高速模式
- 4 针接口 P1：1=CAN_TX，2=CAN_RX，3=3.3V，4=GND
- 差分侧：CANH、CANL 两个端子（板上丝印有标注）

> TJA1050 是 5V 器件，不能直接接到 M2 的 3.3V 引脚，否则可能损坏引脚。

## 3. 与 M2 40Pin 接线

> **安全提示**：接线或更换外设前请先断开开发板电源（拔掉 USB/电源），接好后再上电，避免带电插拔损坏外设或主板。

| M2 40Pin | 信号 | 收发器 P1 |
|---|---|---|
| Pin16 | CAN0_TX（GPIO4_A4 / GPIO132） | P1-1 CAN_TX |
| Pin18 | CAN0_RX（GPIO4_A6 / GPIO134） | P1-2 CAN_RX |
| Pin17 | 3V3 | P1-3 3.3V |
| Pin9 | GND | P1-4 GND |

```text
       M2                           SN65HVD230
  Pin16 (CAN0_TX) ──────────────── P1-1 CAN_TX
  Pin18 (CAN0_RX) ──────────────── P1-2 CAN_RX
  Pin17 (3V3)     ──────────────── P1-3 3.3V
  Pin9  (GND)     ──────────────── P1-4 GND

                                   CANH ──┬── 总线 CANH
                                          │
                                         120Ω（板载）
                                          │
                                   CANL ──┴── 总线 CANL
```

注意：

- **只能接 3.3V**，不要接 Pin2/Pin4 的 5V。
- 2.23inch OLED HAT 把 Pin18 当 DC 使用，**不能和本 CAN 板同时占用 Pin18**。
- 40Pin 上 Pin1、Pin17 都是 3V3；GND 可用 Pin6/9/14/20/25/30/34/39。

### 双板总线连接

两块 M2 各接一个 SN65HVD230，差分侧按下面连接：

```text
M2 #1 + 收发器 #1                 M2 #2 + 收发器 #2
CANH ●───────────────────────────● CANH
CANL ●───────────────────────────● CANL
GND  ●───────────────────────────● GND（必须共地）
       120Ω（板载）                    120Ω（板载）
```

- CANH 接 CANH，CANL 接 CANL，建议使用双绞线。
- 两块板的 GND 必须连通，否则会出现“能收到数据，但发送方错误计数持续增长”。
- 两个节点时两端各一个 120Ω，总线阻抗约 **60Ω**；不要再外加电阻。
- 三个及以上节点时，只保留总线两端的 120Ω，中间节点需要去掉板载电阻。

## 4. 使能 can0

Linux 镜像默认未启用 CAN，需要先打开 DT overlay 并重启：

```sh
qpi-config can0 enable
reboot
```

重启后确认设备节点和引脚复用：

```sh
ip -br link show can0
cat /sys/kernel/debug/pinctrl/*/pinmux-pins | grep can0
```

期望输出：

```text
can0             DOWN           <NOARP,ECHO>
Pin 132 (gpio4-4): 2ac00000.can  function can0 group can0m2-pins   ← TX
Pin 134 (gpio4-6): 2ac00000.can  function can0 group can0m2-pins   ← RX
```

Android 镜像如果设备树中 `&can0` 已置 `okay`，直接使用 `can0` 即可，不需要 `qpi-config`。

## 5. 接口配置

两块板（或回环测试的本板）使用完全一致的参数：

```sh
ip link set can0 down
ip link set can0 type can bitrate 500000 dbitrate 1000000 fd on restart-ms 100
ip link set can0 txqueuelen 1000
ip link set can0 up
ip -details link show can0
```

正常状态：

```text
can0: <NOARP,UP,LOWER_UP,ECHO> ... state UP
  can <FD> state ERROR-ACTIVE (berr-counter tx 0 rx 0) restart-ms 100
        bitrate 498322 sample-point 0.865
        dbitrate 1003378 dsample-point 0.729
```

说明：

- `rk3576_canfd` 要求同时提供数据域位时序 `dbitrate`。只给 `bitrate` 时 `up` 会报 `RTNETLINK answers: Invalid argument`，内核日志为 `incorrect/missing data bit-timing`。
- 实际时钟为 148.5 MHz，配置 500k/1M 后实际值为 498322/1003378；两块 M2 的误差相同，互通没有问题。
- SN65HVD230 最高 1 Mbps，数据段不要超过 1 Mbps。

## 6. 测试方式

### 6.1 双板对接测试

需要 2 块 M2 + 2 个 SN65HVD230，按第 3 节接线并共地。

两块板分别执行第 5 节配置。下面命令使用 `candump/cansend`，需要系统已安装 can-utils；Android 镜像默认没有，可先推送静态 can-utils，或改用 cantool 收发。

```sh
# B：先起监听
candump -t d can0

# A：另开终端发 10 帧
for i in 1 2 3 4 5 6 7 8 9 10; do
    cansend can0 123#1122334455667788
    sleep 0.1
done

# A、B 两端查看状态
ip -details -s link show can0
```

判定标准：

| 检查项 | 通过 |
|---|---|
| 发送方 `berr-counter tx` | 保持 0 |
| 接收方 `RX: packets` | 随发帧递增 |
| 接收方 `RX: errors` | 保持 0 |
| 两端 `bitrate` / `dbitrate` | 数值一致 |
| 数据内容 | ID、长度、字节完全一致 |
| 断电测 CANH-CANL | 约 60Ω |

### 6.2 单板回环测试

只需要 1 块 M2 + 1 个 SN65HVD230，不需要第二块板和第二路总线。直接运行脚本：

```sh
sudo ./M2_can-loopback.sh          # 控制器回环
sudo ./M2_can-loopback.sh silent   # 静默回环，不依赖对端 ACK
```

Android 上运行：

```sh
adb push M2_can-loopback.sh /data/local/tmp/
adb shell
su
sh /data/local/tmp/M2_can-loopback.sh          # 控制器回环
sh /data/local/tmp/M2_can-loopback.sh silent   # 静默回环
```

脚本会：

1. 配置 `can0` 为回环模式；
2. 启动接收端并发送 5 帧 `0x333`；
3. 通过 `rx_packets` 增量判断是否收到，输出 `RESULT: PASS / FAIL`；
4. 退出时自动恢复普通双板模式（`loopback off`）。

收发工具会按顺序选择：

1. 系统有 `candump/cansend` → 使用 can-utils；
2. Android 没有 can-utils → 使用 `/data/local/tmp/cantool`；
3. 两者都没有 → 脚本提示先安装或推送工具。

回环测试能说明：

- 控制器、驱动、SocketCAN、帧格式正常；
- Pin16/Pin18 的 pinmux 配置正确；
- 静默回环不依赖第二个节点的 ACK；
- 本板回环时帧仍会驱动总线，对端已接好时也能收到，可顺带验证 TX 引脚、收发器发送侧和 CANH/CANL 连线。

回环测试不能说明：

- 真实 ACK、优先级仲裁和多节点通信；
- 收发器 RX 侧、终端电阻匹配、共模/长线/抗干扰；
- 两个独立节点的时钟容差、错误计数和 bus-off 恢复。

> 本驱动回环时，本地接收端可能看到每帧出现两次（驱动回环 + 本地回显），计数时注意。

## 7. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| `ip link set can0 up` 报 `Invalid argument`，`dmesg` 显示 `incorrect/missing data bit-timing` | 只配了 `bitrate`，没配 `dbitrate` | 补上 `dbitrate 1000000 fd on` |
| 能收到数据，但发送方 `berr-counter tx` 持续增长 | 两块板未共地 | 把两板 GND 连通 |
| 一端 `tx 256`，另一端 `rx` 完全不动 | CANH/CANL 接反或断线 | 对调 CANH/CANL，检查通断 |
| 两端 `rx` 都有计数但 `errors` 增长 | 两端波特率或数据域位时序不一致 | 用 `ip -details link show can0` 对比 |
| 断电只量到 120Ω，不是 60Ω | 另一端收发器未供电或未接 | 检查另一端 P1-3 3V3、P1-4 GND |
| `No buffer space available` | `txqueuelen` 仍是 10 | `ip link set can0 txqueuelen 1000` |
| FD 帧出现重复帧或尾部补零 | `##` 后的标志位写成 0，被当成经典帧 | FD 帧必须写 `##1` |
| Android 提示 `candump: inaccessible or not found` | 镜像没有 can-utils | 使用本目录脚本（需先有 `/data/local/tmp/cantool`），或把静态 can-utils 推到 `/data/local/tmp` |

## 8. 参考

- [M2_can-loopback.sh](M2_can-loopback.sh)：本目录回环测试脚本
- `SN65HVD230-CAN-Board-Schematic.pdf` / `SN65HVD230-CAN-Board_Datasheets.pdf`：收发器板原理图与数据手册
- [Quectel_Pi_40Pin测试](https://quectel.feishu.cn/wiki/RQT0wumwyiwyFvkJnxTc74ysnxe)
