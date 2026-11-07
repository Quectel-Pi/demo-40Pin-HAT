# 2.23inch OLED HAT 适配说明 —— Quectel Pi L1（QCM2290）

> 本文件对应一键脚本 **`L1_2.23inch-OLED-HAT.sh`**（在 2.23inch-OLED-HAT 目录下执行）。
> 其它开发板见同目录：README_M1.md / README_H1.md / README_M2.md。
>
> L1（QCM2290）的 40Pin 映射与 M1 不同：**Pin18 = GPIO_32 → 417，Pin22 = GPIO_80 → 465**
> （TLMM `500000.pinctrl` 基址 385，已用 `40Pin-ctrl pin 18/22 status` 在板上核对），
> 脚本已按此配置下发，无需改代码。

原厂 `2.23inch-OLED-HAT-Code` 只支持树莓派（C 侧依赖 bcm2835 / wiringPi / lgpio，Python 侧依赖
spidev + gpiozero + PIL，SPI 设备与 DC/RST 引脚全部写死成树莓派的 `spidev0.0` / BCM24 / BCM25）。
本 SDK 是适配版：插上 HAT、在 `2.23inch-OLED-HAT/` 目录下执行一条命令即可使用。

> 本 HAT 默认走 **SPI**（SSD1305，128×32）；把背面电阻改焊成 I2C 也能用，但本 SDK 只按 SPI 交付与实测。

---

## 1. 目录结构

```
Waveshare/
├── _common.sh                          一键脚本共用函数库（无需单独执行）
├── install_deps.sh                    离线依赖安装脚本（HAT 共用，装一次即可）
├── packages/                           45 个 arm64 离线 deb 依赖（HAT 共用）
├── 2.23inch-OLED-HAT/
│   ├── L1_2.23inch-OLED-HAT.sh         ← 本板一键脚本（L1 的差异只在这里）
│   ├── M1_2.23inch-OLED-HAT.sh / H1_2.23inch-OLED-HAT.sh / M2_2.23inch-OLED-HAT.sh
│   ├── README_L1.md / README_M1.md / README_H1.md / README_M2.md   四块板各一份
│   ├── QPi_2.23inch_OLED_HAT_SDK/      ← 适配后的 SDK，交付使用这一个
│   │   ├── prebuilt/                   C 例程预编译静态二进制（兜底用，见第 7 节）
│   │   ├── c/                          C 例程源码（已适配）
│   │   │   ├── Makefile
│   │   │   ├── examples/main.c
│   │   │   └── lib/{Config,OLED}/
│   │   └── python/                     Python 例程（已适配）
│   │       ├── test.py                 零依赖例程（推荐，板上直接跑）
│   │       ├── stats.py                原厂系统信息例程（需要 PIL，见第 6 节）
│   │       └── drive/{config.py,SSD1305.py,hat_hw.py,fonts.py}
│   ├── 2.23inch-OLED-HAT-Code.7z       原厂源码压缩包，未做任何改动，仅作参考
│   ├── *.pdf                           用户手册 / 原理图 / SSD1305 datasheet
│   └── flash_download_tool.zip 等      原厂附带工具
└── Environment-Sensor-HAT/             另一块 HAT（结构与本目录类似）
```

## 2. 硬件连接（40Pin 接线）

> **安全提示**：接线或更换外设前请先断开开发板电源（拔掉 USB/电源），接好后再上电，避免带电插拔损坏外设或主板。

> **连接方式**：本板（L1）不能直接扣在 40Pin 卡座上使用，只能通过杜邦线把 HAT 板上的引脚与 40Pin 对应连接；原因待硬件排查中。

### HAT 只用到这 8 根

下表为 L1 的实测接线（GPIO 全局号 = TLMM 基址 385 + GPIO 偏移）：

