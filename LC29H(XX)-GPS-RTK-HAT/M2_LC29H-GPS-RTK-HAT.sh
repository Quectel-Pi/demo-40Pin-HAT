#!/bin/sh
#
# LC29H(XX) GPS/RTK HAT × Quectel Pi M2（RK3576） 一键脚本
#
# 用法: sudo ./M2_LC29H-GPS-RTK-HAT.sh [-t 秒]
#
# 直扣 40Pin 使用，运行前确认硬件：
#   * J1 上两个黄色跳线帽竖插在丝印 B 档（40Pin 直连模组）；
#     A 档是板载 CP2102 USB-UART 直连模组，C 档是 USB-UART 直连 40Pin
#   * L1/L5 双频天线接到板载 IPEX 1 代座
#   * 板上 micro-USB 不要插（USB_5V 与 Pin2/Pin4 是同一路 5V）
#
# 本脚本只读不写：配置 /dev/ttyS1 为 115200 8N1，抓取 NMEA 0183，
# 校验校验和并解析 GGA，再按需用 gpiomon 检查 Pin12 上的 PPS 秒脉冲。
#
# 退出码：0 通过，1 有失败项，2 环境/用法错误。

set -u

PROG=${0##*/}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/../_common.sh"

DEV=${LC29H_UART_DEV:-/dev/ttyS1}
BAUD=115200
CAPTURE_SEC=8
PPS_CHIP=gpiochip2
PPS_LINE=23
PPS_PIN=12
PPS_GPIO=87

WORKDIR=
cleanup() {
    [ -n "$WORKDIR" ] && rm -rf "$WORKDIR"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

usage() {
    cat <<EOF
用法: $PROG [选项]

在 $DEV 上读取 LC29H(XX) 模块输出的 NMEA 0183，校验语句并解析定位状态。

选项:
    -t SEC      抓取时长，默认 $CAPTURE_SEC 秒
    -h          显示本帮助

硬件前提:
    * J1 两个黄色跳线帽竖插在丝印 B 档（不能横插）
    * 天线接到板载 IPEX 1 代座，天线朝向可见天空
    * 板上 micro-USB 不要插

说明:
    M2 的 40Pin UART 为 Pin8 (TXD) / Pin10 (RXD)，对应 $DEV。
    模块默认 115200 8N1、NMEA 输出 1Hz；PPS 在 Pin$PPS_PIN（GPIO$PPS_GPIO，
    即 $PPS_CHIP line$PPS_LINE），定位后输出 1Hz 秒脉冲。
EOF
}

die() {
    printf '%s\n' "$*" >&2
    exit 2
}

while getopts "t:h" opt; do
    case "$opt" in
        t) CAPTURE_SEC=$OPTARG ;;
        h) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

case "$CAPTURE_SEC" in
    ''|*[!0-9]*) die "抓取时长必须是正整数: $CAPTURE_SEC" ;;
esac
[ "$CAPTURE_SEC" -ge 1 ] || die "抓取时长至少 1 秒"

ws_require_root
echo "=============================================================="
echo " LC29H(XX) GPS/RTK HAT 一键脚本 —— Quectel Pi M2（RK3576）"
echo "=============================================================="
ws_check_board "rk3576" "Quectel Pi M2（RK3576）"

# ---------------------------------------------------------------- 1/3
echo
echo "==> [1/3] 检查串口环境"

[ -e "$DEV" ] || die "$DEV 不存在。
        M2 的 40Pin UART 为 Pin8 (TXD) / Pin10 (RXD)，设备节点应为 $DEV；
        若节点缺失请先确认系统镜像的 40Pin UART 已使能。"
command -v stty >/dev/null 2>&1 || die "未找到 stty，无法配置串口"
command -v awk  >/dev/null 2>&1 || die "未找到 awk，无法解析 NMEA"
echo "    串口: $DEV @ $BAUD 8N1"

TMP_BASE=${TMPDIR:-/tmp}
[ -d "$TMP_BASE" ] || TMP_BASE=$PWD
WORKDIR=$(mktemp -d "$TMP_BASE/lc29h.XXXXXX") || die "无法创建临时目录"
NMEA=$WORKDIR/nmea.txt
STATS=$WORKDIR/stats.txt
PPS=$WORKDIR/pps.txt
PPS_ERR=$WORKDIR/pps.err

