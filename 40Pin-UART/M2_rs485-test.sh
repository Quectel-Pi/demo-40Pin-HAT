#!/bin/sh
#
# 40pin-rs485 -- 40-pin UART 转 RS485 单向链路测试（M2 / RK3576）
#
# 拓扑（一块 M2 + 两块 RS485 模块）：
#   M2 Pin8  TXD  -> TX 模块 DI
#   M2 Pin11      -> TX 模块 RSE（高电平发送，低电平接收）
#   M2 Pin10 RXD  <- RX 模块 RO
#   RX 模块 RSE   -> GND（常接收）
#   两模块 A-A、B-B；VCC 按模块版本接 3V3/5V；GND 共地
#
# 说明：
#   - M2 的 40Pin 只引出一路 UART（Pin8/Pin10），一块 M2 无法做真正的
#     半双工双节点轮次；本脚本用一块模块发、另一块模块收，验证差分
#     驱动/接收链路。
#   - 推荐 5V 模块做 TX（DI/RSE 可识别 3.3V 电平）、3.3V 模块做 RX
#     （RO 直接接 M2）。反过来用 5V 模块做 RX 时，RO 必须经 1k/2k 分压
#     后再接 M2 RXD，否则 5V 会损坏引脚。
#   - TX 模块 RSE 由 Pin11（GPIO78 / gpiochip2 line14）控制，测试期间保持
#     高电平，退出时拉低；RX 模块 RSE 硬件接 GND。
#
# 用法：
#   sudo ./M2_rs485-test.sh
#   sudo ./M2_rs485-test.sh -b "9600 115200"
#   sudo ./M2_rs485-test.sh -d /dev/ttyS1 -c gpiochip2 -l 14
#
# Android：
#   adb push M2_rs485-test.sh /data/local/tmp/
#   adb shell
#   su
#   sh /data/local/tmp/M2_rs485-test.sh
#
# 退出码：0 全部 PASS，1 有 FAIL，2 参数/环境错误。

PROG=${0##*/}

DEV=/dev/ttyS1
TIMEOUT=1.5
BAUDS="9600 38400 115200 230400 460800 921600 1500000"
GPIO_CHIP=gpiochip2
GPIO_LINE=14
GPIO_PIN=11
GPIO_NUM=78

CASE_DETAIL=
RSE_PID=
WORKDIR=

usage() {
    cat <<EOF
用法: $PROG [选项]

在 $DEV 上测试 M2 40Pin UART 经 RS485 模块的发送/接收链路。
需要两块 RS485 模块：一块只做 TX，一块只做 RX。

选项:
    -d DEV      串口设备（默认 $DEV）
    -c CHIP     RSE 所在 gpiochip（默认 $GPIO_CHIP）
    -l LINE     RSE 所在 line（默认 $GPIO_LINE，即 Pin$GPIO_PIN / GPIO$GPIO_NUM）
    -b "BAUDS"  波特率列表，空格分隔（默认 "$BAUDS"）
    -h          显示本帮助

接线（推荐：5V 模块做 TX，3.3V 模块做 RX）:
    M2 Pin8  TXD  -> 5V 模块 DI
    M2 Pin11      -> 5V 模块 RSE
    M2 Pin10 RXD  <- 3.3V 模块 RO
    3.3V 模块 RSE -> GND
    5V 模块 VCC   -> Pin2/Pin4 (5V)
    3.3V 模块 VCC -> Pin1/Pin17 (3V3)
    两模块 GND 共地，A-A、B-B；板载 120Ω，两个节点约 60Ω

模块 5 针顺序（丝印）: VCC、GND、RO、DI、RSE。

注意:
    - 接线或换线前先断开开发板电源。
    - 5V 模块做 RX 时，RO 必须经 1k/2k 分压再接 M2 RXD，否则 5V 会损坏引脚。
    - 本测试是单向链路，不验证半双工方向切换和总线冲突；反向测试需交换
      TX/RX 模块（5V 模块做 RX 时加分压）。
    - 需要 root 和 libgpiod v2 的 gpioset。
    - GPIO 全局号换算: gpiochip = GPIO / 32，line = GPIO % 32；
      默认 GPIO$GPIO_NUM -> $GPIO_CHIP line$GPIO_LINE。
EOF
}

die() {
    printf '%s\n' "$*" >&2
    exit 2
}

rse_stop() {
    if [ -n "$RSE_PID" ]; then
        kill "$RSE_PID" 2>/dev/null
        wait "$RSE_PID" 2>/dev/null
        RSE_PID=
    fi
}

# 保持 RSE 电平：gpioset v2 默认持有 line，进程被 kill 后释放
rse_hold() {
    rse_stop
    gpioset -c "$GPIO_CHIP" "$GPIO_LINE=$1" &
    RSE_PID=$!
    sleep 0.1
    if ! kill -0 "$RSE_PID" 2>/dev/null; then
        RSE_PID=
        die "无法控制 $GPIO_CHIP line$GPIO_LINE 作为 RSE。
       请确认已安装 libgpiod v2（gpioset 支持 -c），且该 GPIO 未被其他程序占用。"
    fi
}

cleanup() {
    rse_stop
    gpioset -c "$GPIO_CHIP" -t 0 "$GPIO_LINE=0" >/dev/null 2>&1 || true
    [ -n "$WORKDIR" ] && rm -rf "$WORKDIR"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        -d)
            [ "$#" -ge 2 ] || die "-d 需要参数"
            DEV=$2
            shift 2
            ;;
        -c)
            [ "$#" -ge 2 ] || die "-c 需要参数"
            GPIO_CHIP=$2
            shift 2
            ;;
        -l)
            [ "$#" -ge 2 ] || die "-l 需要参数"
            GPIO_LINE=$2
            shift 2
            ;;
        -b)
            [ "$#" -ge 2 ] || die "-b 需要参数"
            BAUDS=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
