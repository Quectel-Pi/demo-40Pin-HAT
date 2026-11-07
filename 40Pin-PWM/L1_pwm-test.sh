#!/bin/bash
# PWM output + GPIO edge capture test (L1 / QCM2290)
#
# Usage:
#   ./L1_pwm-test.sh [capture-line]
#       capture-line  line offset on gpiochip0 to capture PWM edges on
#                     (default 4 = Pin 31 / GPIO_4; 5 = Pin 29 / GPIO_5)
#
# Wiring: connect Pin 33 (40Pin PWM, PMU_GPIO02) to the capture pin.
#         default Pin 31 / GPIO_4 / gpiochip0 line 4 ;  5 = Pin 29 / GPIO_5.
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
    capture_chip=gpiochip0
    capture_line=${1:-4}
    case "$capture_line" in
    ''|*[!0-9]*) die "invalid capture line: $capture_line" ;;
    esac
    TMP_BASE=${TMPDIR:-/tmp}
    [ -d "$TMP_BASE" ] || TMP_BASE=$PWD
    tmp_file=$(mktemp "$TMP_BASE/pwm-test.XXXXXX") || die "cannot create temporary file"
    tmp_raw=$(mktemp "$TMP_BASE/pwm-test-raw.XXXXXX") || die "cannot create temporary file"

    require_command gpiomon
    require_command timeout
    require_command awk
    require_command sed

    # 40Pin PWM on L1 is PM2250 PWM (qcom,pwms@be00). pwmchip numbering is
    # not stable, so locate the controller by DT node and fall back to chip2.
    for chip in /sys/class/pwm/pwmchip*; do
        case "$(readlink -f "$chip/device/of_node" 2>/dev/null)" in
        *qcom,pwms@be00) pwm_chip=$chip; break ;;
        esac
    done
    if [ -z "$pwm_chip" ] && [ -d /sys/class/pwm/pwmchip2 ]; then
        pwm_chip=/sys/class/pwm/pwmchip2
    fi
    [ -n "$pwm_chip" ] || die "PM2250 PWM (qcom,pwms@be00) was not found"
    pwm=$pwm_chip/pwm0

    if [ ! -d "$pwm" ]; then
        echo 0 > "$pwm_chip/export" 2>/dev/null || \
            die "cannot export pwm0 (channel may be held by another consumer)"
    fi

    pwm_cleanup()
    {
        if [ -e "$pwm/enable" ] && [ "$(cat "$pwm/enable" 2>/dev/null)" = 1 ]; then
            echo 0 > "$pwm/enable" 2>/dev/null || true
        fi
        rm -f "$tmp_file" "$tmp_raw"
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

    if [ "$capture_line" -eq 4 ]; then
        echo "Connect Pin 33 (PWM) to Pin 31 (GPIO_4, gpiochip0 line 4)."
    elif [ "$capture_line" -eq 5 ]; then
        echo "Connect Pin 33 (PWM) to Pin 29 (GPIO_5, gpiochip0 line 5)."
    else
        echo "Connect Pin 33 (PWM) to gpiochip0 line $capture_line."
    fi
    echo "Output: 1000 Hz, 50% duty cycle"

    # libgpiod v1 and v2 use different chip argument and output formats.
    gpiomon_major=$(gpiomon --version 2>/dev/null | head -1 | \
        awk '{print $NF}' | tr -d 'v' | cut -d. -f1)
    case "$gpiomon_major" in
    ''|*[!0-9]*) die "cannot determine gpiomon version" ;;
    esac

    if [ "$gpiomon_major" -ge 2 ]; then
        timeout 5 gpiomon -c "$capture_chip" -n 21 \
            "$capture_line" > "$tmp_file" 2>&1 &
    else
        timeout 5 gpiomon -n 21 -F '%s.%n %e' \
            "$capture_chip" "$capture_line" > "$tmp_raw" 2>&1 &
    fi
    monitor_pid=$!
    sleep 1
    echo 1 > "$pwm/enable" 2>/dev/null || die "cannot enable pwm0"
    wait "$monitor_pid"
    monitor_status=$?
    echo 0 > "$pwm/enable" 2>/dev/null || true

    if [ "$monitor_status" -ne 0 ]; then
        cat "$tmp_raw" "$tmp_file" >&2 2>/dev/null
        die "no valid PWM edges captured on gpiochip0 line $capture_line"
    fi

    if [ "$gpiomon_major" -lt 2 ]; then
        sed -e 's/ 1$/ rising/' -e 's/ 0$/ falling/' "$tmp_raw" > "$tmp_file"
    fi

    awk -v capture_line="$capture_line" '
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
            printf "PASS: PWM output and gpiochip0 line %d capture are working\n", capture_line
        }
    ' "$tmp_file" || exit 1

    pwm_cleanup
    trap - EXIT INT TERM
}

require_root
pwm_test "${1:-4}"
