# Environment Sensor HAT 适配说明（SKU 20471）—— Quectel Pi H1（QCS6490）

> 本文件对应一键脚本 **`H1_Environment-Sensor-HAT.sh`**（在 Environment-Sensor-HAT 目录下执行）。
> 其它开发板见同目录：README_L1.md / README_M1.md / README_M2.md。
>
> ⚠ **本组合（H1 + 环境传感 HAT）未实测。** SDK 保证与其它板完全同一份源码、同一套自动探测逻辑；
> 首次在 H1 上使用时请按第 8 节「首次在 H1 上测试」的建议操作，并把脚本 [3/4] 步的输出作为判断依据。

原厂 `Environment_Sensor_HAT_Code` 只支持树莓派（依赖 bcm2835 / wiringPi / lgpio，I2C 写死 `i2c-1`）。
本 SDK 是适配版：插上 HAT、在 `Environment-Sensor-HAT/` 目录下执行一条命令即可使用。

---

## 1. 目录结构

```
a.Waveshare/
├── _common.sh                          一键脚本共用函数库（无需单独执行）
├── install_deps.sh                    离线依赖安装脚本（HAT 共用，装一次即可）
├── packages/                           45 个 arm64 离线 deb 依赖（HAT 共用）
├── Environment-Sensor-HAT/
│   ├── H1_Environment-Sensor-HAT.sh    ← 本板一键脚本（H1 的差异只在这里）
│   ├── L1_Environment-Sensor-HAT.sh / M1_Environment-Sensor-HAT.sh / M2_Environment-Sensor-HAT.sh
│   ├── README_L1.md / README_M1.md / README_H1.md / README_M2.md   四块板各一份
│   ├── QPi_Environment_Sensor_HAT_SDK/ ← 适配后的 SDK，交付使用这一个
│   │   ├── prebuilt/                   C 例程预编译静态二进制（兜底用，见第 7 节）
│   │   ├── c/                          C 例程源码（已适配）
│   │   └── python/                     Python 例程（已适配）
│   ├── Environment_Sensor_HAT_Code.zip 原厂源码压缩包，未做任何改动，仅作参考
│   └── *.pdf                           各传感器数据手册 + HAT 原理图
└── 2.23inch-OLED-HAT/                  另一块 HAT（结构与本目录类似）
```

## 2. 硬件连接（40Pin 接线）

### HAT 只用 Pin 3 / Pin 5 这一组 I2C

| 40Pin | 原理图引脚名 | 实际接了什么 |
|---|---|---|
| **Pin 3 / Pin 5** | `SDA1` / `SCL1`（网名 `RPI_SDA`/`SDA`、`RPI_SCL`/`SCL`） | **HAT 上 5 个传感器全部接在这一组** |
| Pin 27 / Pin 28 | `SDA0` / `SCL0` | 第二组 I2C，**HAT 上没有接任何器件，是悬空的** |

> **连接方式**：所有开发板都可以直接把 HAT 扣在 40Pin 卡座上使用（扣板接通的即 Pin 3 / Pin 5 这组 I2C）；
> **Pin 27 / Pin 28** 这组 I2C 也可以把 HAT 板与 40Pin 用杜邦线对应连接后使用。

所以直接扣板时，**Pin 27 / 28 对应的那条 I2C 扫不到任何 HAT 器件是正常的**。
H1 上 Pin 3 / 5 这条总线需先用 qpi-config 使能（详见第 3 节）。

## 3. 快速使用

**第一步：确认 40Pin 的 I2C 已使能。**

H1 的 40Pin 外设由 **`qpi-config`** 管理（例如 OLED 侧实测的 spi10 就是这么打开的）。
需要在 `/etc/qpi-config/qpi-config.ini` 里把 40Pin I2C 对应的控制器改成 `enable`，
再执行 `sudo qpi-config 40pin set` 应用（具体项名以板子出厂配置 / H1 手册为准）。

使能后用下面的命令确认 HAT 在线：