done

[ -n "$BAUDS" ] || die "波特率列表为空"

[ "$(id -u)" -eq 0 ] || die "请以 root 运行（Linux: sudo $0；Android: 先 su）"

[ -e "$DEV" ] || die "$DEV 不存在。
       请确认 40Pin UART（Pin8/Pin10）已启用"

command -v stty >/dev/null 2>&1 || die "未找到 stty"
command -v dd >/dev/null 2>&1 || die "未找到 dd"
command -v cmp >/dev/null 2>&1 || die "未找到 cmp"
command -v od >/dev/null 2>&1 || die "未找到 od"
command -v timeout >/dev/null 2>&1 || die "未找到 timeout"
command -v gpioset >/dev/null 2>&1 || die "未找到 gpioset，请安装 libgpiod v2"

# 临时目录：优先 $TMPDIR，其次 Android 常用目录，最后当前目录
TMP=
for d in "${TMPDIR:-}" /data/local/tmp /tmp .; do
    if [ -n "$d" ] && [ -d "$d" ] && [ -w "$d" ]; then
        TMP=$d
        break
    fi
done
[ -n "$TMP" ] || die "找不到可写的临时目录"
WORKDIR=$(mktemp -d "$TMP/40pin-rs485.XXXXXX") || die "无法创建临时目录"

make_payload() {
    # 0xA5 0x5A 0x00 0xFF 0x55 0xAA 0x01 0xFE "LOOP" x2 + 前 8 字节 = 32
    printf '\245\132\000\377\125\252\001\376LOOP\245\132\000\377\125\252\001\376LOOP\245\132\000\377\125\252\001\376' > "$1"
}

hex_string() {
    od -An -v -tx1 "$1" 2>/dev/null | awk '{ for (i = 1; i <= NF; i++) printf "%s", $i }'
}

uart_config() {
    cfg_baud=$1
    stty -F "$DEV" "$cfg_baud" raw cs8 -cstopb -parenb cread clocal -crtscts -echo 2>/dev/null
}