| 40Pin | 本板（L1）的 GPIO | HAT 侧网络名 | 说明 |
|---|---|---|---|
| **Pin 2 / 4** | 5V | VCC / 5V | **HAT 唯一的电源输入**：5V → U2(XC6206) → 3V3 → U1(SX1308) → VPP |
| 多个 GND 引脚 | GND | GND | 地 |
| Pin 1 / 17 | 3V3 | P_3V3 | **该 HAT 上用不到**：`P_3V3` 是空网络 |
| **Pin 18** | **417**（GPIO_32） | DC | 命令/数据选择 |
| **Pin 19** | SPI0_MOSI | DIN | 数据 |
| **Pin 22** | **465**（GPIO_80） | RST | 复位（低有效） |
| **Pin 23** | SPI0_CLK | CLK | 时钟 |
| **Pin 24** | SPI0_CE0 | CS | 片选（低有效），所以用 `/dev/spidev0.0` |

一句话：这块 HAT 只用 **SPI 那一组 + Pin18/Pin22 两个普通 GPIO**；L1 的 SPI 为 `/dev/spidev0.0`，
Pin18/Pin22 为 GPIO 417/465，由一键脚本下发（第 5 节），不用改代码。

若改用 **CE1**：把 HAT 的 CS 从 Pin24 改接 **Pin26**（SPI0_CE1 → GPIO467 / GPIO_82），
运行一键脚本时在 [3/4] 步选 `1` 即可（对应 `/dev/spidev0.1`）。

## 3. 快速使用

**第一步：确认 40Pin 的 SPI 已经使能，也就是 `/dev/spidevX.Y` 存在。**

```bash
ls /dev/spidev*.*
```

没有的话，用厂商工具打开 40Pin 的 SPI（L1 与 M1 同平台）：

```bash
sudo 40Pin-ctrl test spi        # Qualcomm 系列板（厂商 40Pin 工具）
```

**第二步：安装一次离线依赖（HAT 共用，装过可跳过）：**

```bash
cd <Waveshare 所在目录>
chmod +x install_deps.sh            # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./install_deps.sh
```

**第三步：执行本板一键脚本（先 `cd` 到 `2.23inch-OLED-HAT/`；脚本只检查依赖，不再安装）：**

```bash
cd <Waveshare 所在目录>/2.23inch-OLED-HAT
chmod +x L1_2.23inch-OLED-HAT.sh    # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./L1_2.23inch-OLED-HAT.sh
```

> 本板（QTI Linux）默认就是 root shell，没有 `sudo`，直接执行 `./L1_2.23inch-OLED-HAT.sh`。

脚本依次做四件事：

1. 检查 `packages/` 里的离线依赖是否已装好（缺失时会提示先跑一次 `sudo ./install_deps.sh`；脚本本身不再安装）
2. 校验工具链与 Python 运行环境（例程只用标准库，无需额外安装）
3. 询问片选（`CE0`=Pin24 / `CE1`=Pin26，回车默认 CE0），并打印 SPI 设备与 DC/RST 引脚（参数由脚本下发，见第 5 节）
4. 编译 C 例程生成 `c/main`，随后跑自检（例程显示完即退出，屏幕会一直保持显示内容）

脚本参数：

| 参数 | 作用 |
|---|---|
| （无） | 检查依赖 + 校验 + 识别 + 编译 + 自检 |
| `--no-build` | 跳过 C 例程编译（`c/main` 不存在时仍会取 `prebuilt/` 兜底） |

安装完成后单独运行例程（已是 root 时去掉 `sudo`）：

```bash
sudo OLED_DC_GPIO=417 OLED_RST_GPIO=465 OLED_SPI_CS=0 \
     2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/c/main          # C 例程
sudo OLED_DC_GPIO=417 OLED_RST_GPIO=465 OLED_SPI_CS=0 \
     python3 2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/python/test.py  # Python 例程（纯标准库）
```

两个例程都会自动选 SPI 设备；**DC/RST 必须由环境变量传入**（脚本内已 export，一键脚本打印的运行提示也带上了）。
单独运行时不带这些变量会退回编译期默认值 134/76，屏幕不会亮。上面的 `OLED_SPI_CS=0` 是 CE0（Pin24），
改用 CE1 时改成 `1`。

