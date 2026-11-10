# OLED/LCD HAT (A) 适配说明 —— Quectel Pi M2（RK3576）

> 本文件对应一键脚本 **`M2_OLED-LCD-HAT.sh`**（在 OLED-LCD-HAT 目录下执行）。

原厂 `OLED_LCD_HAT_A_Demo` 只支持树莓派：C 侧默认 wiringPi / bcm2835，Python 侧依赖
`luma.oled` + `gpiozero` + `PIL`，设备与引脚全部写死成树莓派的 `spidev0.0` / `i2c-1` / BCM 编号。
本 SDK 是适配版，**C 例程已改成本板可直接运行的 spidev + i2c-dev + sysfs GPIO 实现**。

> 本 HAT 的 Python 例程本期未适配（依赖树莓派专有库与 `vcgencmd`），交付以 C 例程为准。

---

## 1. 目录结构

```
demo-40Pin-HAT/
├── _common.sh                         一键脚本共用函数库（无需单独执行）
└── OLED-LCD-HAT/
    ├── M2_OLED-LCD-HAT.sh             ← 本板一键脚本（M2 的差异只在这里）
    ├── README_M2.md                   本文件
    └── QPi_OLED_LCD_HAT_SDK/          ← 适配后的 SDK，交付使用这一个
        ├── prebuilt/main_aarch64_static   C 例程静态二进制（无 gcc 的板子兜底用）
        └── c/
            ├── Makefile
            ├── examples/                  main.c / OLED_0in96_test.c / LCD_2in_test.c
            │                              Triple_OLED_LCD.c / KEY_test.c
            ├── lib/Config/                DEV_Config.c(.h) —— 唯一重写的硬件层
            ├── lib/{OLED,LCD,GUI,Fonts}/  原厂驱动与字库，未改动
            └── pic/                       例程用到的 bmp
```

> 原厂 `OLED_LCD_HAT_A_Demo` 压缩包与原理图 / 屏幕资料不随仓库发布。

## 2. 硬件连接（40Pin 接线）

> **安全提示**：接线或更换外设前请先断开开发板电源（拔掉 USB/电源），接好后再上电，避免带电插拔损坏外设或主板。

> **连接方式**：HAT 可以直接扣在 M2 的 40Pin 卡座上使用，不需要杜邦线。

### HAT 用到的引脚

| 信号 | 40Pin | 树莓派 BCM | 本板（M2）的 GPIO | 说明 |
|---|---|---|---|---|
| SDA | Pin 3 | BCM2 | **140** | 0.96 OLED，`/dev/i2c-3` |
| SCL | Pin 5 | BCM3 | **141** | 0.96 OLED，`/dev/i2c-3` |
| KEY1 | Pin 7 | BCM4 | **79** | 输入，HAT 上有 10K 上拉，按下为低 |
| KEY2 | Pin 11 | BCM17 | **78** | 同上 |
| BL 背光 | Pin 12 | BCM18 | **87** | 高电平点亮（经 NPN 3904 驱动） |
| LCD RST | Pin 13 | BCM27 | **70** | 复位（低有效） |
| LCD DC | Pin 15 | BCM22 | **71** | 命令/数据选择 |
| KEY3 | Pin 16 | BCM23 | **132** | 输入 |
| KEY4 | Pin 18 | BCM24 | **134** | 输入 |
| SPI MOSI | Pin 19 | BCM10 | SPI1_MOSI | 2inch LCD 数据 |
| SPI SCLK | Pin 23 | BCM11 | SPI1_CLK | 2inch LCD 时钟 |
| SPI CS | Pin 24 | BCM8 | SPI1_CE0 | 硬件片选，`/dev/spidev1.0` |
| 3V3 | Pin 1 / 17 | — | 3.3V | HAT 供电 |
| GND | 多个 | — | GND | 地 |

说明：

- 这块 HAT **只吃 3.3V**，排针上的 5V（Pin 2 / 4）原理图上没有接到任何器件。
- 0.96 OLED 有两块，I2C 地址分别是 **0x3C** 和 **0x3D**（板上 R1/R31 决定），
  命令与显存会同时写两块；如需只驱动其中一块，可用 `OLED_I2C_ADDR=0x3C` 覆盖。