rs485_roundtrip() {
    rt_baud=$1
    rt_want=$WORKDIR/want.bin
    rt_got=$WORKDIR/got.bin

    if ! uart_config "$rt_baud"; then
        CASE_DETAIL="无法把 $DEV 配置为 $rt_baud baud"
        return 1
    fi

    # 丢弃上一档波特率留下的数据
    timeout 0.2 dd if="$DEV" of=/dev/null bs=64 count=1 2>/dev/null || true

    : > "$rt_got"
    # 先启动读端并保持端口打开：如果写完后最后一个 fd 关闭，n_tty 会释放
    # 读缓冲，回环收到的数据会被丢掉，后面的读就永远收不到数据。
    timeout "$TIMEOUT" dd if="$DEV" of="$rt_got" bs=1 count=32 2>/dev/null &
    rt_rx_pid=$!
    sleep 0.2
    timeout "$TIMEOUT" dd if="$rt_want" of="$DEV" bs=32 count=1 2>/dev/null || true
    wait "$rt_rx_pid" || true

    if cmp -s "$rt_want" "$rt_got"; then
        CASE_DETAIL=
        return 0
    fi

    rt_size=$(wc -c < "$rt_got" | tr -d ' ')
    if [ "$rt_size" -eq 0 ]; then
        CASE_DETAIL="未收到数据"
        return 1
    fi
    if [ "$rt_size" -ne 32 ]; then
        CASE_DETAIL="收到 $rt_size 字节，期望 32"
        return 1
    fi

    rt_cmp=$(cmp -l "$rt_want" "$rt_got" 2>/dev/null | head -n 1)
    if [ -n "$rt_cmp" ]; then
        read -r rt_byte rt_sent rt_recv <<EOF
$rt_cmp
EOF
        CASE_DETAIL=$(printf '第 %d 字节: 发送 0x%02X 收到 0x%02X' \
            "$((rt_byte - 1))" "$((0$rt_sent))" "$((0$rt_recv))")
    else
        CASE_DETAIL="数据不一致"
    fi
    return 1
}

make_payload "$WORKDIR/want.bin" || die "无法生成测试数据"
size=$(wc -c < "$WORKDIR/want.bin" | tr -d ' ')
[ "$size" -eq 32 ] || die "内部错误: 测试数据为 $size 字节，期望 32"

printf 'device : %s\n' "$DEV"
printf 'gpio   : %s line%s (TX 模块 RSE)\n' "$GPIO_CHIP" "$GPIO_LINE"
printf 'wiring : TX: DI<-Pin8, RSE<-GPIO; RX: RO->Pin10, RSE->GND; A-A, B-B\n'
printf 'data   : %s\n' "$(hex_string "$WORKDIR/want.bin")"
printf '\n'

# 先让发送模块处于接收态；配置第一档波特率后再打开发送使能
rse_hold 0
first_baud=
for b in $BAUDS; do
    first_baud=$b
    break
done
[ -n "$first_baud" ] || die "波特率列表为空"
uart_config "$first_baud" || die "无法把 $DEV 配置为 $first_baud baud"
rse_hold 1

: > "$WORKDIR/results"
for baud in $BAUDS; do
    if rs485_roundtrip "$baud"; then
        printf '%s|1|\n' "$baud" >> "$WORKDIR/results"
    else
        printf '  %s baud FAIL: %s\n' "$baud" "$CASE_DETAIL"
        printf '%s|0|%s\n' "$baud" "$CASE_DETAIL" >> "$WORKDIR/results"
    fi
done

# 测试结束，释放总线
rse_hold 0

printf '\n  %-22s %s\n' "case" "result"
printf '  %s\n' "----------------------------------"
bad=0
total=0
while IFS='|' read -r label ok detail; do
    total=$((total + 1))
    if [ "$ok" = 1 ]; then
        printf '  %-22s PASS\n' "$label baud"
    else
        printf '  %-22s FAIL   %s\n' "$label baud" "$detail"
        bad=$((bad + 1))
    fi
done < "$WORKDIR/results"

printf '\n'
if [ "$bad" -ne 0 ]; then
    printf 'RESULT : FAIL - %d/%d cases failed\n' "$bad" "$total"
    printf '         检查 TX/RX 模块是否接反、A/B 是否对调、RSE 是否接对；\n'
    printf '         5V 模块做 RX 时，RO 必须经 1k/2k 分压再接 Pin10。\n'
    exit 1
fi
printf 'RESULT : PASS - all %d cases matched\n' "$total"
exit 0
