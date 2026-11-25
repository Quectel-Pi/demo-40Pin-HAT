#!/bin/bash
# M2（RK3576）40Pin GPIO 控制脚本（兼容 libgpiod v1.6+ / v2）
#
# 用法：
#   ./M2_pin_control.sh <pinN|all> <up|down|get>
#   ./M2_pin_control.sh pin18 up
#   ./M2_pin_control.sh pin18 down
#   ./M2_pin_control.sh pin18 get
#   ./M2_pin_control.sh all up
#   ./M2_pin_control.sh all down
#   ./M2_pin_control.sh all get
#
# “40PINS引脚对应分布”表。M2（RK3576）的 GPIO 全局号按 Rockchip bank 编号：
#   gpiochip = 全局号 / 32，line = 全局号 % 32
#
# 表中 28 个 GPIO 都已收录。Pin 3/5/8/10/19/21/23/24/26/27/28 默认复用为
# I2C3/UART1/SPI1/I2C7，需先用 qpi-config 释放对应功能，否则 gpioset 会报
# Device or resource busy。
#
# v2 的 gpioset 默认会一直持有 line，需加 "-t 0" 才立即退出；v1 默认 mode=exit。

# pin -> 全局 GPIO 号（由表中的 GPIOx_yz 名称换算）
PIN_TABLE=(
    "3 140" "5 141" "7 79" "8 72" "10 73" "11 78" "12 87" "13 70"
    "15 71" "16 132" "18 134" "19 82" "21 83" "22 76" "23 85" "24 84"
    "26 81" "27 97" "28 96" "29 93" "31 92" "32 77" "33 91" "35 86"
    "36 90" "37 95" "38 88" "40 89"
)

die()
{
    echo "错误: $*" >&2
    exit 1
}

require_root()
{
    [ "$(id -u)" -eq 0 ] || die "需要 root 权限运行"
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 || die "未找到 $1，请确认已安装 libgpiod（v1.6+ 或 v2）"
}

# libgpiod v1 与 v2 的 gpioset/gpioget 命令行语法不同，按主版本号分别处理
detect_gpiod_major()
{
    local major
    major=$(gpioset --version 2>/dev/null | head -1 | \
        awk '{print $NF}' | tr -d 'v' | cut -d. -f1)
    case "$major" in
    ''|*[!0-9]*) die "无法识别 libgpiod 版本（gpioset --version 输出异常）" ;;
    esac
    echo "$major"
}

# 设置 line 电平：v2 需 -t 0 才会设置后立即退出，v1 默认 mode=exit
write_line()
{
    local chip=$1 line=$2 value=$3
    if [ "$GPIOD_MAJOR" -ge 2 ]; then
        gpioset -c "$chip" -t 0 "$line=$value"
    else
        gpioset "$chip" "$line=$value"
    fi
}

# 根据 pin 号查找 GPIO 全局号
lookup_gpio()
{
    local pin=$1 entry p g
    for entry in "${PIN_TABLE[@]}"; do
        read -r p g <<< "$entry"
        [ "$p" -eq "$pin" ] && { echo "$g"; return 0; }
    done
    return 1
}

# GPIO 全局号 -> gpiochip + line offset
resolve_gpio()
{
    local gpio=$1
    chip=gpiochip$((gpio / 32))
    line=$((gpio % 32))
}

# 读取 line 的值（0/1）
# v2 用 -a 保持原配置；v1 读取会把 line 请求为输入，读后按原方向恢复
read_line()
{
    local chip=$1 line=$2 dir value
    if [ "$GPIOD_MAJOR" -ge 2 ]; then
        gpioget -a --numeric -c "$chip" "$line"
        return
    fi
    dir=$(gpioinfo "$chip" 2>/dev/null | awk -v line="$line" \
        '$1 == "line" && $2 == line ":" { print $5 }')
    value=$(gpioget "$chip" "$line") || return 1
    if [ "$dir" = output ]; then
        gpioset "$chip" "$line=$value" >/dev/null 2>&1 || true
    fi
    printf '%s\n' "$value"
}

# 设置引脚电平
set_pin()
{
    local pin=$1 gpio=$2 chip=$3 line=$4 value=$5 actual
    printf 'pin%d (GPIO%d / %s line%d): ' "$pin" "$gpio" "$chip" "$line"

    if ! write_line "$chip" "$line" "$value"; then
        echo "✗ 设置失败"
        return 1
    fi
    echo "✓ 已设置为$([ "$value" -eq 1 ] && echo 高 || echo 低)电平"

    if actual=$(read_line "$chip" "$line"); then
        echo "  验证: 当前$([ "$actual" = 1 ] && echo 高 || echo 低)电平 (值: $actual)"
    fi
    return 0
}

