#!/bin/bash
#
# 2.23inch OLED HAT × Quectel Pi H1（QCS6490） 一键脚本
# 用法: sudo ./H1_2.23inch-OLED-HAT.sh [--no-build]（离线依赖先在根目录跑一次 sudo ./install_deps.sh）
# 板型参数（DC/RST 引脚、gpiochip、片选）由本脚本下发，SDK 源码保持统一。
# H1 未开 CONFIG_GPIO_SYSFS：DC/RST 是 gpiochip 线号，例程自动走 GPIO 字符设备后端。
# 详细说明与排错见 2.23inch-OLED-HAT/README_H1.md
#
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/../_common.sh"

# ---- 板型参数：H1（QCS6490），chardev GPIO ----
# DC=33（pin18）/ RST=19（pin22）是 f100000.pinctrl 内的线号，不是 sysfs 全局号
export OLED_DC_GPIO=33
export OLED_RST_GPIO=19
export OLED_GPIO_CHIP=f100000.pinctrl
export OLED_SPI_CS=0        # 默认 CE0（Pin24）；[3/4] 步会在终端里询问 CE0/CE1

HAT_DIR="$WS_ROOT/2.23inch-OLED-HAT"
SDK_DIR="$HAT_DIR/QPi_2.23inch_OLED_HAT_SDK"
C_DIR="$SDK_DIR/c"
PY_DIR="$SDK_DIR/python"
PREBUILT_DIR="$SDK_DIR/prebuilt"

ws_parse_args "$@"
ws_require_root

ws_header "Quectel Pi H1（QCS6490）" "2.23inch OLED HAT"

# ---------------------------------------------------------------- 1/4
echo
echo "==> [1/4] 检查离线依赖包 (.deb)"
ws_check_debs

# ---------------------------------------------------------------- 2/4
echo
echo "==> [2/4] 校验工具链与 Python 运行环境"
ws_check_toolchain
ws_check_python3
[ "$PY_OK" = "1" ] && echo "    python3: $(python3 -V 2>&1)（例程只用标准库，无需额外安装）"

# ---------------------------------------------------------------- 3/4
echo
echo "==> [3/4] 识别 HAT 所用的 SPI 总线与 DC/RST 引脚"

ws_check_board "qcs6490" "Quectel Pi H1（QCS6490）"

# 本 HAT 走 SPI，需要 /dev/spidevX.Y；H1 上用 spi10（/dev/spidev10.0 = CE0）
if ! ls /dev/spidev*.* >/dev/null 2>&1; then
    echo "    当前没有 /dev/spidevX.Y 设备！H1 上用 qpi-config 使能 spi10："
    echo "    在 /etc/qpi-config/qpi-config.ini 里打开 spi10=enable，再执行 sudo qpi-config 40pin set"
    echo
fi

# 片选：CE0 = Pin24，CE1 = Pin26；终端里运行且两个节点都在时询问（回车默认 CE0）
ws_choose_spi_cs

if ! command -v python3 >/dev/null 2>&1; then
    echo "    跳过（需要 python3）；C 例程自己会探测，运行时会把结果打印出来"
else
    HAT_TEST_LOG="$(mktemp)"
    HAT_INFO="$(HAT_PY_DIR="$PY_DIR" python3 -c '
import os, sys
sys.path.insert(0, os.environ["HAT_PY_DIR"])
from drive import hat_hw
dev = os.environ.get("OLED_SPI_DEV") or hat_hw.find_spi_device()
dc, rst = hat_hw.resolve_pins()
print("%s %d %d" % (dev, dc, rst))
' 2>"$HAT_TEST_LOG")"

    if [ -n "$HAT_INFO" ]; then
        set -- $HAT_INFO
        echo "    SPI : $1"
        echo "    GPIO: DC=$2  RST=$3"
    else
        echo "    未能自动识别到 HAT！"
        echo "    检测时的报错信息："
        sed 's/^/      /' "$HAT_TEST_LOG"
        echo "    现有 SPI 设备: $(ls /dev/spidev*.* 2>/dev/null | tr '\n' ' ')"
        echo "    请确认 40Pin 插紧、HAT 已上电后重试，"
        echo "    也可以手动指定后运行，例如:"
        echo "    sudo OLED_SPI_DEV=/dev/spidev10.0 OLED_DC_GPIO=33 OLED_RST_GPIO=19 OLED_GPIO_CHIP=f100000.pinctrl $C_DIR/main"
    fi
    rm -f "$HAT_TEST_LOG"
fi

# ---------------------------------------------------------------- 4/4
echo
echo "==> [4/4] 准备 C 例程可执行文件"

if ws_prepare_main "$C_DIR" "$PREBUILT_DIR"; then
    ws_selfcheck_oled "$C_DIR"
else
    echo "    没有可用的 C 例程（prebuilt/ 里也没有 $(uname -m) 版本）"
fi

ws_run_hint "$C_DIR" "$PY_DIR" "OLED_DC_GPIO=33 OLED_RST_GPIO=19 OLED_GPIO_CHIP=f100000.pinctrl OLED_SPI_CS=$OLED_SPI_CS"