```bash
ls /dev/i2c-*          # 应能看到对应总线
i2cdetect -y -q <总线号>   # 应能看到 0x29 0x53 0x59 0x68 0x76
```

**第二步：安装一次离线依赖（HAT 共用，装过可跳过）：**

```bash
cd <a.Waveshare 所在目录>
chmod +x install_deps.sh            # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./install_deps.sh
```

本板系统源自带 gcc/make，这一步主要是补齐 `python3-smbus2` 等运行依赖（已装好的会自动跳过）。

**第三步：执行本板一键脚本（先 `cd` 到 `Environment-Sensor-HAT/`；脚本只检查依赖，不再安装）：**

```bash
cd <a.Waveshare 所在目录>/Environment-Sensor-HAT
chmod +x H1_Environment-Sensor-HAT.sh    # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./H1_Environment-Sensor-HAT.sh
```

脚本依次做四件事：

1. 检查 `packages/` 里的离线依赖是否已装好（缺失时会提示先跑一次 `sudo ./install_deps.sh`；脚本本身不再安装）
2. 校验工具链与 Python 运行环境（H1 系统源自带 gcc/make）
3. 自动识别 HAT 插在哪条 I2C 总线上，并把结果打印出来
4. 编译 C 例程生成 `c/main`，随后跑 10 秒自检，直接打印各传感器读数

脚本参数：

| 参数 | 作用 |
|---|---|
| （无） | 检查依赖 + 校验 + 识别 + 编译 + 自检 |
| `--no-build` | 跳过 C 例程编译（`c/main` 不存在时仍会取 `prebuilt/` 兜底） |

安装完成后单独运行例程：

```bash
sudo Environment-Sensor-HAT/QPi_Environment_Sensor_HAT_SDK/c/main                    # C 例程
sudo python3 Environment-Sensor-HAT/QPi_Environment_Sensor_HAT_SDK/python/test.py   # Python 例程
```

两个例程都会自动识别总线，无需改代码。

## 4. 本板情况（未实测）

本组合尚未在 H1 上实测，下表是 H1 的系统信息与预期：

| 项 | 值 / 预期 |
|---|---|
| 系统 | Debian 13 (trixie)，内核 6.6.116-qli-1.7 |
| Python | 3.13.5 |
| 板上编译器 | 有（gcc / make） |
| HAT 所在总线 | **未实测**。需先按第 3 节用 qpi-config 使能 40Pin I2C；识别靠自动探测，脚本 [3/4] 会打印结果 |
| C / Python 例程 | 预期与 H1+OLED（已实测）一样开箱即用；未实测 |

> 参考：OLED 侧在 H1 上已实测跑通（同一条 40Pin 排针、同样走 qpi-config 使能），
> 本 HAT 走 I2C、无引脚参数，预期同样可行。失败时优先排查「40Pin I2C 是否已使能」与
> 「排针是否插紧」，再按第 5 节手动指定总线。

## 5. 板型差异如何下发（源码保持统一）

SDK 源码（`c/`、`python/`）对四块开发板**完全相同**，板型参数不写进源码。

本 HAT 走 I2C，不需要 DC/RST 这类引脚参数；总线识别是**通用自动探测**，保留在源码里：

- 先读环境变量 `HAT_I2C_BUS`（手动指定，优先级最高）；
- 否则扫描 `i2c-0` ~ `i2c-15`，用「BME280 在 0x76 能读回 `0xD0 == 0x60`」当探针，命中即用。

一键脚本把「检查依赖 → 校验工具链/运行环境 → 识别总线 → 编译/自检」固化下来。
自动识别失败时按脚本提示手动指定即可（把 `n` 换成 H1 上实际出现的那条总线号）：

```bash
sudo HAT_I2C_BUS=n Environment-Sensor-HAT/QPi_Environment_Sensor_HAT_SDK/c/main
```

## 6. 相对原厂代码改了什么

### C 例程

