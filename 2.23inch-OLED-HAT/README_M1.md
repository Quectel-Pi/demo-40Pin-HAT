# 2.23inch OLED HAT 适配说明 —— Quectel Pi M1（SM6115，QTI Linux）

> 本文件对应一键脚本 **`M1_2.23inch-OLED-HAT.sh`**（在 2.23inch-OLED-HAT 目录下执行）。
> 其它开发板见同目录：README_L1.md / README_H1.md / README_M2.md。

原厂 `2.23inch-OLED-HAT-Code` 只支持树莓派（C 侧依赖 bcm2835 / wiringPi / lgpio，Python 侧依赖
spidev + gpiozero + PIL，SPI 设备与 DC/RST 引脚全部写死成树莓派的 `spidev0.0` / BCM24 / BCM25）。
本 SDK 是适配版，**已在本板实测跑通**：插上 HAT、在 `2.23inch-OLED-HAT/` 目录下执行一条命令即可使用。

本板是 **QTI Linux（Yocto 类，非 Debian）**，没有 dpkg / 编译器，C 例程由 `prebuilt/` 兜底；
Python 例程纯标准库、开箱即用（见第 7、8 节）。

> 本 HAT 默认走 **SPI**（SSD1305，128×32）；把背面电阻改焊成 I2C 也能用，但本 SDK 只按 SPI 交付与实测。

---

## 1. 目录结构

```
Waveshare/
├── _common.sh                          一键脚本共用函数库（无需单独执行）
├── install_deps.sh                    离线依赖安装脚本（HAT 共用，装一次即可）
├── packages/                           45 个 arm64 离线 deb 依赖（HAT 共用）
├── 2.23inch-OLED-HAT/
│   ├── M1_2.23inch-OLED-HAT.sh         ← 本板一键脚本（M1 的差异只在这里）
│   ├── L1_2.23inch-OLED-HAT.sh / H1_2.23inch-OLED-HAT.sh / M2_2.23inch-OLED-HAT.sh
│   ├── README_L1.md / README_M1.md / README_H1.md / README_M2.md   四块板各一份
│   ├── QPi_2.23inch_OLED_HAT_SDK/      ← 适配后的 SDK，交付使用这一个
│   │   ├── prebuilt/                   C 例程预编译静态二进制（本板靠它，见第 7 节）
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

> **连接方式**：本板（M1）不能直接扣在 40Pin 卡座上使用，只能通过杜邦线把 HAT 板上的引脚与 40Pin 对应连接；原因待硬件排查中。

### HAT 只用到这 8 根

| 40Pin | 本板（M1）的 GPIO | HAT 侧网络名 | 说明 |
|---|---|---|---|
| **Pin 2 / 4** | 5V | VCC / 5V | **HAT 唯一的电源输入**：5V → U2(XC6206) → 3V3 → U1(SX1308) → VPP |
| 多个 GND 引脚 | GND | GND | 地 |
| Pin 1 / 17 | 3V3 | P_3V3 | **该 HAT 上用不到**：`P_3V3` 是空网络 |
| **Pin 18** | **423** | DC | 命令/数据选择 |
| **Pin 19** | SPI0_MOSI | DIN | 数据 |
| **Pin 22** | **478** | RST | 复位（低有效） |
| **Pin 23** | SPI0_CLK | CLK | 时钟 |
| **Pin 24** | SPI0_CE0 | CS | 片选（低有效），所以用 `/dev/spidev0.0` |

一句话：这块 HAT 只用 **SPI 那一组 + Pin18/Pin22 两个普通 GPIO**；本板 SPI 落在
`/dev/spidev0.0`、Pin18/Pin22 落在 GPIO 423/478——这两个值由一键脚本下发（第 5 节），不用改代码。

## 3. 快速使用

**第一步：确认 40Pin 的 SPI 已经使能，也就是 `/dev/spidevX.Y` 存在。**

```bash
ls /dev/spidev*.*
```

没有的话，用厂商工具打开 40Pin 的 SPI：

```bash
sudo 40Pin-ctrl test spi        # Qualcomm 系列板（厂商 40Pin 工具）
```

**第二步：安装一次离线依赖（HAT 共用，装过可跳过）：**

```bash
cd <Waveshare 所在目录>
chmod +x install_deps.sh            # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./install_deps.sh
```

> 本板没有 dpkg，`install_deps.sh` 会自动整段跳过、不装任何东西（也绝不能装，见第 7、8 节）。

**第三步：执行本板一键脚本（先 `cd` 到 `2.23inch-OLED-HAT/`；脚本只检查依赖，不再安装）：**

```bash
cd <Waveshare 所在目录>/2.23inch-OLED-HAT
chmod +x M1_2.23inch-OLED-HAT.sh    # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./M1_2.23inch-OLED-HAT.sh
```

脚本依次做四件事：

1. 检查 `packages/` 里的离线依赖是否已装好——**本板没有 dpkg，检查直接跳过**（install_deps.sh 在本板同样不安装）
2. 校验工具链与 Python 运行环境（本板没有 gcc/make，C 例程走 `prebuilt/` 兜底）
3. 询问片选（`CE0`=Pin24 / `CE1`=Pin26，回车默认 CE0），并打印 SPI 设备与 DC/RST 引脚（参数由脚本下发，见第 5 节）
4. 部署 C 例程（`prebuilt/` 静态二进制 → `c/main`），随后跑自检（例程显示完即退出，屏幕会一直保持显示内容）

脚本参数：

| 参数 | 作用 |
|---|---|
| （无） | 检查依赖 + 校验 + 识别 + 准备 C 例程 + 自检 |
| `--no-build` | 跳过 C 例程编译（本板本来就没编译器，无影响） |

安装完成后单独运行例程：

```bash
sudo OLED_DC_GPIO=423 OLED_RST_GPIO=478 OLED_SPI_CS=0 \
     2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/c/main          # C 例程（prebuilt 兜底）
