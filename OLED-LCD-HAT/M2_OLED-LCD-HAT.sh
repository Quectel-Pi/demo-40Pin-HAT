#!/bin/bash
#
# OLED/LCD HAT (A) × Quectel Pi M2（RK3576） 一键脚本
# 用法: sudo ./M2_OLED-LCD-HAT.sh [--no-build]（离线依赖先在根目录跑一次 sudo ./install_deps.sh）
# 板型参数（DC/RST/BL/按键、SPI/I2C 设备）由本脚本下发，SDK 源码保持统一。
# 详细说明与排错见 OLED-LCD-HAT/README_M2.md
#
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/../_common.sh"

# ---- 板型参数：M2（RK3576），sysfs GPIO ----
export LCD_DC_GPIO=71       # Pin15 / GPIO2_A7
export LCD_RST_GPIO=70      # Pin13 / GPIO2_A6
export LCD_BL_GPIO=87       # Pin12 / GPIO2_C7
export KEY1_GPIO=79         # Pin7  / GPIO2_B7
export KEY2_GPIO=78         # Pin11 / GPIO2_B6
export KEY3_GPIO=132        # Pin16 / GPIO4_A4
export KEY4_GPIO=134        # Pin18 / GPIO4_A6
export OLED_I2C_DEV=/dev/i2c-3
export OLED_I2C_ADDR=0x3C,0x3D   # 本 HAT 有两块 0.96 OLED
export LCD_SPI_MODE=0       # ST7789VW：Mode 0
export LCD_SPI_SPEED=8000000

# M2 的 40Pin SPI 是 spi1（/dev/spidev1.0）；存在时直接指定，避免误选其它 spidev
if [ -e /dev/spidev1.0 ]; then
    export LCD_SPI_DEV=/dev/spidev1.0
fi

HAT_DIR="$WS_ROOT/OLED-LCD-HAT"
SDK_DIR="$HAT_DIR/QPi_OLED_LCD_HAT_SDK"
C_DIR="$SDK_DIR/c"
PREBUILT_DIR="$SDK_DIR/prebuilt"

ws_parse_args "$@"
ws_require_root

ws_header "Quectel Pi M2（RK3576）" "OLED/LCD HAT (A)"

# ---------------------------------------------------------------- 1/4
echo
echo "==> [1/4] 检查离线依赖包 (.deb)"
ws_check_debs

# ---------------------------------------------------------------- 2/4
echo
echo "==> [2/4] 校验工具链"
ws_check_toolchain

# ---------------------------------------------------------------- 3/4
echo
echo "==> [3/4] 识别 HAT 所用的 SPI/I2C 设备与 GPIO"

ws_check_board "rk3576" "Quectel Pi M2（RK3576）"

if ! ls /dev/spidev*.* >/dev/null 2>&1; then
    echo "    当前没有 /dev/spidevX.Y 设备！请先在 40Pin 使能 SPI："
    echo "    sudo qpi-config spi1 enable   （改完需要重启）"
fi

echo "    SPI : ${LCD_SPI_DEV:-自动探测（优先 CE0 = /dev/spidevX.0）}"
echo "    I2C : $OLED_I2C_DEV  addr $OLED_I2C_ADDR（0.96 OLED）"

if [ -e "$OLED_I2C_DEV" ]; then
    echo "    $OLED_I2C_DEV 已存在"
else
    echo "    警告: 找不到 $OLED_I2C_DEV（M2 的 40Pin I2C 应为 /dev/i2c-3）"
fi

echo "    GPIO: DC=$LCD_DC_GPIO  RST=$LCD_RST_GPIO  BL=$LCD_BL_GPIO"
echo "    KEY : $KEY1_GPIO / $KEY2_GPIO / $KEY3_GPIO / $KEY4_GPIO"

# ---------------------------------------------------------------- 4/4
echo
echo "==> [4/4] 准备 C 例程可执行文件"

if ! ws_prepare_main "$C_DIR" "$PREBUILT_DIR"; then
    echo "    没有可用的 C 例程（prebuilt/ 里也没有 $(uname -m) 版本）"
    exit 1
fi

echo
echo "    自检 1/2: ./main 2（2inch LCD，约 4 秒后自动退出）"
echo "    ----------------------------------------------------------"
(cd "$C_DIR" && ./main 2)
rc=$?
echo "    ----------------------------------------------------------"
if [ "$rc" = "0" ]; then
    echo "    自检通过，请目视确认 2inch LCD 是否显示了图形"
else
    echo "    自检异常退出，返回码 $rc"
fi

echo
echo "    自检 2/2: ./main 0.96（0.96 OLED，12 秒后自动结束）"
echo "    ----------------------------------------------------------"
if [ -e "$OLED_I2C_DEV" ]; then
    (cd "$C_DIR" && timeout --preserve-status -s INT -k 2 12 ./main 0.96)
    rc=$?
    echo "    ----------------------------------------------------------"
    if [ "$rc" = "0" ]; then
        echo "    自检通过，请目视确认 0.96 OLED 是否显示了文字"
    else
        echo "    自检异常退出，返回码 $rc"
    fi
else
    echo "    跳过: 找不到 $OLED_I2C_DEV"
fi

ENVS="LCD_DC_GPIO=$LCD_DC_GPIO LCD_RST_GPIO=$LCD_RST_GPIO LCD_BL_GPIO=$LCD_BL_GPIO"
ENVS="$ENVS KEY1_GPIO=$KEY1_GPIO KEY2_GPIO=$KEY2_GPIO KEY3_GPIO=$KEY3_GPIO KEY4_GPIO=$KEY4_GPIO"
ENVS="$ENVS OLED_I2C_DEV=$OLED_I2C_DEV OLED_I2C_ADDR=$OLED_I2C_ADDR"

# 板上没有 sudo（如 QTI Linux 的 root 环境）时直接给命令
SUDO_CMD="sudo "
command -v sudo >/dev/null 2>&1 || SUDO_CMD=""

echo
echo "=============================================================="
echo " 安装完成，运行方式（先 cd $C_DIR）："
echo "   2inch LCD : $SUDO_CMD$ENVS ./main 2"
echo "   0.96 OLED : $SUDO_CMD$ENVS ./main 0.96"
echo "   同时显示  : $SUDO_CMD$ENVS ./main all     （一直运行，Ctrl+C 退出）"
echo "   按键测试  : $SUDO_CMD$ENVS ./main key     （一直运行，Ctrl+C 退出）"
echo "=============================================================="