- Pin 24 在 M2 上是 SPI1 的硬件片选，由内核 SPI 控制器持有。原厂代码里的软件 CS
  在适配版中已忽略，靠硬件片选工作。
- **这组引脚是独占的**：SPI1_CE0（Pin24/SCLK Pin23/MOSI Pin19）加上 GPIO70/71/87
  一旦被其他用途占用，本 SDK 就无法再驱动这块 2inch LCD。典型场景是想让这块屏显示
  Debian GNOME 桌面：需要内核 DRM/fbtft 面板驱动接管 SPI1，届时 `/dev/spidev1.0`
  消失，用户态例程（`./main 2` 等）会同时失效，两者不能共存。
  只跑例程时保持现状即可，不要加载面板驱动；若必须显示桌面，请改用 HDMI/USB/DSI 屏，
  以免占用 40Pin 的 SPI。占用这组 SPI 不影响 I2C3（两块 0.96 OLED）及其余 GPIO。

## 3. 快速使用

**第一步：确认 40Pin 的 SPI 已经使能，也就是 `/dev/spidevX.Y` 存在。**

```bash
ls /dev/spidev*.*
```

没有的话，M2 用厂商工具打开 40Pin 的 SPI（改完需要重启）：

```bash
sudo qpi-config spi1 enable
```

I2C 不用配置：M2 的 Pin3/Pin5 直连 SoC 的 I2C3，开机即有 `/dev/i2c-3`。

**第二步：安装一次离线依赖（HAT 共用，装过可跳过）：**

```bash
cd <Waveshare 所在目录>
chmod +x install_deps.sh            # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./install_deps.sh
```

**第三步：执行本板一键脚本。**

```bash
cd <Waveshare 所在目录>/OLED-LCD-HAT
chmod +x M2_OLED-LCD-HAT.sh
sudo ./M2_OLED-LCD-HAT.sh
```

脚本依次做四件事：

1. 检查 `packages/` 里的离线依赖是否已装好（缺失时提示先跑一次 `sudo ./install_deps.sh`）
2. 校验工具链（没有 gcc / make 时改用 `prebuilt/` 里的静态二进制）
3. 打印本板使用的 SPI/I2C 设备与 GPIO 号
4. 编译 C 例程生成 `c/main`（或复制 prebuilt），并依次跑两个自检：
   - `./main 2`：2inch LCD 显示图形，约 4 秒后自动退出
   - `./main 0.96`：两块 0.96 OLED 显示文字/图片，12 秒后自动结束

脚本参数：

| 参数 | 作用 |
|---|---|
| （无） | 检查依赖 + 校验 + 识别 + 编译 + 自检 |
| `--no-build` | 跳过 C 例程编译（`c/main` 不存在时用 `prebuilt/main_aarch64_static` 兜底） |

安装完成后单独运行例程（**必须先 `cd` 到 `c/` 目录**，例程用相对路径读 `pic/*.bmp`）：

```bash
cd <Waveshare 所在目录>/OLED-LCD-HAT/QPi_OLED_LCD_HAT_SDK/c

# 本板参数（每行都要带，否则新 shell 会退回源码里的兜底值）
HAT_ENV="LCD_DC_GPIO=71 LCD_RST_GPIO=70 LCD_BL_GPIO=87 \
KEY1_GPIO=79 KEY2_GPIO=78 KEY3_GPIO=132 KEY4_GPIO=134 \
LCD_SPI_DEV=/dev/spidev1.0 OLED_I2C_DEV=/dev/i2c-3 OLED_I2C_ADDR=0x3C,0x3D"

sudo env $HAT_ENV ./main 2        # 2inch LCD（约 4 秒后退出）
sudo env $HAT_ENV ./main 0.96     # 0.96 OLED（一直运行，Ctrl+C 退出）
sudo env $HAT_ENV ./main all      # 两块屏同时显示（一直运行，Ctrl+C 退出）
sudo env $HAT_ENV ./main key      # 按键测试（一直运行，Ctrl+C 退出）
```

