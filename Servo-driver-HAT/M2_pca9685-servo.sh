#!/bin/bash
# PCA9685 16-channel PWM servo driver control (auto-detect I2C bus)
#
# Usage:
#   ./M2_pca9685-servo.sh [-b i2c-bus] [-a address] [-c "ch ..."]
#                         [-p "min_us max_us"] [-s step_us] [-t]
#
# Defaults: bus auto (scan /dev/i2c-* for 0x40 + ALLCALL 0x70), address 0x40,
#           channels "4 8 12", pulse 500-2500 us, step 50 us.
#           Board silkscreen labels 5/9/13 are register channels 4/8/12
#           (zero based); use -c to override.
#
# Mode:
#   no -t   interactive menu: all channels -> angle, one channel -> angle,
#           synchronized sweep, outputs off, exit (keeps the last position)
#   -t      one synchronized sweep, then exit (outputs hold at center)
#   Angle mapping: 0-180 deg -> PULSE_MIN..PULSE_MAX (default 500-2500 us)
#
# Wiring:
#   Pin 3/5 (I2C-3) or Pin 27/28 (I2C-7): SDA -> SDA, SCL -> SCL, GND common.
#   Logic VCC 3.3 V (keep it at 3.3 V when the module ties its I2C pull-ups
#   to VCC), servo V+ 5-6 V. Never power V+ from 3V3.
#   OE must be low (enabled); most modules tie OE to GND.

die()
{
    echo "ERROR: $*" >&2
    exit 1
}

require_root()
{
    [ "$(id -u)" -eq 0 ] || die "must be run as root"
}

require_command()
{
    command -v "$1" >/dev/null 2>&1 || die "$1 is not installed"
}

usage()
{
    echo "Usage: $0 [-b i2c-bus] [-a address] [-c \"ch ...\"] [-p \"min_us max_us\"] [-s step_us] [-t]"
    echo "  defaults: bus auto (scan all /dev/i2c-*), address 0x40, channels \"4 8 12\", pulse 500-2500 us, step 50 us"
    echo "  -t  run one synchronized sweep and exit (default is the interactive menu)"
}

BUS=3
BUS_AUTO=1
ADDR=0x40
CHANNELS="4 8 12"
PULSE_MIN=500
PULSE_MAX=2500
STEP=50
SETTLE=0.02
RUN_SWEEP=0

# PCA9685 registers
MODE1=0x00
MODE2=0x01
LED0_ON_L=0x06
PRESCALE=0xFE

i2c_get()
{
    local out

    out=$(i2cget -y "$BUS" "$ADDR" "$1" 2>/dev/null) || return 1
    case "$out" in
    0x*) printf '%d\n' "$out" ;;
    *) return 1 ;;
    esac
}

i2c_set()
{
    i2cset -y "$BUS" "$ADDR" "$1" "$2" >/dev/null 2>&1
}

# Write LEDn_ON_L/H + LEDn_OFF_L/H in one I2C block transfer so the four
# registers take effect together at STOP (auto-increment must be enabled).
set_channel_raw()
{
    local base

    base=$((LED0_ON_L + 4 * $1))
    i2cset -y "$BUS" "$ADDR" "$base" "$2" "$3" "$4" "$5" i >/dev/null 2>&1
}

# Set one channel: pulse width in microseconds, 0 = output off (full OFF).
set_pulse()
{
    local ch=$1 pulse=$2 ticks

    if [ "$pulse" -le 0 ]; then
        set_channel_raw "$ch" 0 0 0 0x10 || return 1
        return 0
    fi
    # 50 Hz: 4096 ticks per 20 ms, 1 tick = 4.88 us
    ticks=$(((pulse * 4096 + 10000) / 20000))
    [ "$ticks" -lt 1 ] && ticks=1
    [ "$ticks" -gt 4095 ] && ticks=4095
    set_channel_raw "$ch" 0 0 $((ticks & 0xFF)) $(((ticks >> 8) & 0x0F)) || return 1
}

