#!/bin/bash
#
# Environment Sensor HAT（SKU 20471）× Quectel Pi H1（QCS6490） 一键脚本
# 用法: sudo ./H1_Environment-Sensor-HAT.sh [--no-build]（离线依赖先在根目录跑一次 sudo ./install_deps.sh）
# 本 HAT 走 I2C：例程用 BME280 当探针自动探测总线，没有板型参数；
# 如自动识别失败，可加 HAT_I2C_BUS=n 手动指定。
# 注：本组合（H1 + 环境传感 HAT）未实测，H1 的 40Pin I2C 由 qpi-config 管理。
# 详见 Environment-Sensor-HAT/README_H1.md
#
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/../_common.sh"

HAT_DIR="$WS_ROOT/Environment-Sensor-HAT"
SDK_DIR="$HAT_DIR/QPi_Environment_Sensor_HAT_SDK"
C_DIR="$SDK_DIR/c"
PY_DIR="$SDK_DIR/python"
PREBUILT_DIR="$SDK_DIR/prebuilt"

ws_parse_args "$@"
ws_require_root

ws_header "Quectel Pi H1（QCS6490）" "Environment Sensor HAT"

# ---------------------------------------------------------------- 1/4
echo
echo "==> [1/4] 检查离线依赖包 (.deb)"
ws_check_debs

# ---------------------------------------------------------------- 2/4
echo
echo "==> [2/4] 校验工具链与 Python 依赖"
ws_check_toolchain
ws_check_python3
ws_ensure_smbus2

# ---------------------------------------------------------------- 3/4
echo
echo "==> [3/4] 识别 HAT 所在的 I2C 总线"

ws_check_board "qcs6490" "Quectel Pi H1（QCS6490）"

# Quectel 系列板的 40Pin 公共 I2C 由 i2c-gpio 模拟（H1 由 qpi-config 管理）
if command -v 40Pin-ctrl >/dev/null 2>&1; then
    I2C_STATE="$(40Pin-ctrl common_i2c status 2>/dev/null)"
    if [ "$I2C_STATE" != "enabled" ]; then
        echo "    注意: 本板 40Pin 的公共 I2C 当前是 '$I2C_STATE'，Pin3/Pin5 对应的 i2c-1 不会出现。"
        echo "          请先执行: sudo 40Pin-ctrl common_i2c enable"
        echo
    fi
fi

HAT_BUS=""
if [ "$PY_OK" = "0" ]; then
    echo "    跳过（需要 python3 + smbus2）"
    echo "    不影响 C 例程：它自己会探测总线，运行时会把结果打印出来"
else
    HAT_TEST_LOG="$(mktemp)"
    HAT_BUS="$(HAT_PY_DIR="$PY_DIR" python3 -c '
import os, sys
sys.path.insert(0, os.environ["HAT_PY_DIR"])
from hat_bus import BUS
print(BUS)
' 2>"$HAT_TEST_LOG")"

    if [ -n "$HAT_BUS" ]; then
        echo "    识别结果: /dev/i2c-$HAT_BUS"
    else
        echo "    未能自动识别到 HAT！"
        echo "    检测时的报错信息："
        sed 's/^/      /' "$HAT_TEST_LOG"
        echo "    现有 I2C 设备: $(ls /dev/i2c-* 2>/dev/null | tr '\n' ' ')"
        echo "    请确认排针插紧、HAT 已上电后重试；本组合未实测，"
        echo "    并请先确认 40Pin 的 I2C 已使能（H1 由 qpi-config 管理）。"
        echo "    也可以手动指定总线运行，例如: HAT_I2C_BUS=n sudo $C_DIR/main"
    fi
    rm -f "$HAT_TEST_LOG"
fi

# ---------------------------------------------------------------- 4/4
echo
echo "==> [4/4] 准备 C 例程可执行文件"

if ws_prepare_main "$C_DIR" "$PREBUILT_DIR"; then
    ws_selfcheck_envsensor "$C_DIR"
else
    echo "    没有可用的 C 例程（prebuilt/ 里也没有 $(uname -m) 版本）"
fi

ws_run_hint "$C_DIR" "$PY_DIR"