`0.96` / `all` / `key` 是原厂的无限循环例程，按 `Ctrl+C` 退出；
退出时例程会关闭设备但**保持屏幕显示内容**。

## 4. 板型差异如何下发（源码保持统一）

板型参数不在源码里，由一键脚本经环境变量下发；`DEV_Config.c` 里只保留 M2 的兜底值。

| 环境变量 | 作用 | 本脚本下发的值 |
|---|---|---|
| `LCD_DC_GPIO` | LCD DC（Pin15） | **71** |
| `LCD_RST_GPIO` | LCD RST（Pin13） | **70** |
| `LCD_BL_GPIO` | 背光 BL（Pin12） | **87** |
| `KEY1_GPIO` … `KEY4_GPIO` | 四个按键 | **79 / 78 / 132 / 134** |
| `LCD_SPI_DEV` | 直接指定 SPI 节点（优先级最高） | 存在时下发 **/dev/spidev1.0**，否则自动探测（优先 CE0） |
| `LCD_SPI_MODE` | SPI 模式 | **0**（ST7789VW） |
| `LCD_SPI_SPEED` | SPI 时钟 | **8000000**（8 MHz） |
| `OLED_I2C_DEV` | 0.96 OLED 的 I2C 设备 | **/dev/i2c-3** |
| `OLED_I2C_ADDR` | 0.96 OLED 的 I2C 地址（逗号分隔） | **0x3C,0x3D** |

## 5. 相对原厂代码改了什么

| 文件 | 改动 |
|---|---|
| `lib/Config/DEV_Config.c`、`DEV_Config.h` | 彻底去掉 bcm2835 / wiringPi，改为直接操作 `/dev/spidevX.Y`、`/dev/i2c-N` 和 sysfs GPIO；DC/RST/BL/按键号与设备节点由环境变量下发 |
| `lib/Config/DEV_Config.h` | 新增 `PI_THREAD` / `piThreadCreate` 的 pthread 等价实现（原厂这两个宏来自 wiringPi） |
| `lib/Config/DEV_Config.c` | 软件 CS 改为忽略：`LCD_CS` 固定为 -1，Pin24 由 SPI 控制器做硬件片选 |
| `lib/Config/DEV_Config.c` | `DEV_SetBacklight()` 改为开/关背光（M2 的 Pin12 不是 PWM 通道；原厂软件 PWM 线程在 `USE_DEV_LIB` 分支里本来也没启动） |
| `lib/Config/DEV_Config.c` | SPI 写失败对 `EIO` 重试 3 次；OLED 的 I2C 写失败只提示一次，避免刷屏 |
| `lib/Config/DEV_Config.c`、`DEV_Config.h` | 新增 `I2C_Write_nByte()` 整块 I2C 写接口 |
| `lib/OLED/OLED_0in96.c` | 显存数据由逐字节 I2C 事务改为一次整块写：整屏刷新从 664 ms 降到 192 ms，翻页时不再看到上一页残留 |
| `lib/GUI/GUI_BMPfile.c` | 修复原厂 `GUI_ReadBmp()` 读调色板的栈越界：`fread(&bmprgbquad[i * 4], ...)` 改为 `fread(&bmprgbquad[i], ...)`；原写法会越过 2 元素调色板数组，显示 page 4 的 BMP 时偶发 `stack smashing detected` |
| `examples/Triple_OLED_LCD.c` | 一处 `delay(1500)`（wiringPi）改成 `DEV_Delay_ms(1500)` |
| `examples/OLED_0in96_test.c`、`Triple_OLED_LCD.c` | 中文“微雪电子”的 y 坐标由 20 改成 14：原坐标下字形底部贴近屏幕下边缘，看起来像被截断 |
| `examples/main.c`、`LCD_2in_test.c` | 初始化/内存失败时的返回码改为非 0（原厂 `main` 一律返回 0、`LCD_2IN_test` 失败也 `exit(0)`，一键脚本的自检会误判为通过） |
| `Makefile` | 去掉 `USELIB` 与三个库分支，只保留 `-lm -lpthread`；自动创建 `bin/` |
| `lib/{LCD,GUI,Fonts}/`、其余 examples | **未改动** |
| 原厂的 `RPI_sysfs_gpio.*`、`dev_hardware_SPI.*`、`dev_hardware_i2c.*` | 未收录（功能已并入 `DEV_Config.c`） |
| Python 例程 | 未适配（依赖 `luma.oled` / `gpiozero` / `PIL` / `vcgencmd`，且写死 `i2c-1`） |