# Read back the OFF count of one channel; echoes -1 when full OFF is set.
read_ticks()
{
    local ch=$1 base off_l off_h

    base=$((LED0_ON_L + 4 * ch))
    off_l=$(i2c_get $((base + 2))) || return 1
    off_h=$(i2c_get $((base + 3))) || return 1
    if [ $((off_h & 0x10)) -ne 0 ]; then
        echo -1
        return 0
    fi
    echo $((((off_h & 0x0F) << 8) | off_l))
}

pulse_ticks()
{
    local ticks

    ticks=$((($1 * 4096 + 10000) / 20000))
    [ "$ticks" -lt 1 ] && ticks=1
    [ "$ticks" -gt 4095 ] && ticks=4095
    echo "$ticks"
}

# Without -b, scan every /dev/i2c-* bus. A PCA9685 is identified by both
# its own address (0x40) and the default ALLCALL address (0x70), so a random
# device that happens to sit at 0x40 is not selected by accident.
select_bus()
{
    local bus pca_list="" only40_list="" scanned="" first="" count=0

    [ "$BUS_AUTO" -eq 1 ] || return 0

    for bus in $(ls /dev/i2c-* 2>/dev/null | sed 's#.*/i2c-##' | sort -n); do
        case "$bus" in
        '' | *[!0-9]*) continue ;;
        esac
        scanned="$scanned $bus"
        if i2cget -y "$bus" "$ADDR" 0x00 >/dev/null 2>&1; then
            if i2cget -y "$bus" 0x70 0x00 >/dev/null 2>&1; then
                pca_list="$pca_list $bus"
            else
                only40_list="$only40_list $bus"
            fi
        fi
    done

    if [ -z "$pca_list" ]; then
        if [ -n "$only40_list" ]; then
            die "found $ADDR on bus(es) [$only40_list] but no ALLCALL 0x70; use -b <bus> if that is the PCA9685"
        fi
        die "PCA9685 not found at $ADDR on i2c buses:$scanned (check wiring; use -b to select a bus)"
    fi

    for bus in $pca_list; do
        [ -z "$first" ] && first=$bus
        count=$((count + 1))
    done
    if [ "$count" -gt 1 ]; then
        echo "WARNING: PCA9685 found on multiple buses:$pca_list; using i2c-$first (override with -b)" >&2
    fi
    BUS=$first
}

init_chip()
{
    local mode ch

    mode=$(i2c_get "$MODE1") ||
        die "PCA9685 did not answer at $ADDR on i2c-$BUS (check wiring, then run: i2cdetect -y -r $BUS)"
    echo "PCA9685 on i2c-$BUS at $ADDR (MODE1=0x$(printf '%02X' "$mode"))"
    if [ $((mode & 0x40)) -ne 0 ]; then
        die "MODE1.EXTCLK is set and cannot be cleared in software; power-cycle the servo board and retry"
    fi

    # PRE_SCALE is writable only while SLEEP=1
    i2c_set "$MODE1" 0x10 || die "cannot set MODE1.SLEEP"
    i2c_set "$MODE2" 0x04 || die "cannot set MODE2 (totem pole)"
    # 50 Hz: round(25 MHz / (4096 * 50)) - 1 = 121 = 0x79
    i2c_set "$PRESCALE" 121 || die "cannot set PRE_SCALE"
    i2c_set "$MODE1" 0x21 || die "cannot clear MODE1.SLEEP"
    sleep 0.01

    # Safe state before clearing RESTART: every channel off, so a restart
    # cannot resume old pulse widths left by a previous program.
    ch=0
    while [ "$ch" -lt 16 ]; do
        set_channel_raw "$ch" 0 0 0 0x10 || die "cannot reset channel $ch"
        ch=$((ch + 1))
    done
    i2c_set "$MODE1" 0xA1 || die "cannot clear MODE1.RESTART"
}

