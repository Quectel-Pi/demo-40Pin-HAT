#!/bin/bash
# PWM output + GPIO edge capture test (M2 / RK3576)
#
# Usage:
#   ./M2_pwm-test.sh [capture-gpio]
#       capture-gpio  global GPIO number to capture PWM edges on
#                     (default 92 = Pin 31; 93 = Pin 29)
#
# Wiring: connect Pin 33 (PWM2_CH3, GPIO2_D3) to the capture GPIO.
#         default Pin 31 / GPIO92 / GPIO2_D4 ;  93 = Pin 29 / GPIO2_D5.
# Output: 1000 Hz, 50% duty cycle.
# PASS criteria: measured 900-1100 Hz and 40-60% duty cycle, >=3 complete periods.

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

pwm_test()
{
    pwm_chip=
    capture_gpio=${1:-92}
    case "$capture_gpio" in
    ''|*[!0-9]*) die "invalid capture GPIO: $capture_gpio" ;;
    esac
    capture_chip=gpiochip$((capture_gpio / 32))
    capture_line=$((capture_gpio % 32))
    TMP_BASE=${TMPDIR:-/tmp}
    [ -d "$TMP_BASE" ] || TMP_BASE=$PWD
    tmp_file=$(mktemp "$TMP_BASE/pwm-test.XXXXXX") || die "cannot create temporary file"

    require_command gpiomon
    require_command timeout
    require_command awk
    for chip in /sys/class/pwm/pwmchip*; do
        case "$(readlink -f "$chip/device" 2>/dev/null)" in
        */2ade3000.pwm) pwm_chip=$chip; break ;;
        esac
    done
    [ -n "$pwm_chip" ] || die "PWM2_CH3 controller 2ade3000.pwm was not found"
    pwm=$pwm_chip/pwm0

    if [ ! -d "$pwm" ]; then
        echo 0 > "$pwm_chip/export" 2>/dev/null || die "cannot export pwm0"
    fi

    pwm_cleanup()
    {
        if [ -e "$pwm/enable" ] && [ "$(cat "$pwm/enable" 2>/dev/null)" = 1 ]; then
            echo 0 > "$pwm/enable" 2>/dev/null || true
        fi
        rm -f "$tmp_file"
    }
    pwm_exit()
    {
        exit_status=$?
        trap - EXIT INT TERM
        pwm_cleanup
        exit "$exit_status"
    }
    trap pwm_exit EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM

    if [ "$(cat "$pwm/enable" 2>/dev/null)" = 1 ]; then
        echo 0 > "$pwm/enable" 2>/dev/null || die "cannot disable pwm0"
    fi
    if [ "$(cat "$pwm/period" 2>/dev/null)" != 0 ]; then
        echo 0 > "$pwm/duty_cycle" 2>/dev/null || die "cannot reset PWM duty cycle"
    fi
    echo 1000000 > "$pwm/period" 2>/dev/null || die "cannot set PWM period"
    echo 500000 > "$pwm/duty_cycle" 2>/dev/null || die "cannot set PWM duty cycle"

    if [ "$capture_gpio" -eq 92 ]; then
        echo "Connect Pin 33 (PWM, GPIO2_D3) to Pin 31 (GPIO92, GPIO2_D4)."
    elif [ "$capture_gpio" -eq 93 ]; then
        echo "Connect Pin 33 (PWM, GPIO2_D3) to Pin 29 (GPIO93, GPIO2_D5)."
    else
        echo "Connect Pin 33 (PWM, GPIO2_D3) to GPIO$capture_gpio."
    fi
    echo "Output: 1000 Hz, 50% duty cycle"
    timeout 5 gpiomon -c "$capture_chip" -n 21 \
        "$capture_line" > "$tmp_file" 2>&1 &
    monitor_pid=$!
    sleep 1
    echo 1 > "$pwm/enable" 2>/dev/null || die "cannot enable pwm0"
    wait "$monitor_pid"
    monitor_status=$?
    echo 0 > "$pwm/enable" 2>/dev/null || true

    if [ "$monitor_status" -ne 0 ]; then
        cat "$tmp_file" >&2 2>/dev/null
        die "no valid PWM edges captured on GPIO$capture_gpio"
    fi

    awk -v capture_gpio="$capture_gpio" '
        $2 == "rising" {
            if (have_rise && have_fall) {
                period = $1 - prev_rise
                high = fall_time - rise_time
                if (period > 0 && high >= 0) {
                    period_sum += period
                    high_sum += high
                    samples++
                }
            }
            prev_rise = $1
            rise_time = $1
            have_rise = 1
            have_fall = 0
        }
        $2 == "falling" && have_rise {
            fall_time = $1
            have_fall = 1
        }
        END {
            if (samples < 3) {
                print "FAIL: insufficient complete PWM periods" > "/dev/stderr"
                exit 1
            }
            frequency = samples / period_sum
            duty = 100 * high_sum / period_sum
            printf "Measured: %.2f Hz, %.2f%% duty cycle (%d periods)\n", frequency, duty, samples
            if (frequency < 900 || frequency > 1100 || duty < 40 || duty > 60) {
                print "FAIL: PWM measurement is outside the expected range" > "/dev/stderr"
                exit 1
            }
            printf "PASS: PWM output and GPIO%d capture are working\n", capture_gpio
        }
    ' "$tmp_file" || exit 1

    pwm_cleanup
    trap - EXIT INT TERM
}

require_root
pwm_test "${1:-92}"
