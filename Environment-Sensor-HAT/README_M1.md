# Environment Sensor HAT 适配说明（SKU 20471）—— Quectel Pi M1（SM6115，QTI Linux）

> 本文件对应一键脚本 **`M1_Environment-Sensor-HAT.sh`**（在 Environment-Sensor-HAT 目录下执行）。
> 其它开发板见同目录：README_L1.md / README_H1.md / README_M2.md。

原厂 `Environment_Sensor_HAT_Code` 只支持树莓派（依赖 bcm2835 / wiringPi / lgpio，I2C 写死 `i2c-1`）。
本 SDK 是适配版，**已在本板实测跑通**：插上 HAT、在 `Environment-Sensor-HAT/` 目录下执行一条命令即可使用。
本板是 **QTI Linux（Yocto 类，非 Debian）**，没有 dpkg / 编译器，脚本会自动走 `prebuilt/` 兜底路径（见第 7、8 节）。

---

## 1. 目录结构

```
a.Waveshare/
├── _common.sh                          一键脚本共用函数库（无需单独执行）
├── install_deps.sh                    离线依赖安装脚本（HAT 共用，装一次即可）
├── packages/                           45 个 arm64 离线 deb 依赖（HAT 共用）
├── Environment-Sensor-HAT/
│   ├── M1_Environment-Sensor-HAT.sh    ← 本板一键脚本（M1 的差异只在这里）
│   ├── L1_Environment-Sensor-HAT.sh / H1_Environment-Sensor-HAT.sh / M2_Environment-Sensor-HAT.sh
│   ├── README_L1.md / README_M1.md / README_H1.md / README_M2.md   四块板各一份
│   ├── QPi_Environment_Sensor_HAT_SDK/ ← 适配后的 SDK，交付使用这一个
│   │   ├── prebuilt/                   C 例程预编译静态二进制（本板靠它，见第 7 节）
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
本板 Pin 3 / 5 这条总线是 GPIO 软件模拟的（adapter 名 `soc:common_i2c`，编号 `i2c-1`），
**必须先 `sudo 40Pin-ctrl common_i2c enable` 才会出现**（详见第 3 节）。

## 3. 快速使用

**第一步：确认 40Pin 的 I2C 已使能。**

M1 的 40Pin 公共 I2C 是 GPIO 软件模拟的（adapter 名 `soc:common_i2c`），**未使能时
Pin3(SDA1) / Pin5(SCL1) 对应的 `i2c-1` 不会出现**，例程自然找不到 HAT：

```bash
sudo 40Pin-ctrl common_i2c enable    # 使能；状态会持久化，重启后由 40Pin-ctrl --boot 恢复
40Pin-ctrl common_i2c status         # 查看状态：enabled / disabled
ls /dev/i2c-*                        # 使能后才会出现 i2c-1
```

**第二步：安装一次离线依赖（HAT 共用，装过可跳过）：**

```bash
cd <a.Waveshare 所在目录>
chmod +x install_deps.sh            # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./install_deps.sh
```

> 本板没有 dpkg，`install_deps.sh` 会自动整段跳过、不装任何东西（也绝不能装，见第 7、8 节）。

**第三步：执行本板一键脚本（先 `cd` 到 `Environment-Sensor-HAT/`；脚本只检查依赖，不再安装）：**

```bash
cd <a.Waveshare 所在目录>/Environment-Sensor-HAT
chmod +x M1_Environment-Sensor-HAT.sh    # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./M1_Environment-Sensor-HAT.sh
```

脚本依次做四件事：

1. 检查 `packages/` 里的离线依赖是否已装好——**本板没有 dpkg，检查直接跳过**（install_deps.sh 在本板同样不安装）
2. 校验工具链与 Python 运行环境（本板没有 gcc/make，C 例程走 `prebuilt/` 兜底；smbus2 已预装）
3. 自动识别 HAT 插在哪条 I2C 总线上（本板实测 `i2c-1`），并把结果打印出来
4. 部署 C 例程（`prebuilt/` 静态二进制 → `c/main`），随后跑 10 秒自检，直接打印各传感器读数

脚本参数：

| 参数 | 作用 |
|---|---|
| （无） | 检查依赖 + 校验 + 识别 + 准备 C 例程 + 自检 |
| `--no-build` | 跳过 C 例程编译（本板本来就没编译器，无影响） |

安装完成后单独运行例程：

```bash
sudo Environment-Sensor-HAT/QPi_Environment_Sensor_HAT_SDK/c/main                    # C 例程（prebuilt 兜底）
sudo python3 Environment-Sensor-HAT/QPi_Environment_Sensor_HAT_SDK/python/test.py   # Python 例程
```

两个例程都会自动识别总线，无需改代码。

## 4. 本板实测情况

| 项 | 值 |
|---|---|
| 系统 | QTI Linux full reference stack（Yocto 类，**非 Debian**），内核 5.15.180 |
| glibc / Python | **2.35** / **3.10.15** |
| 板上已有 smbus2 | **是（0.4.1）**，Python 例程开箱即用 |
| 板上编译器 | **无，且装不上**（没有 dpkg / apt）→ C 例程自动走 `prebuilt/` |
| HAT 所在总线 | **i2c-1**（40Pin Pin3/Pin5，软件 I2C，需先 40Pin-ctrl 使能） |
| 气压 / 温度 / 湿度 | 1010.3 hPa / 31.9 ℃ / 37.1 % |
| Lux / UVS | 193 / 0 |
| SGP40 原始值 | 28861 |
| ICM20948 | 初始化正常，姿态数据正常 |
| C 例程 | 自动改用 `prebuilt/` 里的预编译二进制，正常输出 |
| Python 例程 | 通过 |

> UVS 在室内接近 0 属正常现象（室内几乎没有 UV），拿到窗边或户外会有明显读数。
> 每块板第一帧的气压值会偏低（如 956 hPa），第二个采样周期起就回到真实值。

## 5. 板型差异如何下发（源码保持统一）

SDK 源码（`c/`、`python/`）对四块开发板**完全相同**，板型参数不写进源码。

本 HAT 走 I2C，不需要 DC/RST 这类引脚参数；总线识别是**通用自动探测**，保留在源码里：

- 先读环境变量 `HAT_I2C_BUS`（手动指定，优先级最高）；
- 否则扫描 `i2c-0` ~ `i2c-15`，用「BME280 在 0x76 能读回 `0xD0 == 0x60`」当探针，命中即用。

一键脚本把「检查依赖 → 校验工具链/运行环境 → 识别总线 → 准备 C 例程/自检」固化下来。
自动识别失败时按脚本提示手动指定即可：

```bash
sudo HAT_I2C_BUS=1 Environment-Sensor-HAT/QPi_Environment_Sensor_HAT_SDK/c/main      # 本板总线为 i2c-1
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
跨平台 / 跨 Python 版本都没有装不上的风险——本板预装的正是 smbus2 0.4.1，开箱即用。