# 读取引脚状态
get_pin()
{
    local pin=$1 gpio=$2 chip=$3 line=$4 value
    printf 'pin%d (GPIO%d / %s line%d): ' "$pin" "$gpio" "$chip" "$line"

    if ! value=$(read_line "$chip" "$line"); then
        echo "✗ 读取失败"
        return 1
    fi

    case "$value" in
    0|1)
        echo "✓ 当前$([ "$value" = 1 ] && echo 高 || echo 低)电平 (值: $value)"
        ;;
    *)
        echo "✗ 读取失败（无法解析返回值: $value）"
        return 1
        ;;
    esac
    return 0
}

# 控制所有引脚
control_all_pins()
{
    local action=$1 entry pin gpio chip line success=0 total=0
    echo "========================================"
    echo "开始控制所有引脚为$([ "$action" -eq 1 ] && echo 高 || echo 低)电平..."
    echo "========================================"

    for entry in "${PIN_TABLE[@]}"; do
        read -r pin gpio <<< "$entry"
        resolve_gpio "$gpio"
        total=$((total + 1))
        set_pin "$pin" "$gpio" "$chip" "$line" "$action" && success=$((success + 1))
    done

    echo "========================================"
    echo "成功控制 $success/$total 个引脚"
    echo "========================================"
    [ "$success" -eq "$total" ]
}

# 读取所有引脚状态
read_all_pins()
{
    local entry pin gpio chip line success=0 total=0
    echo "========================================"
    echo "=== 读取所有引脚状态 ==="
    echo "========================================"

    for entry in "${PIN_TABLE[@]}"; do
        read -r pin gpio <<< "$entry"
        resolve_gpio "$gpio"
        total=$((total + 1))
        get_pin "$pin" "$gpio" "$chip" "$line" && success=$((success + 1))
    done

    echo "========================================"
    echo "成功读取 $success/$total 个引脚"
    echo "========================================"
}

# 说明 gpioset 的持有行为
print_hold_note()
{
    if [ "$GPIOD_MAJOR" -ge 2 ]; then
        echo "说明: gpioset 使用 -t 0，设置后立即退出并释放 line，不常驻持有。"
    else
        echo "说明: gpioset 默认设置后立即退出并释放 line，不常驻持有。"
    fi
}

# 显示帮助信息
show_help()
{
    local prog_name=$1 entry pin gpio chip line i=0
    echo "用法: $prog_name <pin_number|all> <up|down|get>"
    echo "例如: $prog_name pin18 up     # 拉高 pin18"
    echo "例如: $prog_name pin22 down   # 拉低 pin22"
    echo "例如: $prog_name pin7 get     # 读取 pin7"
    echo "例如: $prog_name all up       # 拉高所有 pin"
    echo "例如: $prog_name all down     # 拉低所有 pin"
    echo "例如: $prog_name all get      # 读取所有 pin 状态"
    echo
    echo "注: Pin 3/5/8/10/19/21/23/24/26/27/28 默认复用为 I2C3/UART1/SPI1/I2C7，"
    echo "    需先用 qpi-config 释放对应功能，否则 gpioset 会报 Device or resource busy。"
    echo
    print_hold_note
    echo
    echo "支持的引脚:"

    for entry in "${PIN_TABLE[@]}"; do
        read -r pin gpio <<< "$entry"
        resolve_gpio "$gpio"
        printf 'pin%d(GPIO%d/%s:%d)  ' "$pin" "$gpio" "$chip" "$line"
        i=$((i + 1))
        [ $((i % 4)) -eq 0 ] && echo
    done
    echo
}

if [ $# -lt 2 ]; then
    show_help "$0"
    exit 1
fi

pin_str=$1
action_str=$2

case "$action_str" in
up) action=1 ;;
down) action=0 ;;
get) action=2 ;;
*) die "动作必须是 up, down 或 get" ;;
esac

require_root
require_command gpioset
require_command gpioget
require_command gpioinfo

GPIOD_MAJOR=$(detect_gpiod_major)

print_hold_note

if [ "$pin_str" = "all" ]; then
    if [ "$action" -eq 2 ]; then
        read_all_pins
        exit 0
    fi
    control_all_pins "$action"
    exit $?
fi

case "$pin_str" in
pin*) pin=${pin_str#pin} ;;
*) die "无效的引脚格式，请使用 pin3, pin5 等" ;;
esac

case "$pin" in
''|*[!0-9]*) die "无效的引脚格式，请使用 pin3, pin5 等" ;;
esac

gpio=$(lookup_gpio "$pin") || die "不支持的引脚 pin$pin"
resolve_gpio "$gpio"

if [ "$action" -eq 2 ]; then
    get_pin "$pin" "$gpio" "$chip" "$line"
else
    set_pin "$pin" "$gpio" "$chip" "$line" "$action"
fi