check_readback()
{
    local ch=$1 expected=$2 got

    got=$(read_ticks "$ch") || die "I2C read failed on channel $ch"
    if [ "$got" -ne "$expected" ]; then
        echo "FAIL: channel $ch read-back $got, expected $expected" >&2
        return 1
    fi
    return 0
}

sweep_channels()
{
    local ch pulse ok=1 ticks_min ticks_max

    echo "Channels $CHANNELS: center, min -> max -> min (synchronized)"
    for ch in $CHANNELS; do
        set_pulse "$ch" 1500 || die "I2C write failed on channel $ch"
    done
    sleep 0.4

    pulse=$PULSE_MIN
    while [ "$pulse" -le "$PULSE_MAX" ]; do
        for ch in $CHANNELS; do
            set_pulse "$ch" "$pulse" || die "I2C write failed on channel $ch"
        done
        sleep "$SETTLE"
        pulse=$((pulse + STEP))
    done
    ticks_max=$(pulse_ticks "$PULSE_MAX")
    ticks_min=$(pulse_ticks "$PULSE_MIN")
    for ch in $CHANNELS; do
        set_pulse "$ch" "$PULSE_MAX" || die "I2C write failed on channel $ch"
        check_readback "$ch" "$ticks_max" || ok=0
    done
    sleep 0.3

    pulse=$((PULSE_MAX - STEP))
    while [ "$pulse" -ge "$PULSE_MIN" ]; do
        for ch in $CHANNELS; do
            set_pulse "$ch" "$pulse" || die "I2C write failed on channel $ch"
        done
        sleep "$SETTLE"
        pulse=$((pulse - STEP))
    done
    for ch in $CHANNELS; do
        set_pulse "$ch" "$PULSE_MIN" || die "I2C write failed on channel $ch"
        check_readback "$ch" "$ticks_min" || ok=0
    done
    sleep 0.3

    for ch in $CHANNELS; do
        set_pulse "$ch" 1500 || die "I2C write failed on channel $ch"
    done
    sleep 0.3

    [ "$ok" = 1 ] || return 1
    for ch in $CHANNELS; do
        echo "PASS: channel $ch sweep completed (register read-back OK)"
    done
    return 0
}

angle_to_pulse()
{
    echo $((PULSE_MIN + ($1 * (PULSE_MAX - PULSE_MIN) + 90) / 180))
}

set_angle_all()
{
    local angle=$1 pulse ch

    pulse=$(angle_to_pulse "$angle")
    for ch in $CHANNELS; do
        set_pulse "$ch" "$pulse" || die "I2C write failed on channel $ch"
    done
    echo "channels $CHANNELS -> $angle deg ($pulse us)"
}

set_angle_one()
{
    local ch=$1 angle=$2 pulse

    pulse=$(angle_to_pulse "$angle")
    set_pulse "$ch" "$pulse" || die "I2C write failed on channel $ch"
    echo "channel $ch -> $angle deg ($pulse us)"
}

all_off()
{
    local ch

    ch=0
    while [ "$ch" -lt 16 ]; do
        set_channel_raw "$ch" 0 0 0 0x10 || die "I2C write failed on channel $ch"
        ch=$((ch + 1))
    done
    echo "all outputs off"
}

