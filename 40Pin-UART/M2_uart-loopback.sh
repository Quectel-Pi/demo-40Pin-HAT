#!/bin/sh
#
# 40pin-uart-loop -- 40-pin header UART loopback test (/dev/ttyS1)
#
# Wiring : pin8 (TXD) shorted to pin10 (RXD)
#
# Exit codes: 0 all cases PASS, 1 one or more FAIL, 2 setup/usage error.

PROG=${0##*/}

DEV=/dev/ttyS1
TIMEOUT=1.5
BAUDS="9600 38400 115200 230400 460800 921600 1500000"
TXD_PIN=8
RXD_PIN=10

CASE_DETAIL=

usage() {
    cat <<EOF
USAGE:
    $PROG

Run the 40-pin UART loopback test on $DEV.

Wiring:
    pin$TXD_PIN (TXD) must be shorted to pin$RXD_PIN (RXD).

Options:
    -h       show this help.

Baud rates: $BAUDS
EOF
}

die() {
    printf '%s\n' "$*" >&2
    exit 2
}

WORKDIR=$(mktemp -d /tmp/40pin-uart-loop.XXXXXX) || die "cannot create temporary directory"
cleanup() {
    rm -rf "$WORKDIR"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

[ -e "$DEV" ] || die "$DEV does not exist.
       40Pin UART (Pin$TXD_PIN/Pin$RXD_PIN) must be enabled"

make_payload() {
    # 0xA5 0x5A 0x00 0xFF 0x55 0xAA 0x01 0xFE "LOOP" x2 + first 8 bytes = 32
    printf '\245\132\000\377\125\252\001\376LOOP\245\132\000\377\125\252\001\376LOOP\245\132\000\377\125\252\001\376' > "$1"
}

hex_string() {
    od -An -v -tx1 "$1" 2>/dev/null | awk '{ for (i = 1; i <= NF; i++) printf "%s", $i }'
}

uart_config() {
    cfg_baud=$1
    stty -F "$DEV" "$cfg_baud" raw cs8 -cstopb -parenb cread clocal -crtscts 2>/dev/null
}

uart_roundtrip() {
    rt_baud=$1
    rt_want=$WORKDIR/want.bin
    rt_got=$WORKDIR/got.bin

    if ! uart_config "$rt_baud"; then
        CASE_DETAIL="cannot configure port at $rt_baud baud"
        return 1
    fi

    # Drop anything left over from the previous baud rate.
    timeout 0.2 dd if="$DEV" of=/dev/null bs=64 count=1 2>/dev/null || true

    : > "$rt_got"
    # 先启动读端并保持端口打开：如果写完后最后一个 fd 关闭，n_tty 会释放
    # 读缓冲，回环收到的数据会被丢掉，后面的读就永远是 no echo。
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
        CASE_DETAIL="no echo"
        return 1
    fi
    if [ "$rt_size" -ne 32 ]; then
        CASE_DETAIL="got $rt_size bytes, expected 32"
        return 1
    fi

    rt_cmp=$(cmp -l "$rt_want" "$rt_got" 2>/dev/null | head -n 1)
    if [ -n "$rt_cmp" ]; then
        read -r rt_byte rt_sent rt_recv <<EOF
$rt_cmp
EOF
        CASE_DETAIL=$(printf 'byte %d: sent 0x%02X got 0x%02X' \
            "$((rt_byte - 1))" "$((0$rt_sent))" "$((0$rt_recv))")
    else
        CASE_DETAIL="data mismatch"
    fi
    return 1
}

make_payload "$WORKDIR/want.bin" || die "cannot create the test payload"
size=$(wc -c < "$WORKDIR/want.bin" | tr -d ' ')
[ "$size" -eq 32 ] || die "internal error: payload is $size bytes, expected 32"

printf 'device : %s\n' "$DEV"
printf 'expect : pin%d (TXD) shorted to pin%d (RXD)\n' "$TXD_PIN" "$RXD_PIN"
printf 'data   : %s\n' "$(hex_string "$WORKDIR/want.bin")"
printf '\n'

: > "$WORKDIR/results"
for baud in $BAUDS; do
    if uart_roundtrip "$baud"; then
        printf '%s|1|\n' "$baud" >> "$WORKDIR/results"
    else
        printf '  %d baud FAIL: %s\n' "$baud" "$CASE_DETAIL"
        printf '%s|0|%s\n' "$baud" "$CASE_DETAIL" >> "$WORKDIR/results"
    fi
done

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
    printf '         Is pin%d shorted to pin%d? This test needs a jumper.\n' "$TXD_PIN" "$RXD_PIN"
    exit 1
fi
printf 'RESULT : PASS - all %d cases matched\n' "$total"
exit 0