## 4. 本板情况

下表为在手 L1 板上实测的系统信息（QTI Linux 镜像，`qcm2290-mtp`）：

| 项 | 值 |
|---|---|
| 系统 | QTI Linux arm64，内核 5.15.180，无桌面 |
| Python | 3.10.15（例程只用标准库） |
| 板上编译器 | 无 gcc / make，也没有 dpkg（装不上）→ 脚本自动改用 `prebuilt/` 静态二进制 |
| GPIO 后端 | sysfs（`/sys/class/gpio` 可用） |
| SPI 设备 | `/dev/spidev0.0`（spi0，CE0）；板上另有 `/dev/spidev0.1` |
| DC（Pin18） | **417**（GPIO_32） |
| RST（Pin22） | **465**（GPIO_80） |
| C / Python 例程 | C 与 Python 例程均正常退出（C 走 `prebuilt/`） |

> 参考：环境传感 HAT 已在 L1 上实测跑通（传感器总线 i2c-1 自动命中），
> 说明 L1 的 40Pin 通路与这个 SDK 的适配方式是可行的。

## 5. 板型差异如何下发（源码保持统一）

板型参数（DC/RST 引脚号、gpiochip label、片选）**不在源码里**，由本一键脚本经环境变量下发给例程；
四块板的 SDK 源码完全相同。环境变量接口：

| 环境变量 | 作用 | 本脚本下发的值 |
|---|---|---|
| `OLED_DC_GPIO` | DC（Pin18）引脚号 | **417** |
| `OLED_RST_GPIO` | RST（Pin22）引脚号 | **465** |
| `OLED_SPI_CS` | 片选：按号匹配 `/dev/spidevX.<n>` | 运行脚本时选择：**0** = CE0（Pin24），**1** = CE1（Pin26）；非终端环境默认 0 |
| `OLED_GPIO_CHIP` | chardev 后端下 DC/RST 所在 gpiochip 的 label 关键字 | 不需要（预期有 sysfs） |
| `OLED_SPI_DEV` | 直接指定完整 SPI 节点（优先级最高） | 未设置，自动探测（优先 CS0） |

未设环境变量直接运行源码时，用编译期默认值兜底（M2 的 134/76，仅兜底），并自动做通用探测：
SPI 优先选 CS0 节点、没有则 CS1；GPIO 有 `/sys/class/gpio` 走 sysfs，否则自动改用 gpiochip 字符设备。

## 6. 相对原厂代码改了什么

### C 例程

| 文件 | 改动 |
|---|---|
| `c/lib/Config/DEV_Config.h`、`DEV_Config.c` | 彻底去掉 bcm2835 / wiringPi / lgpio，改为直接操作 `/dev/spidevX.Y`（spidev 裸 `write()`）+ sysfs GPIO。**原因：** trixie 源里根本没有 `liblgpio-dev`，而原厂 `USE_DEV_LIB` 分支强依赖它 |
| `c/lib/Config/DEV_Config.c` | 新增 SPI 设备自动探测：先看环境变量 `OLED_SPI_DEV`，否则 `glob("/dev/spidev*.*")` 并**优先选 CS0 节点**（`/dev/spidevX.0`），没有 CS0 时退回 CS1（`/dev/spidevX.1`）；片选可用 `OLED_SPI_CS=0|1` 指定（匹配 `spidevX.<n>`，找不到会明确报错）；**在终端里直接运行且 CS0/CS1 两个节点都存在时，会提示输入 0/1 选择片选**（回车默认 CS0），非终端环境不询问 |
| `c/lib/Config/DEV_Config.c` | DC/RST 解析：环境变量 `OLED_DC_GPIO` / `OLED_RST_GPIO` > 编译期默认值。**板型预设已从源码移出**，各板的值由一键脚本下发 |
| `c/lib/Config/DEV_Config.c`、`DEV_Config.h` | 新增 **GPIO 字符设备后端**：没有 `/sys/class/gpio` 的板自动改用 `/dev/gpiochipN` + ioctl，纯 C 实现、**不依赖 libgpiod**（预期本板有 sysfs，走原路径） |
| `c/lib/Config/DEV_Config.c` | `DEV_SPI_Write_nByte()` 对 `EIO` 重试 3 次（Qualcomm GENI/GSI 控制器偶发首个传输返回 EIO，见第 8 节） |
| `c/lib/Config/DEV_Config.c` | `DEV_ModuleExit()` 不 unexport GPIO、RST 保持高、DC 保持低。**原因：** unexport 会把引脚拉成输入，屏幕会花/灭 |
| `c/Makefile` | 去掉 `USELIB` 与三个库分支，链接只保留 `-lm`；自动创建 `bin/` 目录 |
| `c/lib/OLED/ssd1305.c`、`ssd1305.h` | **未改动**（初始化序列、字库、滚动逻辑全部沿用原厂） |