read_angle()
{
    local input

    while :; do
        printf "Angle 0-180 (q to cancel): " >&2
        read -r input || return 1
        case "$input" in
        q | Q) return 1 ;;
        '' | *[!0-9]*) echo "invalid angle" >&2; continue ;;
        esac
        if [ "$input" -le 180 ]; then
            ANGLE=$((10#$input))
            return 0
        fi
        echo "invalid angle" >&2
    done
}

read_channel()
{
    local input

    while :; do
        printf "Channel 0-15 (board label = channel + 1, q to cancel): " >&2
        read -r input || return 1
        case "$input" in
        q | Q) return 1 ;;
        '' | *[!0-9]*) echo "invalid channel" >&2; continue ;;
        esac
        if [ "$input" -le 15 ]; then
            CHANNEL=$((10#$input))
            return 0
        fi
        echo "invalid channel" >&2
    done
}

menu()
{
    local choice

    while :; do
        echo
        echo "===== PCA9685 servo control (i2c-$BUS, addr $ADDR) ====="
        echo "  channels: $CHANNELS (board label = channel + 1)"
        echo "  1) all selected channels -> angle"
        echo "  2) one channel -> angle"
        echo "  3) synchronized sweep"
        echo "  4) turn all outputs off"
        echo "  0) exit (keep last position)"
        printf "Select: "
        read -r choice || exit 0
        case "$choice" in
        1) read_angle && set_angle_all "$ANGLE" ;;
        2) read_channel && read_angle && set_angle_one "$CHANNEL" "$ANGLE" ;;
        3) sweep_channels || echo "FAIL: read-back mismatch" >&2 ;;
        4) all_off ;;
        0) echo "exit; outputs keep the last position"; exit 0 ;;
        *) echo "invalid choice" >&2 ;;
        esac
    done
}

require_root
require_command i2cset
require_command i2cget

while getopts "b:a:c:p:s:th" opt; do
    case "$opt" in
    b) BUS=$OPTARG; BUS_AUTO=0 ;;
    a) ADDR=$OPTARG ;;
    c) CHANNELS=$OPTARG ;;
    p)
        PULSE_MIN=${OPTARG%% *}
        PULSE_MAX=${OPTARG#* }
        ;;
    s) STEP=$OPTARG ;;
    t) RUN_SWEEP=1 ;;
    h) usage; exit 0 ;;
    *) usage; exit 1 ;;
    esac
done

case "$BUS" in
'' | *[!0-9]*) die "invalid i2c bus: $BUS" ;;
esac

case "$ADDR" in
0x* | 0X*)
    addr_hex=${ADDR#0[xX]}
    case "$addr_hex" in
    '' | *[!0-9a-fA-F]*) die "invalid i2c address: $ADDR" ;;
    esac
    addr_val=$((16#$addr_hex))
    ;;
*)
    case "$ADDR" in
    '' | *[!0-9]*) die "invalid i2c address: $ADDR" ;;
    esac
    addr_val=$ADDR
    ;;
esac
[ "$addr_val" -ge 64 ] && [ "$addr_val" -le 127 ] || die "i2c address out of range (0x40-0x7f): $ADDR"
ADDR=$addr_val

[ -n "$CHANNELS" ] || die "no channel given"
for ch in $CHANNELS; do
    case "$ch" in
    '' | *[!0-9]*) die "invalid channel: $ch" ;;
    esac
    [ "$ch" -le 15 ] || die "channel out of range (0-15): $ch"
done

case "$PULSE_MIN" in
'' | *[!0-9]*) die "invalid pulse width: $PULSE_MIN" ;;
esac
case "$PULSE_MAX" in
'' | *[!0-9]*) die "invalid pulse width: $PULSE_MAX" ;;
esac
[ "$PULSE_MIN" -ge 1 ] && [ "$PULSE_MAX" -le 20000 ] && [ "$PULSE_MIN" -lt "$PULSE_MAX" ] ||
    die "invalid pulse range: $PULSE_MIN-$PULSE_MAX us"
case "$STEP" in
'' | *[!0-9]*) die "invalid step: $STEP" ;;
esac
[ "$STEP" -ge 1 ] || die "invalid step: $STEP"

select_bus
init_chip

if [ "$RUN_SWEEP" -eq 1 ]; then
    sweep_channels || die "servo sweep failed (register read-back mismatch)"
    echo "PASS: sweep completed; outputs hold at center"
elif [ -t 0 ]; then
    menu
else
    die "stdin is not a terminal; use -t for a one-shot sweep"
fi