| 文件 | 改动 |
|---|---|
| `c/lib/Config/DEV_Config.c`、`DEV_Config.h` | 彻底去掉 bcm2835 / wiringPi / lgpio，改为直接操作 `/dev/i2c-N`（i2c-dev + `I2C_RDWR`） |
| `c/lib/Config/DEV_Config.c` | 新增总线自动探测：先看环境变量 `HAT_I2C_BUS`，否则扫描 i2c-0~15，用 BME280（0x76 读 0xD0 == 0x60）当探针 |
| `c/lib/Config/DEV_Config.c` | 新增裸读接口 `I2C_Read_Raw()`：不发寄存器地址，直接读 N 字节 |
| `c/lib/SGP40/SGP40.c` | 读完 SGP40 命令后改用 `I2C_Read_Raw(Rbuf, 3)`。**原因：**「写 1 字节寄存器地址再读」的方式会被 SGP40 回 NACK，必须先发命令、再单独发起一次读时序 |
| `c/Makefile` | 去掉 `USELIB` 和 `-llgpio`，只保留 `-lm`；自动创建 `bin/` 目录 |

原厂的 GPIO / SPI / UART 相关代码全部删除——本 HAT 只走 I2C，例程里没有任何一处调用它们。

### Python 例程

| 文件 | 改动 |
|---|---|
| `python/hat_bus.py` | **新增**，负责自动识别 I2C 总线号 |
| `python/*.py` | `import smbus` → `import smbus2 as smbus`，`smbus.SMBus(1)` → `smbus.SMBus(BUS)` |
| `python/SGP40.py` | `Read()` 改用 `smbus2.i2c_msg` 做裸读，原因同 C 版 |
| `python/MPU925x.py` | 去掉 `import RPi.GPIO`（原厂用它做光强中断，本方案走轮询读取） |
| `python/test.py` | 主程序，改用 smbus2 与自动识别的总线号 |

为什么选 `smbus2` 而不是 `python-smbus`：前者是纯 Python 实现，不依赖 I2C 运行时 ABI，
跨平台 / 跨 Python 版本都没有装不上的风险。

## 7. 离线依赖包与预编译二进制

