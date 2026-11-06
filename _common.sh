#!/bin/bash
#
# a.Waveshare 公共函数库 —— 由各 HAT 目录下 <板>_<外设>.sh 一键脚本 source，不单独执行。
#
# 设计约定：
#   * 板型差异（DC/RST 引脚号、gpiochip label、片选等）只存在于各一键脚本里，
#     通过环境变量下发给 SDK 例程；SDK 源码（c/、python/）保持统一，不含板型预设；
#   * 离线依赖包统一放在 a.Waveshare/packages/（HAT 共用），由根目录 install_deps.sh
#     单独安装；各板脚本只检查依赖是否已装好，不执行安装；
#   * 用法: sudo ./<板>_<外设>.sh [--no-build]
#
set -u

WS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_DIR="$WS_ROOT/packages"

DO_BUILD=1
CAN_BUILD=1
PY_OK=0

ws_usage() {
    echo "用法: sudo $0 [--no-build]"
    echo "  --no-build  跳过 C 例程编译"
    echo "  提示: 离线依赖需先在 a.Waveshare 根目录执行一次 sudo ./install_deps.sh"
}

ws_parse_args() {
    for arg in "$@"; do
        case "$arg" in
            --no-build) DO_BUILD=0 ;;
            -h|--help)  ws_usage; exit 0 ;;
            *) echo "未知参数: $arg"; ws_usage; exit 1 ;;
        esac
    done
}

ws_require_root() {
    if [ "$(id -u)" != "0" ]; then
        echo "请用 root 运行: sudo $0"
        exit 1
    fi
}

ws_header() {   # $1=板名  $2=外设名
    echo "=============================================================="
    echo " $2 一键脚本（$1）"
    echo " 板型参数由本脚本下发，SDK 源码保持统一"
    echo "=============================================================="
}

# ------------------------------------------------------------------ 板型提示
# 按 device-tree 里的关键字提示当前板型；不匹配只警告不退出（板子存在变体）
ws_check_board() {   # $1=关键字（可用 | 分隔多个）  $2=板型说明
    local name=""

    [ -r /proc/device-tree/model ]      && name="$(tr '\0' ' ' < /proc/device-tree/model 2>/dev/null)"
    [ -r /proc/device-tree/compatible ] && name="$name $(tr '\0' ' ' < /proc/device-tree/compatible 2>/dev/null)"
    name="$(echo "$name" | tr 'A-Z' 'a-z')"

    if [ -z "${name// /}" ]; then
        echo "    跳过板型提示（读不到 /proc/device-tree）"
    elif echo "$name" | grep -Eq "$1"; then
        echo "    当前板型: $2"
    else
        echo "    警告: 当前板型与「$2」不匹配（device-tree 里没有 \"$1\"）"
        echo "          仍按 $2 的参数继续；若插错板请改用对应的脚本。"
    fi
}

# ------------------------------------------------------------------ 片选选择
# 交互选择 CE0/CE1：仅终端环境、且 CS0/CS1 两个节点都存在时询问；结果写入 OLED_SPI_CS
ws_choose_spi_cs() {   # 无参数；默认 CE0（Pin24）
    [ -t 0 ] || return 0
    ls /dev/spidev*.0 >/dev/null 2>&1 || return 0
    ls /dev/spidev*.1 >/dev/null 2>&1 || return 0

    echo "    片选: CE0 = Pin24（/dev/spidevX.0），CE1 = Pin26（/dev/spidevX.1）"
    printf "    选择片选 [0/1]（回车默认 CE0）: "
    read -r HAT_CS
    case "${HAT_CS:-0}" in
        0) ;;
        1) export OLED_SPI_CS=1
           echo "    注意: 请确认 HAT 的 CS 已接到 Pin26" ;;
        *) echo "    输入 \"$HAT_CS\" 无效，按 CE0 继续" ;;
    esac
}