sudo OLED_DC_GPIO=423 OLED_RST_GPIO=478 OLED_SPI_CS=0 \
     python3 2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/python/test.py  # Python 例程（纯标准库）
```

两个例程都会自动选 SPI 设备；**DC/RST 必须由环境变量传入**（一键脚本内已 export，结尾提示也带上了）。
单独运行不带这些变量会退回编译期默认值 134/76。`OLED_SPI_CS=0` 是 CE0（Pin24），改接 CE1 时改成 `1`。

## 4. 本板实测情况

| 项 | 值 |
|---|---|
| `/proc/device-tree/model` | `Qualcomm Technologies, Inc. BENGAL IDP nopmi` |
| 系统 | QTI Linux full reference stack（Yocto 类，**非 Debian**） |
| 内核 / Python | 5.15.180 / 3.10.15 |
| 板上编译器 | **无，且装不上**（没有 dpkg/apt）→ C 例程自动走 `prebuilt/` |
| GPIO 后端 | sysfs |
| SPI 设备 | **`/dev/spidev0.0`**（spi0 = `4a80000.spi`） |
| DC（Pin18） | **GPIO 423** |
| RST（Pin22） | **GPIO 478** |
| C 例程 | 自动改用 `prebuilt/` 里的预编译二进制 |
| Python 例程 | 通过 |

实测自检输出：

```
SPI: /dev/spidev0.0  mode3  8bit  1000000Hz
GPIO: DC=423  RST=478  (sysfs)
退出码 0
```

自检能验证到的范围是「SPI 与两个 GPIO 的所有读写都成功、初始化序列与 1056 字节显存数据全部写出」；
**屏幕上的画面需要目视确认**（例程内容：`Welcome to use` / `Waveshare Electronic` /
`2.23inch OLED` / `128x32 Pixels`，并打开上下+左右滚动）。

> HAT 的 CS 默认接 **Pin24（CE0）**，用 `/dev/spidev0.0`；若改接 Pin26（CE1），
> 一键脚本 [3/4] 步选 `1` 即可（走 `/dev/spidev0.1`）。

## 5. 板型差异如何下发（源码保持统一）

板型参数（DC/RST 引脚号、gpiochip label、片选）**不在源码里**，由本一键脚本经环境变量下发给例程；
四块板的 SDK 源码完全相同。环境变量接口：

| 环境变量 | 作用 | 本脚本下发的值 |
|---|---|---|
| `OLED_DC_GPIO` | DC（Pin18）引脚号 | **423** |
| `OLED_RST_GPIO` | RST（Pin22）引脚号 | **478** |
| `OLED_SPI_CS` | 片选：按号匹配 `/dev/spidevX.<n>` | 运行脚本时选择：**0** = CE0（Pin24），**1** = CE1（Pin26）；非终端环境默认 0 |
| `OLED_GPIO_CHIP` | chardev 后端下 DC/RST 所在 gpiochip 的 label 关键字 | 不需要（本板有 sysfs） |
| `OLED_SPI_DEV` | 直接指定完整 SPI 节点（优先级最高） | 未设置，自动探测（优先 CS0） |

未设环境变量直接运行源码时，用编译期默认值兜底（M2 的 134/76，仅兜底），并自动做通用探测：
SPI 优先选 CS0 节点、没有则 CS1；GPIO 有 `/sys/class/gpio` 走 sysfs，否则自动改用 gpiochip 字符设备。

## 6. 相对原厂代码改了什么

### C 例程

| 文件 | 改动 |
|---|---|
| `c/lib/Config/DEV_Config.h`、`DEV_Config.c` | 彻底去掉 bcm2835 / wiringPi / lgpio，改为直接操作 `/dev/spidevX.Y`（spidev 裸 `write()`）+ sysfs GPIO |
| `c/lib/Config/DEV_Config.c` | 新增 SPI 设备自动探测：先看环境变量 `OLED_SPI_DEV`，否则 `glob("/dev/spidev*.*")` 并**优先选 CS0 节点**（`/dev/spidevX.0`），没有 CS0 时退回 CS1（`/dev/spidevX.1`）；片选可用 `OLED_SPI_CS=0|1` 指定（匹配 `spidevX.<n>`，找不到会明确报错）；**在终端里直接运行且 CS0/CS1 两个节点都存在时，会提示输入 0/1 选择片选**（回车默认 CS0），非终端环境不询问 |
| `c/lib/Config/DEV_Config.c` | DC/RST 解析：环境变量 `OLED_DC_GPIO` / `OLED_RST_GPIO` > 编译期默认值。**板型预设已从源码移出**，各板的值由一键脚本下发 |
| `c/lib/Config/DEV_Config.c`、`DEV_Config.h` | 新增 **GPIO 字符设备后端**：没有 `/sys/class/gpio` 的板自动改用 `/dev/gpiochipN` + ioctl，纯 C 实现、**不依赖 libgpiod**（本板有 sysfs，走原路径） |
| `c/lib/Config/DEV_Config.c` | `DEV_SPI_Write_nByte()` 对 `EIO` 重试 3 次。**原因：** Qualcomm GENI/GSI SPI 控制器偶发首个传输返回 EIO（本板实测遇到过一次，重试即可，见第 8 节） |
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
| `python/stats.py` | 字体路径由相对路径改成基于 `__file__`；**该例程仍需要 PIL**（本板没有 dpkg，装不上，用不到可忽略） |

本 HAT 的驱动本身只需要 `fcntl` / `struct` / `os` / `time` 这几个标准库模块，
除 C 编译器外**不需要任何第三方依赖**——对本板来说，Python 例程把 `python/` 拷到 `/data` 下即可直接跑。

## 7. 离线依赖包与预编译二进制

离线依赖已**统一放在 Waveshare/packages/**（45 个 arm64 deb，HAT 共用），
由根目录的 **install_deps.sh** 用 `dpkg -i` 安装（先检测、已装好的自动跳过；缺的跑两遍）。**本板没有 dpkg，该脚本会自动整段跳过、绝不强行安装**
——这类系统的 glibc 比 Debian 13 低，混装会破坏系统（见第 8 节）。

有意**没有**打包的东西：

| 没打包 | 原因 |
|---|---|
| `liblgpio-dev` / `liblgpio1` | trixie 源里不存在，C 代码已改为直接访问 `/dev/spidevX.Y`，不需要它 |
| `python3-smbus2` / `python3-spidev` / `python3-gpiozero` / `python3-pil` | Python 侧已改成纯标准库，这些一个都不需要 |

### prebuilt/ —— 本板依赖的兜底二进制

`prebuilt/main_aarch64_static`（约 924 KB）是一份**静态链接**的 C 例程，内容就是 `c/` 这套源码。

本板没有 gcc/make 且装不上，一键脚本会直接把这份二进制复制成 `c/main` 再运行，跳过编译。
因为是静态链接，它不依赖目标板的 glibc 版本——实测在 glibc 2.35 的本板与 glibc 2.41 的
Debian 板上都能正常运行。（实测动态版也能直接跑：本程序只用到了很老的 libc 符号，
启动只要求 `GLIBC_2.34`，2.35 满足。）

`c/` 源码改动后请重新生成这份二进制（在 Debian 板上）：

```bash
make CFLAGS="-O2 -Wall -static"
```

## 8. 注意事项与排错

### 在非 Debian 系统（本板 QTI Linux）上的额外注意事项

- **不要试图安装 `packages/` 里的 deb**：本板没有 dpkg/apt，而且 glibc 版本比 Debian 13 低，混装会破坏系统。实测也完全不需要，`install_deps.sh` 与各板脚本都会自动跳过。
- **C 例程**：板上没有 gcc / make / cc / clang，无法就地编译，由 `prebuilt/main_aarch64_static` 兜底。
- **Python 例程：开箱即用。** 只用标准库，把 `python/` 拷到一个可写目录即可：
  ```bash
  adb shell mkdir -p /data/oled
  adb push 2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/python /data/oled/
  adb shell 'cd /data/oled/python && python3 test.py'
  ```
- 文件系统是只读的：`/`、`/home/root`、`/opt` 都写不进去，可写的位置是 `/tmp`、`/data`、`/var/tmp`、`/var/volatile`，程序请放在 `/data` 下。
- `/dev/spidev*` 与 `/sys/class/gpio/*` 都需要 **root** 才能访问（`adb shell` 默认就是 root）。

### 扣板全黑时先查这两处（杜邦能亮 ≠ 扣板能亮）

HAT 上 H1~H6 是六个 3 焊盘 0Ω 跳线，选 SPI 侧还是 I2C 侧。**扣板走的是跳线通路，H7 杜邦是直接
接在跳线之后的模块侧网络上**，所以两者能不能亮是两件事：

```
扣板： 排针 pin19/23/24/18 ──▶ H3/H4/H5/H6（0Ω 跳线）──▶ OLED 模块
杜邦： H7 pin3/4/5/6        ──▶ (同一网络、直连模块，绕过这 4 个跳线)
```

SPI 模式 = **六个跳线全部桥接中间的 pin2 与 pin3**（H1: BS1↔GND、H2: BS2↔NC、
H3: DC↔Pin18、H4: CS↔Pin24、H5: DIN↔Pin19、H6: CLK↔Pin23）；
I2C 模式 = 桥接 pin1–pin2（DC/CS 被拉到 GND，DIN/CLK 走 Pin3/Pin5 的 SDA/SCL）。

用万用表蜂鸣档在 **HAT 单独**测（不用接开发板），全部应为 0Ω：

| 测点 | 期望 | 说明 |
|---|---|---|
| H7 pin1 (VCC) ↔ 40Pin 座子 pin2 / pin4 | 通 | HAT 自身 5V 布线 |
| H7 pin6 (DC) ↔ 座子 pin18 | 通 | H3 在 SPI 侧 |
| H7 pin5 (CS) ↔ 座子 pin24 | 通 | H4 在 SPI 侧 |
| H7 pin3 (DIN) ↔ 座子 pin19 | 通 | H5 在 SPI 侧 |
| H7 pin4 (CLK) ↔ 座子 pin23 | 通 | H6 在 SPI 侧 |
| H7 pin7 (RST) ↔ 座子 pin22 | 通 | 直连，无跳线 |

再上电（扣在板上）量：排针 pin2/pin4 对 pin6 应约 5V；HAT 上 C4 两端 / U2 的 Vout 对 GND 应约 3.3V。
**pin2/pin4 没有 5V 时整块屏不会亮**——这块 HAT 不吃排针的 3.3V，只吃 5V。

### 实测案例：扣板全黑 = 排针 pin22 / 座子 pin22 这一路接触不良

**本板实测**：杜邦接 H7 能亮、扣板全黑，此时在扣板状态下补一根 `排针 pin22 → HAT H7 pin7`
的飞线，**屏幕立刻点亮**。从原理图看只有一个解释——RST 那一路断了：

- RST 网络（网标 `P25`）全网只有 5 个点：**座子 pin22 + H7 pin7 + R6 10K→3V3 + C5 100nF→GND + OLED 模块 pin9 RES**；
- 它是 SPI 四线之外**唯一不经过 H1~H6 跳线**的信号——扣板时它的通路只有「排针 pin22 ↔ 座子 pin22」这一处机械接触；
- 同一块 HAT 在 Quectel Pi M2 上扣板是正常的（走的是同一段 HAT 板内铜箔），所以断点落在 **本板排针 pin22 的接触上**。给同一网络并联飞线能亮，等价于坐实了这一点。

初始化第一步就是 RST 脉冲（`OLED_RST_0/1`），RST 送不进去时它被 R6 一直拉到高、模块收不到复位脉冲，
表现为**整屏全黑**（不是花屏）。这个实验同时证明：5V 供电、DC/CLK/DIN/CS 的接触、H3~H6 跳线、
软件里 `RST=478` 全部是好的，**不需要改代码**。

定位到具体点（蜂鸣档两步）：

1. HAT 单独：`座子 pin22 焊点 ↔ H7 pin7` 应通（同一网络），不通 = HAT 板内断路；
2. 扣板状态：`开发板 pin22 引脚根部（板背面）↔ HAT H7 pin7` 应通，不通 = 排针/座子接触不良
   ——对照相邻的 pin20 / pin24，看 pin22 是否歪针、缩针，或座子孔内弹片是否塌陷。

注意这类故障多为**半接触（时通时断）**：蜂鸣档量到「通」不能排除它（量的一瞬间恰好接触上了），
最终以「拆飞线扣板 → 黑；插上飞线扣板 → 亮」的 A/B 复现作为判定。

三种修法：

| 修法 | 做法 | 说明 |
|---|---|---|
| **修接触（推荐）** | 校正/补焊开发板 pin22，或更换 HAT 上的 40Pin 排母 | 治本 |
| 保留飞线 | `排针 pin22 → H7 pin7` | 与板内网络并联，无风险，只是不美观 |
| 换一根脚做 RST | `排针 pin32 → H7 pin7`，运行时加 `OLED_RST_GPIO=465` | pin32 = TLMM67 = Linux GPIO 465，该脚在 HAT 上是空网络；**别用 pin26**（TLMM82 属 SPI CE1 组，SPI 传输时会被 pinctrl 切走） |

### 设备与引脚的覆盖方式

```bash
# 一键脚本已经下发好参数；下面是手动指定（环境变量优先级最高）：
sudo OLED_SPI_DEV=/dev/spidev0.0 OLED_DC_GPIO=423 OLED_RST_GPIO=478 \
     2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/c/main

# 不想用一键脚本时也可直接跑 Python 例程（同板同参数）：
sudo OLED_DC_GPIO=423 OLED_RST_GPIO=478 \
     python3 2.23inch-OLED-HAT/QPi_2.23inch_OLED_HAT_SDK/python/test.py
```

Python 侧同样支持这些环境变量（`test.py` / `hat_hw.py`）。

### SPI 参数

- **Mode 3**（`CPOL=1, CPHA=1`）、8bit、**1 MHz**。原厂 C 版用 bcm2835 的 `SPI_MODE3`、lgpio 的 `lgSpiOpen(..., 0)`、Python 版 `mode=0b11` 都是 Mode 3，本次统一成 Mode 3 + 1 MHz。SSD1305 支持到 10 MHz，这个值很保守。
- 选设备默认优先取 `/dev/spidevX.0`（HAT 的 CS 接 CE0/Pin24）；`OLED_SPI_CS=1` / `OLED_SPI_DEV=/dev/spidevX.Y` 可覆盖。**一键脚本 [3/4] 步会询问选 CE0 还是 CE1（回车默认 CE0，非终端环境默认 CE0）**；例程在终端里直接运行、且 CS0/CS1 两个节点都存在时也会提示输入 0/1。
- 本板有 `spidev0.0` 与 `spidev0.1` 两个节点：`spidev0.0` = Pin24（CE0），`spidev0.1` = Pin26（CE1），CS 接哪根就选哪个。

### 屏幕可见区只有 32 行

本屏是 **128×32**，而驱动 IC 的显存是 132×64（代码里 `WIDTH=132`、`HEIGHT=64`、8 页）。
初始化序列里 `0xA8, 0x1F` 把 MUX 设成 32 行，所以**只有最上面 32 行可见**。
原厂例程按 12px 行高画了 4 行（`y = 0 / 12 / 24 / 36`），第 4 行 `128x32 Pixels` 起始于 y=36，
落在可见区外；例程随后打开了「垂直+水平滚动」，滚动过程中它会移进可见区。
**这里与原厂代码完全一致，未做改动。**

### 权限

- 读写 `/dev/spidev*` 需要 root（或相应的 udev 规则）；`/sys/class/gpio` 的 export / direction / value 同样需要 root。
- 所以两个例程都要 **`sudo`** 运行（`adb shell` 默认就是 root）。

### Qualcomm 板首次传输可能报 EIO

在本板实测遇到过一次 `/dev/spidev0.0` 的裸 `write()` 返回 `Input/output error`
（内核日志：`spi_geni 4a80000.spi: err.routine 0, err.type 0, err.code 0` → `spidev spi0.0: SPI transfer failed: -5`），
之后连续 6 次运行都没再出现，属于 GENI/GSI 控制器首个传输偶发失败。

对 OLED 来说首个字节正好是初始化命令，丢掉就整屏不亮，所以 C 与 Python 两侧都对 `EIO` 做了 **3 次重试**。
如果仍然频繁报这个错，可以先用厂商工具确认 40Pin 的 SPI 通路：

```bash
sudo 40Pin-ctrl test spi        # Qualcomm 系列板
```

### 其他已知现象（都不影响使用）

- **一键脚本从 Windows 拷过去后不可执行**：执行 `chmod +x M1_2.23inch-OLED-HAT.sh`。文件本身已是 LF 行尾，无需转换。
- **`stats.py` 报 `No module named 'PIL'`**：这是预期的，本 HAT 的驱动与 `test.py` 都不需要 PIL。本板装不上 PIL，用不到可忽略。
- **`python/drive/config.py` 里的 `RST_PIN` / `DC_PIN` 是 Linux GPIO 号，不是树莓派 BCM 号**：原厂是 `25` / `24`（BCM），现在由脚本下发的 423 / 478。

### 用厂商工具排查 40Pin 时

```bash
# 查 Pin18 / Pin22 的电平状态（正常应为 high 或 low，而不是 unexported/err）
40Pin-ctrl pin 18 status
40Pin-ctrl pin 22 status
```

`40Pin-ctrl pin <N> status` 只覆盖普通 GPIO（本 HAT 的 Pin18/Pin22），SPI 那 4 根（Pin19/21/23/24）
由 SPI 控制器直接驱动，不会出现在这个命令里，显示 `unexported` 是正常的。

## 9. 开发板上做过的持久改动（与 HAT 无关，但请知悉）

为防止调试时开发板自动休眠，曾在本板执行过：

```bash
systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target
```

如需恢复默认休眠行为：

```bash
systemctl unmask sleep.target suspend.target hibernate.target hybrid-sleep.target
```

**本 HAT 自身没有在开发板上留下任何持久改动**：GPIO 只 export 不 unexport（退出后引脚仍保持
输出状态、屏幕继续显示），不会写入任何配置文件、不会开机自启。要彻底清理：

```bash
echo 423 > /sys/class/gpio/unexport     # DC
echo 478 > /sys/class/gpio/unexport     # RST
```