# ---------------------------------------------------------------- 2/3
echo
echo "==> [2/3] 抓取 NMEA（${CAPTURE_SEC} 秒）"

if ! stty -F "$DEV" "$BAUD" raw -echo -crtscts clocal 2>/dev/null; then
    die "配置 $DEV 失败（串口被占用或权限不足）"
fi

# 丢掉上一轮残留；dd 按块落盘，timeout 杀掉时已收数据不会丢
timeout 0.3 dd if="$DEV" of=/dev/null bs=64 count=1 2>/dev/null || true
timeout "$CAPTURE_SEC" dd if="$DEV" of="$NMEA" bs=256 count=$((CAPTURE_SEC * 45)) 2>/dev/null || true

if [ ! -s "$NMEA" ]; then
    echo
    echo "RESULT : FAIL - ${CAPTURE_SEC} 秒内没有收到任何数据"
    echo "         1) 跳线帽是否竖插在丝印 B 档？（A 档时 UART 走板载 USB-UART）"
    echo "         2) 天线是否接到 IPEX 座？"
    echo "         3) 板上 micro-USB 是否插着？"
    exit 1
fi
echo "    已抓取 $(wc -c < "$NMEA" | tr -d ' ') 字节"

# 抓包从数据流中间开始/结束，首尾各有一行可能是半截语句，解析前去掉
sed '1d;$d' "$NMEA" > "$WORKDIR/nmea_trim.txt"

# ---------------------------------------------------------------- 3/3
echo
echo "==> [3/3] 校验 NMEA 并解析定位"

cat > "$WORKDIR/parse.awk" <<'AWK'
BEGIN {
    for (i = 32; i < 127; i++) ORD[sprintf("%c", i)] = i
    valid = 0; bad = 0; gga = 0; quality = -1; sats = -1
    lat = ""; ns = ""; lon = ""; ew = ""; alt = ""; utc = ""; types = ""
}
function hexval(c) { return index("0123456789ABCDEF", toupper(c)) - 1 }
function xor8(a, b,    r, bit) {
    r = 0; bit = 1
    while (a > 0 || b > 0) {
        if ((a % 2) != (b % 2)) r += bit
        a = int(a / 2); b = int(b / 2); bit *= 2
    }
    return r
}
{
    line = $0
    sub(/\r$/, "", line)
    if (substr(line, 1, 1) != "$") next
    star = index(line, "*")
    if (star == 0 || length(line) < star + 2) { bad++; next }
    sum = 0
    for (i = 2; i < star; i++) sum = xor8(sum, ORD[substr(line, i, 1)])
    want = hexval(substr(line, star + 1, 1)) * 16 + hexval(substr(line, star + 2, 1))
    if (sum != want) { bad++; next }

    valid++
    n = split(substr(line, 2, star - 2), f, ",")
    typ = substr(f[1], length(f[1]) - 2, 3)
    if (index(types, typ) == 0) types = types (types == "" ? "" : " ") typ

    if (typ == "GGA") {
        gga++
        utc = f[2]
        if (f[7] + 0 > quality) quality = f[7] + 0
        if (f[8] + 0 > sats) sats = f[8] + 0
        lat = f[3]; ns = f[4]; lon = f[5]; ew = f[6]; alt = f[10]
    }
}
END {
    printf "valid=%d\n", valid
    printf "bad=%d\n", bad
    printf "gga=%d\n", gga
    printf "quality=%d\n", quality
    printf "sats=%d\n", sats
    printf "utc=%s\n", utc
    printf "lat=%s\n", lat
    printf "ns=%s\n", ns
    printf "lon=%s\n", lon
    printf "ew=%s\n", ew
    printf "alt=%s\n", alt
    printf "types=%s\n", types
}
AWK

awk -f "$WORKDIR/parse.awk" "$WORKDIR/nmea_trim.txt" > "$STATS" || die "解析 NMEA 失败"

get_val() {
    sed -n "s/^$1=//p" "$STATS"
}