## 7. 离线依赖包与预编译二进制

离线依赖已**统一放在 a.Waveshare/packages/**（45 个 arm64 deb，HAT 共用），
由根目录的 **install_deps.sh** 用 `dpkg -i` 安装（先检测、已装好的自动跳过；缺的跑两遍）。**本板没有 dpkg，该脚本会自动整段跳过、绝不强行安装**
——这类系统的 glibc 比 Debian 13 低，混装会破坏系统（见第 8 节）。

45 个包分三类（给 Debian 板用）：

1. **C 工具链**：`gcc*`、`cpp*`、`binutils*`、`make`、`libc6-dev` 及其依赖
2. **`libc6_2.41-12+deb13u4`**：用于把 `deb13u3` 升级到与 `libc6-dev` 相同的版本
3. **`python3-smbus2_0.4.3-1`**：纯 Python，不需要额外的 C 扩展依赖链（只有本 HAT 用得到）

### prebuilt/ —— 本板依赖的兜底二进制

`prebuilt/main_aarch64_static`（约 790 KB）是一份**静态链接**的 C 例程，内容就是 `c/` 这套源码。

本板没有 gcc/make 且装不上，一键脚本会直接把这份二进制复制成 `c/main` 再运行，跳过编译。
因为是静态链接，它不依赖目标板的 glibc 版本——实测在 glibc 2.35 的本板与 glibc 2.41 的
Debian 板上都能正常运行。

`c/` 源码改动后请重新生成这份二进制（在 Debian 板上）：

```bash
gcc -O2 -static bin/*.o -o main_static -lm
```

## 8. 注意事项与排错

### 在非 Debian 系统（本板 QTI Linux）上的额外注意事项

- **不要试图安装 `packages/` 里的 deb**：本板没有 dpkg/apt，而且 glibc 版本比 Debian 13 低，混装会破坏系统。实测也完全不需要，`install_deps.sh` 与各板脚本都会自动跳过。
- **C 例程**：板上没有 gcc / make / cc / clang，无法就地编译，由 `prebuilt/main_aarch64_static` 兜底。若要换成自己编的版本，在 Debian 板上编静态版再拷过来（见第 7 节）。
  （实测动态版也能直接跑：本程序只用到了很老的 libc 符号，启动只要求 `GLIBC_2.34`，2.35 满足。）
- **Python 例程：开箱即用。** smbus2 已预装（实测 0.4.1），不需要装任何东西，把 `python/` 拷到一个可写目录即可：
  ```bash
  adb shell mkdir -p /data/hat
  adb push Environment-Sensor-HAT/QPi_Environment_Sensor_HAT_SDK/python /data/hat/
  adb shell 'cd /data/hat/python && python3 -u test.py'   # -u 便于看实时输出
  ```
- 文件系统是只读的：`/`、`/home/root`、`/opt` 都写不进去，可写的位置是 `/tmp`、`/data`、`/var/tmp`、`/var/volatile`，程序请放在 `/data` 下。
- `/dev/i2c-*` 的权限是 `crw------- root root`，**必须用 root 运行**（`adb shell` 默认就是 root）。
- 本板 i2c-0 与 i2c-3 不支持 SMBus Quick Write（`i2cdetect -q` 会直接报 “Can't use SMBus Quick Write command on this bus”），i2c-2 上有被驱动占用的设备，HAT 在 i2c-1。这不影响自动识别——程序按序扫过去会直接命中 i2c-1。

### 总线识别

- 优先读环境变量，可手动指定：`sudo HAT_I2C_BUS=1 ./c/main`
- 自动识别的依据是「这条总线上 0x76 能读回 0x60」，也就是 BME280 在线。插针没插紧就会识别失败。

### 权限

- 读写 `/dev/i2c-*` 通常需要 root 或 `i2c` 组：
  ```bash
  sudo usermod -aG i2c $USER   # 之后重新登录
  ```

### 用 i2cdetect 排查时注意

- 请用 **`i2cdetect -y -q 1`**（quick write 探测），不要用 `-r`。
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
- **一键脚本从 Windows 拷过去后不可执行**：执行 `chmod +x M1_Environment-Sensor-HAT.sh`。文件本身已是 LF 行尾，无需转换。
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

本板与 HAT 相关的持久改动只有 40Pin 使能（`40Pin-ctrl common_i2c enable`，状态会持久化），
不再使用时执行 `sudo 40Pin-ctrl common_i2c disable` 即可。