# ------------------------------------------------------------------ 1/4 依赖
# 单个 deb 是否已按不低于包内的版本安装好（0=已装好，无需再装）
ws_deb_installed() {
    local deb="$1" pkg ver cur

    command -v dpkg-deb >/dev/null 2>&1 || return 1
    pkg="$(dpkg-deb -f "$deb" Package 2>/dev/null)"
    ver="$(dpkg-deb -f "$deb" Version 2>/dev/null)"
    [ -n "$pkg" ] && [ -n "$ver" ] || return 1

    cur="$(dpkg-query -W -f='${Status} ${Version}' "$pkg" 2>/dev/null)"
    case "$cur" in
        "install ok installed "*) cur="${cur#install ok installed }" ;;
        *) return 1 ;;
    esac
    # 已装版本不低于包内版本才算“已装好”，避免把板上更新的版本降级回旧包
    dpkg --compare-versions "$cur" ge "$ver"
}

# 只检查 packages/ 里的 deb 是否已装好，不执行安装；安装统一由根目录 install_deps.sh 负责
ws_check_debs() {
    if ! command -v dpkg >/dev/null 2>&1; then
        echo "    本系统没有 dpkg，跳过检查（依赖装不上；C 例程走 prebuilt/ 兜底）"
        return
    fi

    local deb total=0 missing=0
    for deb in "$PKG_DIR"/*.deb; do
        [ -e "$deb" ] || continue
        total=$((total + 1))
        ws_deb_installed "$deb" || missing=$((missing + 1))
    done

    if [ "$total" = "0" ]; then
        echo "    未找到 $PKG_DIR/*.deb（packages/ 需与脚本、SDK 一起拷贝；依赖已装好可忽略）"
        return
    fi
    if [ "$missing" = "0" ]; then
        echo "    共 $total 个 deb 包（HAT 共用），已全部安装"
        return
    fi
    echo "    共 $total 个 deb 包（HAT 共用），缺少 $missing 个"
    echo "    请先在根目录安装依赖（装一次即可）: sudo $WS_ROOT/install_deps.sh"
}

# ------------------------------------------------------------------ 2/4 工具链
ws_check_toolchain() {   # 结束后 CAN_BUILD=0/1
    local missing=""

    command -v gcc  >/dev/null 2>&1 || missing="$missing gcc"
    command -v make >/dev/null 2>&1 || missing="$missing make"

    if [ -z "$missing" ]; then
        echo "    gcc:  $(gcc --version | head -n 1)"
        echo "    make: $(make --version | head -n 1)"
        return
    fi

    if command -v dpkg >/dev/null 2>&1; then
        echo "    缺少:$missing —— 请先执行一次 sudo $WS_ROOT/install_deps.sh 安装依赖"
    else
        echo "    缺少:$missing（本系统没有 dpkg，装不上）"
    fi
    CAN_BUILD=0
    echo "    → 跳过编译，改用 prebuilt/ 里的预编译二进制"
}

ws_check_python3() {     # 结束后 PY_OK=0/1（只表示 python3 是否存在）
    PY_OK=0
    if command -v python3 >/dev/null 2>&1; then
        PY_OK=1
    else
        echo "    python3 不可用 → Python 例程无法运行（C 例程不受影响）"
    fi
}

ws_ensure_smbus2() {     # 环境传感 HAT 专用：只检查 smbus2 是否可用（不可用则 PY_OK=0）
    [ "$PY_OK" = "1" ] || return 0
    if python3 -c 'import smbus2' >/dev/null 2>&1; then
        echo "    python3-smbus2: 已就绪"
        return 0
    fi
    if command -v dpkg >/dev/null 2>&1; then
        echo "    python3-smbus2 不可用 → 可先执行一次 sudo $WS_ROOT/install_deps.sh 安装；C 例程不受影响"
    else
        echo "    python3-smbus2 不可用 → Python 例程无法运行（本系统没有 dpkg，装不上）；C 例程不受影响"
    fi
    PY_OK=0
}

# ------------------------------------------------------------------ 4/4 构建
# $1=C_DIR  $2=PREBUILT_DIR；返回 0 表示 c/main 可用
ws_prepare_main() {
    local c_dir="$1" prebuilt_dir="$2" arch

    arch="$(uname -m)"

    if [ "$DO_BUILD" != "1" ]; then
        echo "    已通过 --no-build 跳过编译"
    elif [ "$CAN_BUILD" = "1" ]; then
        # 开发板时钟可能落后于 PC，会让 make 报“修改时间在未来”，先统一时间戳
        find "$c_dir" -exec touch {} + 2>/dev/null

        make -C "$c_dir" clean >/dev/null 2>&1
        if make -C "$c_dir"; then
            echo "    生成: $c_dir/main"
        else
            echo "    编译失败"
        fi
    fi

    if [ ! -x "$c_dir/main" ] && [ -f "$prebuilt_dir/main_${arch}_static" ]; then
        # 没有编译工具链（或编译失败）时的兜底：用 prebuilt/ 里预编译好的静态二进制
        cp -f "$prebuilt_dir/main_${arch}_static" "$c_dir/main"
        chmod +x "$c_dir/main"
        echo "    改用预编译二进制: $c_dir/main   （prebuilt/main_${arch}_static）"
    fi

    [ -x "$c_dir/main" ]
}

ws_selfcheck_envsensor() {   # $1=C_DIR：跑 10 秒自动结束的自检
    local c_dir="$1" rc

    echo
    echo "    自检（运行 10 秒后自动结束）:"
    echo "    ----------------------------------------------------------"
    # 用 SIGINT 结束：例程里注册了 SIGINT 处理，能正常退出并刷出缓存里的打印
    # --foreground: 不让 timeout 另建进程组，否则终端 Ctrl+C 打不到 main，脚本退出后 main 会变孤儿继续刷屏
    # -k 2: SIGINT 后 2 秒仍未退出则强杀，避免 timeout 无限等待
    # BusyBox 的 timeout 不认 --foreground（它本身不另建进程组，语义等价），探测后自动退化
    if timeout --foreground -s INT -k 2 10 true >/dev/null 2>&1; then
        timeout --foreground -s INT -k 2 10 "$c_dir/main"
    else
        timeout -s INT -k 2 10 "$c_dir/main"
    fi
    rc=$?
    echo "    ----------------------------------------------------------"
    if [ "$rc" = "124" ] || [ "$rc" = "0" ]; then
        echo "    自检通过"
    else
        echo "    自检异常退出，返回码 $rc"
    fi
}

ws_selfcheck_oled() {        # $1=C_DIR：例程显示完即退出，屏幕会一直保持显示内容
    local c_dir="$1" rc

    echo
    echo "    自检（例程显示完即退出，屏幕会一直保持显示内容）:"
    echo "    ----------------------------------------------------------"
    "$c_dir/main"
    rc=$?
    echo "    ----------------------------------------------------------"
    if [ "$rc" = "0" ]; then
        echo "    自检通过，请查看 OLED 是否显示了欢迎文字"
    else
        echo "    自检异常退出，返回码 $rc"
    fi
}

ws_run_hint() {   # $1=C_DIR  $2=PY_DIR  $3=可选：运行例程时附带的环境变量
    local env="${3:-}" sudo_cmd="sudo "

    # 板上没有 sudo（如 QTI Linux 的 root 环境）时直接给命令，加上 sudo 反而跑不了
    command -v sudo >/dev/null 2>&1 || sudo_cmd=""

    echo
    echo "=============================================================="
    echo " 安装完成，运行方式："
    echo "   C 例程:      ${sudo_cmd}${env:+$env }$1/main"
    echo "   Python 例程: ${sudo_cmd}${env:+$env }python3 $2/test.py"
    echo "=============================================================="
}