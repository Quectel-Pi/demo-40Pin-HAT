#!/bin/bash
#
# a.Waveshare 离线依赖安装脚本（HAT 共用，装一次即可）
# 用法: sudo ./install_deps.sh
#
# 说明：
#   * 各板一键脚本（<板>_<HAT>.sh）不再安装依赖，只在开头检查是否已装好；
#     需要安装依赖时运行本脚本即可；
#   * 包统一放在本目录 packages/（45 个 arm64 deb），已装好（版本不低于包内版本）
#     的自动跳过，可反复执行；
#   * 第一遍按文件名顺序安装，个别包会因依赖顺序报错，第二遍自动补齐，属正常现象；
#   * 非 Debian 系统（无 dpkg，如 QTI Linux / Yocto）自动跳过，绝不强行安装。
#
set -u

WS_ROOT="$(cd "$(dirname "$0")" && pwd)"
PKG_DIR="$WS_ROOT/packages"

if [ "$(id -u)" != "0" ]; then
    echo "请用 root 运行: sudo $0"
    exit 1
fi

# 单个 deb 是否已按不低于包内的版本安装好（0=已装好，无需再装）
deb_installed() {
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

echo "=============================================================="
echo " 离线依赖安装（HAT 共用，装一次即可）"
echo "=============================================================="

if ! command -v dpkg >/dev/null 2>&1; then
    # 非 Debian 系统（QTI Linux / Yocto 等）：没有 dpkg，也绝不能装这些 deb
    echo "    本系统没有 dpkg，跳过（这不是 Debian 系系统，切勿强行装这些包）"
    exit 0
fi

deb_total=0
deb_missing=()
for deb in "$PKG_DIR"/*.deb; do
    [ -e "$deb" ] || continue
    deb_total=$((deb_total + 1))
    deb_installed "$deb" || deb_missing+=("$deb")
done

if [ "$deb_total" = "0" ]; then
    echo "    未找到 $PKG_DIR/*.deb（packages/ 需与本脚本一起拷贝）"
    exit 1
fi

if [ "${#deb_missing[@]}" = "0" ]; then
    echo "    共 $deb_total 个 deb 包（HAT 共用），检测到全部已安装，跳过"
    exit 0
fi

echo "    共 $deb_total 个 deb 包（HAT 共用），已安装 $((deb_total - ${#deb_missing[@]})) 个，开始安装缺失的 ${#deb_missing[@]} 个..."
# 第一遍：按文件名顺序安装，部分包会因为依赖还没装好而报错，属正常现象
dpkg -i "${deb_missing[@]}"
# 第二遍：此时依赖已齐，把第一遍失败的包补上
dpkg -i "${deb_missing[@]}"

deb_still=0
for deb in "${deb_missing[@]}"; do
    deb_installed "$deb" || deb_still=$((deb_still + 1))
done
if [ "$deb_still" = "0" ]; then
    echo "    完成：依赖已全部装好，可以运行各板一键脚本了"
else
    echo "    完成：仍有 $deb_still 个包未装好（详见上面的 dpkg 报错）"
    exit 1
fi