# demo-40Pin-HAT —— Waveshare HAT × Quectel Pi 适配总览

> 本目录汇总 Waveshare 外设 HAT 在 Quectel Pi 系列开发板（L1 / M1 / H1 / M2）上的适配：
> SDK 源码保持统一，离线依赖与一键脚本共用。
> 每个 HAT 的详细说明（接线、排错、实测数据）在该 HAT 目录下的 **README_<板>.md** 里。

---

## 1. 支持的开发板

| 板 | 平台 | 系统 |
|---|---|---|
| Quectel Pi L1 | QCM2290（Qualcomm Scuba） | Linux/Debian/Android |
| Quectel Pi M1 | SM6115（Qualcomm BENGAL） | Linux/Debian/Android |
| Quectel Pi H1 | QCS6490 | Linux/Debian/Android |
| Quectel Pi M2 | RK3576 | Linux/Debian/Android |

## 2. 支持的 HAT

| HAT                               | 总线                                                   | 适配 SDK                           | 一键脚本（4 块板各一个）                   | 详细文档                                   |
| --------------------------------- | ---------------------------------------------------- | -------------------------------- | ------------------------------- | -------------------------------------- |
| 2.23inch OLED HAT                 | SPI（SSD1305，128×32）                                  | `QPi_2.23inch_OLED_HAT_SDK`      | `<板>_2.23inch-OLED-HAT.sh`      | `2.23inch-OLED-HAT/README_<板>.md`      |
| Environment Sensor HAT（SKU 20471） | I2C（BME280 / TSL2591 / LTR-390UV / SGP40 / ICM20948） | `QPi_Environment_Sensor_HAT_SDK` | `<板>_Environment-Sensor-HAT.sh` | `Environment-Sensor-HAT/README_<板>.md` |

> **新增 HAT 时**：在「支持的 HAT」表与该表下方的「各板适配状态」表末尾各追加一行，并按第 6 节的约定补齐目录、脚本与逐板文档（位置已预留）。

## 3. 硬件连接（40Pin）总览

| HAT | 直扣 40Pin 卡座 | 说明 |
|---|---|---|
| 2.23inch OLED HAT | 仅 **M2** 可以直扣 | **L1 / M1 / H1** 只能通过杜邦线把 HAT 板上的引脚与 40Pin 对应连接；原因待硬件排查中 |
| Environment Sensor HAT | **所有开发板**都可以直扣 | 扣板接通的即 Pin 3 / Pin 5 这组 I2C；**Pin 27 / Pin 28** 这组 I2C 也可以把 HAT 板与 40Pin 用杜邦线对应连接后使用 |

两个 HAT 的 40Pin 占用：

- **2.23inch OLED HAT**：SPI 一组（Pin 19 DIN / Pin 23 CLK / **Pin 24 CS**，可选 Pin 26 CE1）+ Pin 18 DC、Pin 22 RST，加上电源与 GND；
  **只吃 Pin 2 / 4 的 5V，不吃 3.3V**。
- **Environment Sensor HAT**：Pin 3 / Pin 5 这组 I2C（5 个传感器全部接在这一组），加上常规电源与 GND。

> 逐引脚接线表（含各板 GPIO 号）见对应目录 README 的「2. 硬件连接」一节。

## 4. 快速使用

**第一步：把整个 `demo-40Pin-HAT` 目录拷到开发板**（至少要包含所选 HAT 目录（含其中一键脚本与 `QPi_*_SDK/`）、共用的 `packages/` 以及根目录的 `install_deps.sh` / `_common.sh`）。

**第二步：安装一次离线依赖（HAT 共用，装过可跳过）：**

```bash
cd <demo-40Pin-HAT 所在目录>
chmod +x install_deps.sh    # 从 Windows 拷过去可能丢可执行位，务必先执行
sudo ./install_deps.sh
```

**第三步：执行本板一键脚本（脚本只检查依赖，不再安装）：**

