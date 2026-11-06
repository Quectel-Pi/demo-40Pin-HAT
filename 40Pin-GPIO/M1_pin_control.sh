#!/bin/bash
# M1（SM6115 / Bengal）40Pin GPIO 控制脚本（libgpiod v2）
#
# 用法：
#   ./M1_pin_control.sh <pinN|all> <up|down|get>
#   ./M1_pin_control.sh pin18 up
#   ./M1_pin_control.sh pin18 down
#   ./M1_pin_control.sh pin18 get
#   ./M1_pin_control.sh all up
#   ./M1_pin_control.sh all down
#   ./M1_pin_control.sh all get
#
# 引脚映射与 pin_control_u.c 一致。M1 上：
#   TLMM GPIO：全局号 = 398 + offset
#   PM6125 PMIC GPIO：全局号 = 389 + offset
# 脚本运行时按 /sys/class/gpio/gpiochip*/base 找到线号，再通过父设备的
# gpiochip* 子目录换算成 /dev/gpiochipN（sysfs 目录名是 base，不是 chip 编号）。
#
# gpiod v2 的 gpioset 默认会一直持有 line；加 "-t 0" 表示设置后立即退出。
# 已被 I2C/UART/SPI/PWM 占用的引脚会返回 Device or resource busy。

# pin -> sysfs 全局 GPIO 号（与 pin_control_u.c 相同）
PIN_TABLE=(
    "3 507" "5 508" "7 481" "8 467" "10 468" "11 504" "12 496" "13 429"
    "15 469" "16 482" "18 423" "19 399" "21 398" "22 478" "23 400" "24 401"
    "26 480" "27 412" "28 413" "29 403" "31 402" "32 465" "33 396" "35 497"
    "36 414" "37 415" "38 499" "40 498"
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
    command -v "$1" >/dev/null 2>&1 || die "未找到 $1，请确认已安装 libgpiod v2"
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
    local gpio=$1 base_file chip_dir base ngpio parent dev
    for base_file in /sys/class/gpio/gpiochip*/base; do
        [ -e "$base_file" ] || continue
        chip_dir=${base_file%/base}
        read -r base < "$base_file"
        read -r ngpio < "$chip_dir/ngpio"
        if [ "$gpio" -ge "$base" ] && [ "$gpio" -lt $((base + ngpio)) ]; then
            line=$((gpio - base))
            # sysfs 目录名 gpiochip<base> 与字符设备 gpiochip<id> 是两套编号，
            # 字符设备名在同级父设备的 gpiochip<id> 子目录下。
            parent=$(readlink -f "$chip_dir/device")
            for dev in "$parent"/gpiochip*; do
                [ -e "$dev" ] || continue
                chip=$(basename "$dev")
                return 0
            done
            die "无法把 $chip_dir 映射到 /dev/gpiochip*"
        fi
    done
    die "无法把 GPIO$gpio 解析为 gpiochip/line（/sys/class/gpio 不可用？）"
}

# 读取 line 的值（0/1），-a 表示不改变方向
read_line()
{
    gpioget -a --numeric -c "$1" "$2"
}

# 设置引脚电平
set_pin()
{
    local pin=$1 gpio=$2 chip=$3 line=$4 value=$5 actual
    printf 'pin%d (GPIO%d / %s line%d): ' "$pin" "$gpio" "$chip" "$line"

    if ! gpioset -c "$chip" -t 0 "$line=$value"; then
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
    echo "说明: gpioset 使用 -t 0，设置后立即退出并释放 line，不常驻持有。"
}

# 显示帮助信息
show_help()
{
    local prog_name=$1 entry pin gpio chip line i=0
    echo "用法: $prog_name <pin_number|all> <up|down|get>"
    echo "例如: $prog_name pin18 up     # 拉高 pin18"
    echo "例如: $prog_name pin22 down   # 拉低 pin22"
    echo "例如: $prog_name pin3 get     # 读取 pin3"
    echo "例如: $prog_name all up       # 拉高所有 pin"
    echo "例如: $prog_name all down     # 拉低所有 pin"
    echo "例如: $prog_name all get      # 读取所有 pin 状态"
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
