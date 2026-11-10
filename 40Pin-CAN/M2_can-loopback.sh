#!/bin/sh
#
# CAN 单板回环测试（M2 / RK3576）
#
# 用法：
#   sudo ./M2_can-loopback.sh           # 控制器回环
#   sudo ./M2_can-loopback.sh silent    # 静默回环（不依赖对端 ACK）
#
# Android：
#   su
#   sh ./M2_can-loopback.sh [silent]
#
# 说明：
#   - 只需要 1 块 M2 + 1 个 SN65HVD230，不需要第二块板。
#   - 优先使用 candump/cansend；没有时回退到 /data/local/tmp/cantool。
#   - 测试完成后自动恢复普通双板模式（loopback off）。
#
# 接线：Pin16 -> P1-1(CAN_TX)，Pin18 -> P1-2(CAN_RX)，
#       Pin17 -> P1-3(3.3V)，Pin9 -> P1-4(GND)。
#       接线或换线前先断开开发板电源。

set -u

IFACE=can0
BITRATE=500000
DBITRATE=1000000
FRAMES=5
MODE="${1:-normal}"

case "$MODE" in
	normal) MODE_NAME="控制器回环" ;;
	silent) MODE_NAME="静默回环" ;;
	*)
		echo "用法: $0 [silent]"
		exit 1
		;;
esac

if [ "$(id -u)" -ne 0 ]; then
	echo "请以 root 运行（Linux: sudo $0；Android: 先 su）"
	exit 1
fi

if ! command -v ip >/dev/null 2>&1; then
	echo "未找到 ip 命令，请先安装 iproute2"
	exit 1
fi

if ! ip link show "$IFACE" >/dev/null 2>&1; then
	echo "未找到 $IFACE，请先执行 qpi-config can0 enable 并重启"
	exit 1
fi

# 选择收发工具
if command -v candump >/dev/null 2>&1 && command -v cansend >/dev/null 2>&1; then
	TOOL=canutils
elif [ -x /data/local/tmp/cantool ]; then
	TOOL=cantool
else
	echo "未找到收发工具："
	echo "  Linux：安装 can-utils（如 apt install can-utils）"
	echo "  Android：把静态 candump/cansend 或 cantool 推到 /data/local/tmp 后重试"
	exit 1
fi

# 临时文件：优先 $TMPDIR，其次 Android 常用的 /data/local/tmp
TMP=""
for d in "${TMPDIR:-}" /data/local/tmp /tmp .; do
	if [ -n "$d" ] && [ -d "$d" ] && [ -w "$d" ]; then
		TMP="$d"
		break
	fi
done
if [ -z "$TMP" ]; then
	echo "找不到可写的临时目录"
	exit 1
fi
LOG="$TMP/can-loopback-$$.log"

restore_normal() {
	ip link set "$IFACE" down 2>/dev/null
	ip link set "$IFACE" type can bitrate "$BITRATE" dbitrate "$DBITRATE" fd on \
		loopback off listen-only off restart-ms 100 2>/dev/null
	ip link set "$IFACE" txqueuelen 1000 2>/dev/null
	ip link set "$IFACE" up 2>/dev/null
}

cleanup() {
	[ -n "${RECV_PID:-}" ] && kill "$RECV_PID" 2>/dev/null
	restore_normal
	rm -f "$LOG"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

echo "==> 配置 $IFACE：$MODE_NAME"
ip link set "$IFACE" down

if [ "$MODE" = "silent" ]; then
	ip link set "$IFACE" type can bitrate "$BITRATE" dbitrate "$DBITRATE" fd on \
		loopback on listen-only on restart-ms 100 || {
		echo "配置失败：请确认驱动支持 loopback/listen-only"
		exit 1
	}
else
	ip link set "$IFACE" type can bitrate "$BITRATE" dbitrate "$DBITRATE" fd on \
		loopback on listen-only off restart-ms 100 || {
		echo "配置失败：请确认驱动支持 loopback"
		exit 1
	}
fi

ip link set "$IFACE" txqueuelen 1000
ip link set "$IFACE" up || {
	echo "启动 $IFACE 失败"
	exit 1
}

echo "==> 当前状态"
ip -details link show "$IFACE" | sed -n '1,6p'

RX_STATS="/sys/class/net/$IFACE/statistics/rx_packets"
RX_BEFORE=$(cat "$RX_STATS" 2>/dev/null || echo 0)

echo "==> 启动接收（$TOOL）"
if [ "$TOOL" = "canutils" ]; then
	candump -t d -n $((FRAMES * 2)) "$IFACE" >"$LOG" 2>&1 &
	RECV_PID=$!
else
	/data/local/tmp/cantool recv "$IFACE" $((FRAMES * 2)) 2000 >"$LOG" 2>&1 &
	RECV_PID=$!
fi
sleep 1

echo "==> 发送 $FRAMES 帧（ID 0x333）"
if [ "$TOOL" = "canutils" ]; then
	i=0
	while [ "$i" -lt "$FRAMES" ]; do
		cansend "$IFACE" "333#00$(printf '%02X' "$i")A2A3A4A5A6A7" || {
			echo "发送失败"
			exit 1
		}
		i=$((i + 1))
		sleep 0.05
	done
else
	/data/local/tmp/cantool send "$IFACE" "$FRAMES" 0x333 50 || {
		echo "发送失败"
		exit 1
	}
fi

sleep 2
kill "$RECV_PID" 2>/dev/null
wait "$RECV_PID" 2>/dev/null
RECV_PID=

RX_AFTER=$(cat "$RX_STATS" 2>/dev/null || echo 0)
if [ -r "$RX_STATS" ]; then
	COUNT=$((RX_AFTER - RX_BEFORE))
else
	COUNT=$(grep -cE '333#|id=0x333' "$LOG" 2>/dev/null || true)
fi
COUNT=${COUNT:-0}

echo "==> 接收结果（前 5 行）"
sed -n '1,5p' "$LOG"
echo "==> 共收到 $COUNT 帧，期望至少 $FRAMES 帧（回环可能每帧重复一次）"

if [ "$COUNT" -ge "$FRAMES" ]; then
	echo "RESULT: PASS"
	exit 0
fi

echo "RESULT: FAIL"
exit 1