原厂 `OLED_CS_0/1`（软件片选）已删除：原厂代码里对 CS 的调用本来就全部被注释掉了，
靠 40Pin 的硬件 CS（Pin24 = CE0）工作。

### Python 例程

| 文件 | 改动 |
|---|---|
| `python/drive/hat_hw.py` | **新增**。纯标准库的硬件层：SPI（`fcntl.ioctl` 设 mode/bits/speed，`os.write` 发数据）、GPIO（有 `/sys/class/gpio` 走 sysfs，否则自动改用字符设备 `ctypes` + ioctl，**不依赖 libgpiod**）、SPI 设备自动选择 |
| `python/drive/config.py` | 重写为调用 `hat_hw`；**去掉 `spidev`、`gpiozero`、`smbus`、`ctypes`**。类名 `RaspberryPi` 与 `gpio_mode` / `digital_write` / `spi_writebyte` / `module_init` / `module_exit` 等接口名保留，方便和树莓派版对照 |
| `python/drive/SSD1305.py` | 修 bug：原厂 `clear()` 里构造了一个局部 `_buffer` 却从未使用，是个**空操作**，现在真正清空显存并刷屏 |
| `python/drive/SSD1305.py` | 初始化序列 `0xDB` 的第二个字节由 `0x08` 改成 `0x3C`（原厂 C 版用 `0x3C`、Python 版用 `0x08`，统一成 C 版） |
| `python/drive/SSD1305.py` | 新增 `pixel()` / `char()` / `string()` / `clear_buffer()`，用内置点阵字库画字，**不再需要 PIL** |
| `python/drive/fonts.py` | **新增**。从 `c/lib/OLED/ssd1305.h` 的 `Font1206[95][12]` 抽出的 8×12 点阵字库（脚本提取，未手工转录） |
| `python/test.py` | **新增**，零依赖例程，效果与 C 版 `c/examples/main.c` 一致 |
| `python/stats.py` | 字体路径由相对路径改成基于 `__file__`；**该例程仍需要 PIL**（本板没有 PIL，用不到可忽略） |

本 HAT 的驱动本身只需要 `fcntl` / `struct` / `os` / `time` 这几个标准库模块，
除 C 编译器外**不需要任何第三方依赖**。

## 7. 离线依赖包与预编译二进制