## 6. 注意事项与排错

### 一块屏亮、另一块不亮

- 两块屏共用 3.3V 和 GND，先确认 HAT 已插紧、Pin1/17 对 GND 约 3.3V。
- 2inch LCD 全黑但 OLED 正常：先看背光（Pin12）是否被拉高；本适配版在 `DEV_ModuleInit()`
  里已把 BL 置高。
- OLED 正常但 LCD 无显示：确认 `ls /dev/spidev*.*` 有 `spidev1.0`，并且已经
  `qpi-config spi1 enable` 后重启。

### OLED 不亮 / 日志出现 `I2C write failed ... no ACK`

- 用 `ls /dev/i2c-*` 确认有 `i2c-3`；`i2cdetect -y -q 3` 应能看到 0x3C 和 0x3D。
- 如果只有一块 OLED 亮，检查另一块的地址是否被 `OLED_I2C_ADDR` 列表漏掉。

### 权限

读写 `/dev/spidev*`、`/dev/i2c-*` 和 `/sys/class/gpio` 都需要 root，所以例程都要 `sudo` 运行。

### 退出后引脚状态

- 例程退出时不 unexport GPIO，RST 保持高、DC 保持低、背光保持点亮，屏幕继续显示。
- 需要彻底释放时：
  ```bash
  for p in 70 71 87 79 78 132 134; do echo $p > /sys/class/gpio/unexport; done
  ```

### 其他

- **原厂包里的 `c/main` 不能直接运行**：它是 32 位 ARM（armhf）程序，M2 是 aarch64。
- **`stats.py` 等 Python 例程**：需要 `luma.oled`、`gpiozero`、`PIL`，且 `vcgencmd` 只有树莓派有；
  本期不提供 Python 支持。
- **一键脚本从 Windows 拷过去后不可执行**：执行 `chmod +x M2_OLED-LCD-HAT.sh`。

## 7. 本板实测记录

| 项 | 值 |
|---|---|
| `/proc/device-tree/model` | `Quectel PI M2 RK3576 Board` |
| 系统 | Debian GNU/Linux 13 (trixie)，内核 6.1.118-rt36，aarch64 |
| GPIO 后端 | sysfs（`/sys/class/gpio`） |
| SPI 设备 | `/dev/spidev1.0`（spi1，mode0，8 MHz） |
| I2C 设备 | `/dev/i2c-3`，0x3C + 0x3D |
| 2inch LCD | **实测通过**（图形 + BMP） |
| 0.96 OLED ×2 | **实测通过**（文字/图片，两块同步显示） |
| 翻页刷新 | **实测通过**（整块 I2C 写，整屏约 192 ms，无上一页残留） |
| page 4 BMP | **实测通过**（修复调色板栈越界后不再 `stack smashing`） |
| 按键 K1~K4 | **实测通过**（GPIO 79/78/132/134 均可拉低，key 例程有响应） |
| 一键脚本 | **实测通过**（板上无 gcc，自动改用 `prebuilt/main_aarch64_static`） |

> 本板当前没有 gcc / make，一键脚本会走 prebuilt 兜底；
> 需要在板上重新编译时，先在 Waveshare 根目录执行一次 `sudo ./install_deps.sh` 安装工具链。
> 按键演示以屏幕上方框填充为准；控制台打印按主线程刷新周期采样，极短的点按可能只在 LCD 线程被捕获。