```bash
cd <HAT 目录>                         # 如 cd 2.23inch-OLED-HAT
chmod +x L1_2.23inch-OLED-HAT.sh      # 换成要用的那一个
sudo ./L1_2.23inch-OLED-HAT.sh
```

> QTI Linux（M1 / L1）默认就是 root shell、没有 `sudo`，直接执行 `./<板>_<HAT>.sh` 即可。

一键脚本依次做四件事：

1. 检查 `packages/` 里的离线依赖是否已装好（缺失时会提示先跑一次根目录的 `sudo ./install_deps.sh`）
2. 校验工具链与 Python 运行环境
3. 识别 HAT 所在总线与引脚（板型参数由脚本下发，SDK 源码保持统一；OLED 走 SPI 时可在终端里选 CE0/CE1）
4. 编译 C 例程生成 `c/main`（没有编译器时自动改用 `prebuilt/` 静态二进制），随后跑自检

脚本参数：

| 参数 | 作用 |
|---|---|
| （无） | 检查依赖 + 校验 + 识别 + 编译 + 自检 |
| `--no-build` | 跳过 C 例程编译（`c/main` 不存在时仍会取 `prebuilt/` 兜底） |

> `install_deps.sh` 的规则：45 个 arm64 deb 中已装好的自动跳过（已装版本不低于包内版本才跳过，不会降级）；
> 非 Debian 系统（无 dpkg，如 M1 / L1 的 QTI Linux）整段自动跳过，绝不强行安装。

## 5. 目录结构

```
demo-40Pin-HAT/
├── README.md                         本文件：总览与新增 HAT 约定
├── install_deps.sh                   离线依赖安装脚本（HAT 共用，装一次即可）
├── _common.sh                        一键脚本共用函数库（无需单独执行）
├── packages/                         45 个 arm64 离线 deb 依赖（HAT 共用）
├── 2.23inch-OLED-HAT/
│   ├── <板>_2.23inch-OLED-HAT.sh      2.23inch OLED HAT 一键脚本（L1/M1/H1/M2 共 4 个）
│   ├── README_L1.md / README_M1.md / README_H1.md / README_M2.md   四块板各一份
│   ├── QPi_2.23inch_OLED_HAT_SDK/    适配后的 SDK（c/ + python/ + prebuilt/）
│   └── 原厂资料（源码压缩包 / 原理图 / datasheet，未做改动）
├── Environment-Sensor-HAT/
│   ├── <板>_Environment-Sensor-HAT.sh 环境传感 HAT 一键脚本（L1/M1/H1/M2 共 4 个）
│   ├── README_L1.md / README_M1.md / README_H1.md / README_M2.md   四块板各一份
│   ├── QPi_Environment_Sensor_HAT_SDK/
│   └── 原厂资料
└── <后续新增的 HAT 目录放在这一层>    ← 见第 6 节
```

## 6. 新增一个 HAT（预留位置）

后续新增 HAT 时，按以下约定加入，本项目其余内容无需改动：

1. **HAT 目录**：新建 `<HAT名>/`（与 `2.23inch-OLED-HAT/` 平级），内含：
   - `README_L1.md / README_M1.md / README_H1.md / README_M2.md`：四块板各一份说明；
   - `QPi_<HAT名>_SDK/`：适配后的 SDK（**SDK 目录固定加 `QPi_` 前缀**）；
   - 原厂源码压缩包、原理图、datasheet 等资料原样保留。
2. **一键脚本**：在该 HAT 目录下新增 4 个脚本 `<板>_<HAT名>.sh`（L1 / M1 / H1 / M2 各一个），source 上一层目录的 `_common.sh`，板型差异只放在对应脚本里。
3. **依赖**：如需要新的系统依赖，把 arm64 deb 放进 `packages/`（HAT 共用，`install_deps.sh` 无需改动）。
4. **登记**：在第 2 节「支持的 HAT」表末尾追加一行：

```markdown
| <HAT 名称> | <总线> | QPi_<HAT名>_SDK | `<板>_<HAT名>.sh` | <HAT目录>/README_<板>.md |
```

> 已登记的新 HAT：（暂无）