离线依赖已**统一放在 Waveshare/packages/**（45 个 arm64 deb，HAT 共用），
由根目录的 **install_deps.sh** 用 `dpkg -i` 安装（先检测、已装好的自动跳过；缺的跑两遍，第二遍补齐第一遍因依赖顺序失败的包）；各板一键脚本只检查、不再安装。
本 HAT 只需要其中的 C 工具链与 `libc6`（Python 侧纯标准库，用不到 `python3-smbus2`）。

有意**没有**打包的东西：

| 没打包 | 原因 |
|---|---|
| `liblgpio-dev` / `liblgpio1` | trixie 源里不存在，C 代码已改为直接访问 `/dev/spidevX.Y`，不需要它 |
| `python3-smbus2` / `python3-spidev` / `python3-gpiozero` / `python3-pil` | Python 侧已改成纯标准库，这些一个都不需要 |

### prebuilt/ —— 兜底用的预编译二进制

`prebuilt/main_aarch64_static`（约 924 KB）是一份**静态链接**的 C 例程，内容就是 `c/` 这套源码，
在 Debian 板上用下面这条命令编出来后保存进来的：

```bash
make CFLAGS="-O2 -Wall -static"
```

用途是**兜底**：目标板没有 gcc/make、或者根本装不了（QTI Linux、Yocto 这类非 Debian 系统），
一键脚本会直接把这份二进制复制成 `c/main` 再运行，跳过编译。因为是静态链接，它不依赖目标板的
glibc 版本——实测在 glibc 2.35 的 QTI Linux 板和 glibc 2.41 的 Debian 板上都能正常运行。

`c/` 源码改动后请重新生成这份二进制，否则兜底路径跑的仍然是旧逻辑。

## 8. 注意事项与排错

### 在 L1 上测试的步骤

1. 先确认 40Pin 的 SPI 已使能（第 3 节），`ls /dev/spidev*.*` 能看到节点；
2. 跑 `./L1_2.23inch-OLED-HAT.sh`（非 root 时加 `sudo`），看 [3/4] 步打印的 SPI 设备与 DC/RST；
3. 自检通过后**目视确认屏幕画面**（`Welcome to use` 等 4 行文字 + 滚动）；
4. 若屏幕全黑但自检通过，按下面的「扣板全黑」小节查硬件接触；
5. 若识别/打开 SPI 失败，可直接手动指定重试（root 直接执行；非 root 时把 `sudo` 放在环境变量前面）：
   ```bash
   OLED_SPI_DEV=/dev/spidev0.0 OLED_DC_GPIO=417 OLED_RST_GPIO=465 \
        2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/c/main
   ```

L1 的实测输出为 `SPI: /dev/spidev0.0 ... GPIO: DC=417 RST=465 (sysfs)`；
GPIO 全局号 = TLMM `500000.pinctrl` 基址 385 + 偏移（Pin18 = GPIO_32 → 417，Pin22 = GPIO_80 → 465），
M1 的 423/478 不适用本板。

### 扣板全黑时先查这两处（杜邦能亮 ≠ 扣板能亮）

HAT 上 H1~H6 是六个 3 焊盘 0Ω 跳线：SPI 模式 = 六个跳线全部桥接 pin2–pin3；I2C 模式 = 桥接 pin1–pin2。
RST 是 SPI 四线之外**唯一不经过跳线**的信号，扣板时通路只有「排针 pin22 ↔ 座子 pin22」一处机械接触，
接触不良会导致整屏全黑（不是花屏）。蜂鸣档检查（HAT 单独，不用接板）：

| 测点 | 期望 |
|---|---|
| H7 pin1 (VCC) ↔ 座子 pin2 / pin4 | 通 |
| H7 pin6 (DC) ↔ 座子 pin18 | 通 |
| H7 pin5 (CS) ↔ 座子 pin24 | 通 |
| H7 pin3 (DIN) ↔ 座子 pin19 | 通 |
| H7 pin4 (CLK) ↔ 座子 pin23 | 通 |
| H7 pin7 (RST) ↔ 座子 pin22 | 通（直连，无跳线） |

再上电量：排针 pin2/pin4 对 pin6 应约 5V；HAT 上 C4 两端 / U2 的 Vout 对 GND 应约 3.3V。
**pin2/pin4 没有 5V 时整块屏不会亮**——这块 HAT 不吃排针的 3.3V，只吃 5V。

完整实测案例（M1 上「杜邦能亮、扣板全黑 = 排针 pin22 接触不良」）与三种修法见
[README_M1.md](README_M1.md) 的同名小节。

### SPI 参数

- **Mode 3**（`CPOL=1, CPHA=1`）、8bit、**1 MHz**。原厂 C 版用 bcm2835 的 `SPI_MODE3`、lgpio 的 `lgSpiOpen(..., 0)`、Python 版 `mode=0b11` 都是 Mode 3，本次统一成 Mode 3 + 1 MHz。SSD1305 支持到 10 MHz，这个值很保守。
- 选设备默认优先取 `/dev/spidevX.0`（HAT 的 CS 接 CE0/Pin24）；`OLED_SPI_CS=1` / `OLED_SPI_DEV=/dev/spidevX.Y` 可覆盖。**一键脚本 [3/4] 步会询问选 CE0 还是 CE1（回车默认 CE0，非终端环境默认 CE0）**；例程在终端里直接运行、且 CS0/CS1 两个节点都存在时也会提示输入 0/1。

### 屏幕可见区只有 32 行

本屏是 **128×32**，而驱动 IC 的显存是 132×64（代码里 `WIDTH=132`、`HEIGHT=64`、8 页）。
初始化序列里 `0xA8, 0x1F` 把 MUX 设成 32 行，所以**只有最上面 32 行可见**。
原厂例程按 12px 行高画了 4 行（`y = 0 / 12 / 24 / 36`），第 4 行 `128x32 Pixels` 起始于 y=36，
落在可见区外；例程随后打开了「垂直+水平滚动」，滚动过程中它会移进可见区。
**这里与原厂代码完全一致，未做改动。**

### 权限

- 读写 `/dev/spidev*` 需要 root（或相应的 udev 规则）；`/sys/class/gpio` 的 export / direction / value 同样需要 root。
- 所以两个例程都要 **`sudo`** 运行。

### Qualcomm 板首次传输可能报 EIO

已在 sm6115 板上实测遇到过一次 `/dev/spidevX.0` 的裸 `write()` 返回 `Input/output error`
（GENI/GSI 控制器首个传输偶发失败），对 OLED 来说首个字节正好是初始化命令，丢掉就整屏不亮，
所以 C 与 Python 两侧都对 `EIO` 做了 **3 次重试**。L1 同为 Qualcomm 平台，该保护同样生效；
如果仍然频繁报这个错，先用厂商工具确认 40Pin 的 SPI 通路：

```bash
sudo 40Pin-ctrl test spi        # Qualcomm 系列板
```

### 其他已知现象（都不影响使用）

- **`make` 报「修改时间在未来」**：开发板时钟落后于 PC 导致的，一键脚本已在编译前统一 `touch` 过，直接手动 `make` 时可能看到该警告。
- **一键脚本从 Windows 拷过去后不可执行**：执行 `chmod +x L1_2.23inch-OLED-HAT.sh`。文件本身已是 LF 行尾，无需转换。
- **`stats.py` 报 `No module named 'PIL'`**：这是预期的，本 HAT 的驱动与 `test.py` 都不需要 PIL。要用 `stats.py` 请自行安装 `python3-pil`。
- **`python/drive/config.py` 里的 `RST_PIN` / `DC_PIN` 是 Linux GPIO 号，不是树莓派 BCM 号**：原厂是 `25` / `24`（BCM），本板由脚本下发的 417 / 465。

### 用厂商工具排查 40Pin 时

```bash
# 查 Pin18 / Pin22 的电平状态（正常应为 high 或 low，而不是 unexported/err）
40Pin-ctrl pin 18 status
40Pin-ctrl pin 22 status
```

`40Pin-ctrl pin <N> status` 只覆盖普通 GPIO（本 HAT 的 Pin18/Pin22），SPI 那 4 根（Pin19/21/23/24）
由 SPI 控制器直接驱动，不会出现在这个命令里，显示 `unexported` 是正常的。

## 9. 开发板上做过的持久改动（与 HAT 无关，但请知悉）

已在本板上跑过一键脚本；例程本身**不会在开发板上留下任何持久改动**：
GPIO 只 export 不 unexport（退出后引脚仍保持输出状态、屏幕继续显示），不会写入任何配置文件、不会开机自启。
跑过之后如需清理：

```bash
echo 417 > /sys/class/gpio/unexport     # DC
echo 465 > /sys/class/gpio/unexport     # RST
```