VALID=$(get_val valid)
BAD=$(get_val bad)
GGA=$(get_val gga)
QUALITY=$(get_val quality)
SATS=$(get_val sats)
UTC=$(get_val utc)
LAT=$(get_val lat)
NS=$(get_val ns)
LON=$(get_val lon)
EW=$(get_val ew)
ALT=$(get_val alt)
TYPES=$(get_val types)

quality_name() {
    case "$1" in
        0) echo "未定位" ;;
        1) echo "GPS 单点定位" ;;
        2) echo "DGPS/SBAS" ;;
        4) echo "RTK 固定解" ;;
        5) echo "RTK 浮动解" ;;
        6) echo "推算定位(DR)" ;;
        *) echo "未知($1)" ;;
    esac
}

# NMEA 的 ddmm.mmmm -> 十进制度
dd_to_deg() {
    awk -v v="$1" 'BEGIN { if (v == "") { print ""; exit } printf "%.6f", int(v / 100) + (v - int(v / 100) * 100) / 60 }'
}

LAT_DEC=$(dd_to_deg "$LAT")
LON_DEC=$(dd_to_deg "$LON")

echo "    有效语句: $VALID 条（校验和正确），异常 $BAD 条"
[ -n "$TYPES" ] && echo "    语句类型: $TYPES"
if [ "$GGA" -gt 0 ]; then
    echo "    定位状态: $(quality_name "$QUALITY")（GGA quality=$QUALITY），卫星 $SATS 颗"
    [ -n "$LAT_DEC" ] && echo "    坐标: $LAT_DEC $NS, $LON_DEC $EW（UTC $UTC）"
    [ -n "$ALT" ] && echo "    海拔: $ALT m"
else
    echo "    定位状态: 未收到 GGA 语句"
fi

# PPS 秒脉冲（仅定位后检查；没有 gpiomon 时跳过）
PPS_RESULT="SKIP"
PPS_DETAIL=""
if [ "$GGA" -gt 0 ] && [ "$QUALITY" -ge 1 ]; then
    if command -v gpiomon >/dev/null 2>&1; then
        timeout 3 gpiomon -c "$PPS_CHIP" "$PPS_LINE" > "$PPS" 2> "$PPS_ERR" || true
        PPS_EDGES=$(grep -c 'rising' "$PPS" 2>/dev/null || true)
        [ -n "$PPS_EDGES" ] || PPS_EDGES=0
        if [ "$PPS_EDGES" -ge 2 ]; then
            PPS_RESULT="PASS"
            PPS_DETAIL="$PPS_EDGES 次上升沿/3 秒"
        elif [ -s "$PPS_ERR" ]; then
            PPS_RESULT="SKIP"
            PPS_DETAIL=$(sed -n '1p' "$PPS_ERR")
        else
            PPS_RESULT="FAIL"
            PPS_DETAIL="$PPS_EDGES 次上升沿/3 秒"
        fi
    else
        PPS_DETAIL="未安装 gpiomon（libgpiod v2）"
    fi
else
    PPS_DETAIL="未定位，跳过"
fi

echo
echo "  ----------------------------------------------"
[ "$VALID" -ge 5 ] && V1=PASS || V1=FAIL
printf '  NMEA 数据（≥5 条）    : %s\n' "$V1"
[ "$GGA" -ge 1 ] && V2=PASS || V2=FAIL
printf '  GGA 定位语句          : %s\n' "$V2"
printf '  PPS 秒脉冲            : %s%s\n' "$PPS_RESULT" "${PPS_DETAIL:+（$PPS_DETAIL）}"
echo "  ----------------------------------------------"

echo
if [ "$V1" = PASS ] && [ "$V2" = PASS ]; then
    if [ "$QUALITY" -lt 1 ]; then
        echo "RESULT : PASS - 串口链路正常；当前未定位，请把天线放到室外或窗边，冷启动约 26 秒"
    else
        echo "RESULT : PASS - 模块已定位，串口与 PPS 均正常"
    fi
    exit 0
fi

echo "RESULT : FAIL - 串口链路异常"
echo "         1) 跳线帽是否竖插在丝印 B 档？（A 档时 UART 走板载 USB-UART）"
echo "         2) 模块波特率是否被改成其它值？默认 115200"
echo "         3) 排针是否插紧、HAT 是否上电（PWR 红灯常亮）"
exit 1