离线依赖已**统一放在 a.Waveshare/packages/**（45 个 arm64 deb，HAT 共用），
由根目录的 **install_deps.sh** 用 `dpkg -i` 安装（先检测、已装好的自动跳过；缺的跑两遍，第二遍补齐第一遍因依赖顺序失败的包）；各板一键脚本只检查、不再安装。
H1 系统源自带 gcc/make，装包只为补齐 `python3-smbus2` 等运行依赖，缺时跑一次 `install_deps.sh` 即可。

45 个包分三类：

1. **C 工具链**：`gcc*`、`cpp*`、`binutils*`、`make`、`libc6-dev` 及其依赖
2. **`libc6_2.41-12+deb13u4`**：用于把 `deb13u3` 升级到与 `libc6-dev` 相同的版本
3. **`python3-smbus2_0.4.3-1`**：纯 Python，不需要额外的 C 扩展依赖链（只有本 HAT 用得到）

### prebuilt/ —— 兜底用的预编译二进制

`prebuilt/main_aarch64_static`（约 790 KB）是一份**静态链接**的 C 例程，内容就是 `c/` 这套源码，
在 Debian 板上用下面这条命令编出来后保存进来的：

```bash
gcc -O2 -static bin/*.o -o main_static -lm
```

用途是**兜底**：目标板没有 gcc/make、或者根本装不了（QTI Linux、Yocto 这类非 Debian 系统），
一键脚本就会直接把这份二进制复制成 `c/main` 再运行，跳过编译。因为是静态链接，它不依赖目标板的
glibc 版本——实测在 glibc 2.35 的 QTI Linux 板和 glibc 2.41 的 Debian 板上都能正常运行。

`c/` 源码改动后请重新生成这份二进制，否则兜底路径跑的仍然是旧逻辑。

## 8. 注意事项与排错

### 首次在 H1 上测试的建议

1. 先确认 40Pin 的 I2C 已使能（第 3 节），`ls /dev/i2c-*` 能看到新总线；
2. 用 `i2cdetect -y -q <总线号>` 确认 5 个器件在（`0x29 0x53 0x59 0x68 0x76`）；
3. 在 `Environment-Sensor-HAT/` 下跑 `sudo ./H1_Environment-Sensor-HAT.sh`，看 [3/4] 步识别结果（Python 例程缺 smbus2 时先在根目录跑一次 `sudo ./install_deps.sh`）；
4. 若识别失败，按提示 `sudo HAT_I2C_BUS=<总线号> ./QPi_Environment_Sensor_HAT_SDK/c/main` 手动指定。

H1 的 40Pin GPIO 未开 `CONFIG_GPIO_SYSFS`（OLED 侧已知），但本 HAT 只走 I2C，不受影响。

### 总线识别

- 优先读环境变量，可手动指定：`sudo HAT_I2C_BUS=n ./c/main`
- 自动识别的依据是「这条总线上 0x76 能读回 0x60」，也就是 BME280 在线。插针没插紧就会识别失败。

### 权限

- 读写 `/dev/i2c-*` 通常需要 root 或 `i2c` 组：
  ```bash
  sudo usermod -aG i2c $USER   # 之后重新登录
  ```

### 用 i2cdetect 排查时注意

- 请用 **`i2cdetect -y -q <总线号>`**（quick write 探测），不要用 `-r`。
- 用 `-r` 时会看到一个并不存在的 `0x28`，同时 SGP40 的 `0x59` 反而不显示，容易误判硬件坏了。
- 用 `-q` 显示的才是真实设备：`0x29 0x53 0x59 0x68 0x76`。

### 传感器地址一览

| 传感器 | 功能 | 地址 | 芯片 ID 校验 |
|---|---|---|---|
| BME280 | 气压 / 温度 / 湿度 | 0x76 | 0xD0 == 0x60 |
| ICM20948 | 9 轴 IMU | 0x68 | 0x00 == 0xEA |
| TSL2591 | 可见光 + 红外光强 | 0x29 | 0xB2（0x12 需带 COMMAND 位 0xA0） |
| LTR390 | UV | 0x53 | 0x06 == 0xB2 |
| SGP40 | VOC 空气质量 | 0x59 | featureset 0x202F / 自检 0x280E |

### 已知现象（都不影响使用）

- **首次运行可能打印 `Self test failed`**：这是原厂 `SGP40_init()` 里的自检提示，随后 `gas` 读数正常。再运行一次就不再出现，属于 SGP40 首次上电状态的时序问题，原厂逻辑未改动。
- **编译时 `IMU.c` 有一条 `'y' is used uninitialized` 警告**：原厂代码里的快速平方根倒数写法，不影响运行结果。
- **`make` 报「修改时间在未来」**：开发板时钟落后于 PC 导致的，一键脚本已在编译前统一 `touch` 过，直接手动 `make` 时可能看到该警告。
- **一键脚本从 Windows 拷过去后不可执行**：执行 `chmod +x H1_Environment-Sensor-HAT.sh`。文件本身已是 LF 行尾，无需转换。
- **首帧气压值偏低**（例如 953 hPa 然后跳到 1010 hPa）：BME280 上电后第一个采样周期未完成，第二个数据起就正常。

## 9. 开发板上做过的持久改动（与 HAT 无关，但请知悉）

为防止调试时开发板自动休眠，曾在调过的板上执行过：

```bash
systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target
```

如需恢复默认休眠行为：

```bash
systemctl unmask sleep.target suspend.target hibernate.target hybrid-sleep.target
```

H1 上为使能 40Pin SPI，曾通过 `qpi-config` 在 `/etc/qpi-config/qpi-config.ini` 中把 `spi10`
改成 `enable`（`sudo qpi-config 40pin set` 应用）——这是板子的 40Pin 配置，与例程本身无关；
不再使用时把对应项改回 `disable` 再应用即可。本 HAT（I2C）如需使能 I2C 控制器同理。