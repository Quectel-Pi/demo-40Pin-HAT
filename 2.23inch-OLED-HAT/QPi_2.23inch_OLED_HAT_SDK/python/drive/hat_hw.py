#!/usr/bin/python3
# -*- coding:utf-8 -*-
"""2.23inch OLED HAT hardware layer, pure Python standard library.

Uses /dev/spidevX.Y plus GPIO control, so spidev / gpiozero / smbus / PIL are not
required. GPIO 默认走 /sys/class/gpio；内核未开 CONFIG_GPIO_SYSFS 的板
（如 Quectel H1 / QCS6490）自动改用 GPIO 字符设备 /dev/gpiochipN + ioctl。
板型参数（引脚号、gpiochip、片选）由 a.Waveshare 根目录的一键脚本经环境变量
OLED_DC_GPIO / OLED_RST_GPIO / OLED_GPIO_CHIP / OLED_SPI_CS 下发，源码不含板型预设。
说明与排错见 2.23inch-OLED-HAT/README_<板>.md（L1 / M1 / H1 / M2）。
"""

import ctypes
import fcntl
import glob
import os
import struct
import time

# linux/spi/spidev.h
SPI_IOC_WR_MODE = 0x40016b01
SPI_IOC_WR_BITS_PER_WORD = 0x40016b03
SPI_IOC_WR_MAX_SPEED_HZ = 0x40046b04
# linux/i2c-dev.h
I2C_SLAVE = 0x0703

SPI_MODE = 3            # SSD1305 works in mode 0/3, mode 3 matches the C demo
SPI_SPEED_HZ = 1000000

# 编译期兜底默认（仅当直接运行且未设环境变量时用；M2/RK3576 的值）
DEFAULT_DC = 134
DEFAULT_RST = 76


def resolve_pins():
    """优先级：环境变量 OLED_DC_GPIO / OLED_RST_GPIO > 编译期默认值。
    板型差异由一键脚本以环境变量下发，源码不含板型预设"""
    dc = os.environ.get("OLED_DC_GPIO")
    rst = os.environ.get("OLED_RST_GPIO")
    return (int(dc, 0) if dc else DEFAULT_DC), (int(rst, 0) if rst else DEFAULT_RST)


def resolve_chip():
    """chardev 后端下 DC/RST 所在 gpiochip 的 label 关键字（如 f100000.pinctrl）。
    只认环境变量 OLED_GPIO_CHIP（一键脚本下发）；未设置时返回 None（只有 chardev 后端会用）"""
    return os.environ.get("OLED_GPIO_CHIP") or None


def find_spi_device():
    """挑 SPI 设备节点。
    OLED_SPI_CS=0|1 按片选号找 /dev/spidevX.<n>（如 H1 的 10.0=CE0 / 10.1=CE1）；
    默认优先 CS0（/dev/spidevX.0，HAT 通常接 CE0），没有 CS0 时退回 CS1（.1），
    再没有就取任意一个；OLED_SPI_DEV=/dev/spidevX.Y 指定完整节点（最优先，见 SPI.__init__）。"""
    nodes = sorted(glob.glob("/dev/spidev*.*"))
    if not nodes:
        raise RuntimeError("没有找到 /dev/spidevX.Y：请确认 SPI 已使能（spi1 / spi0）")
    cs = os.environ.get("OLED_SPI_CS")
    if cs and cs.isdigit():
        for node in nodes:
            if os.path.basename(node).endswith("." + cs):
                return node
        raise RuntimeError("OLED_SPI_CS=%s：没有找到 /dev/spidevX.%s" % (cs, cs))
    for suffix in (".0", ".1"):
        for node in nodes:
            if os.path.basename(node).endswith(suffix):
                return node
    return nodes[0]


def find_i2c_bus():
    """HAT 默认走 SPI；改成 I2C 后用 OLED_I2C_BUS 指定总线号"""
    return int(os.environ.get("OLED_I2C_BUS", "1"), 0)


class SPI(object):
    def __init__(self, dev=None, speed=SPI_SPEED_HZ, mode=SPI_MODE):
        self.dev = dev or os.environ.get("OLED_SPI_DEV") or find_spi_device()
        self.speed = speed
        self.mode = mode
        self.fd = -1

    def open(self):
        self.fd = os.open(self.dev, os.O_RDWR)
        fcntl.ioctl(self.fd, SPI_IOC_WR_MODE, struct.pack("B", self.mode))
        fcntl.ioctl(self.fd, SPI_IOC_WR_BITS_PER_WORD, struct.pack("B", 8))
        fcntl.ioctl(self.fd, SPI_IOC_WR_MAX_SPEED_HZ, struct.pack("I", self.speed))
        return self.fd

    def write(self, data):
        # Qualcomm GENI/GSI 控制器偶发首个传输返回 EIO，重试即可
        for _ in range(3):
            try:
                os.write(self.fd, data)
                return
            except OSError as exc:
                if exc.errno != 5:      # EIO
                    raise
                time.sleep(0.001)
        raise OSError(5, "SPI write failed")

    def close(self):
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


class I2C(object):
    def __init__(self, bus=None, addr=0x3C):
        self.bus = find_i2c_bus() if bus is None else bus
        self.addr = addr
        self.fd = -1

    def open(self):
        self.fd = os.open("/dev/i2c-%d" % self.bus, os.O_RDWR)
        fcntl.ioctl(self.fd, I2C_SLAVE, self.addr)
        return self.fd

    def write(self, data):
        os.write(self.fd, data)

    def close(self):
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


SYSFS_GPIO_DIR = "/sys/class/gpio"

# linux/gpio.h, GPIO 字符设备 uAPI v1
GPIO_GET_CHIPINFO_IOCTL = 0x8044B401
GPIO_GET_LINEHANDLE_IOCTL = 0xC16CB403
GPIOHANDLE_GET_LINE_VALUES_IOCTL = 0xC040B408
GPIOHANDLE_SET_LINE_VALUES_IOCTL = 0xC040B409
GPIOHANDLE_REQUEST_INPUT = 1 << 0
GPIOHANDLE_REQUEST_OUTPUT = 1 << 1
GPIOHANDLES_MAX = 64


class _gpiochip_info(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char * 32),
                ("label", ctypes.c_char * 32),
                ("lines", ctypes.c_uint32)]


class _gpiohandle_request(ctypes.Structure):
    _fields_ = [("lineoffsets", ctypes.c_uint32 * GPIOHANDLES_MAX),
                ("flags", ctypes.c_uint32),
                ("default_values", ctypes.c_uint8 * GPIOHANDLES_MAX),
                ("consumer_label", ctypes.c_char * 32),
                ("lines", ctypes.c_uint32),
                ("fd", ctypes.c_int)]


class _gpiohandle_data(ctypes.Structure):
    _fields_ = [("values", ctypes.c_uint8 * GPIOHANDLES_MAX)]


def _chip_index(path):
    try:
        return int(path.rsplit("gpiochip", 1)[1])
    except (IndexError, ValueError):
        return -1


def _list_gpiochips():
    """列出 /dev/gpiochipN: [(设备路径, label, 线数), ...]"""
    chips = []
    for dev in sorted(glob.glob("/dev/gpiochip*"), key=_chip_index):
        try:
            fd = os.open(dev, os.O_RDONLY)
        except OSError:
            continue
        try:
            info = _gpiochip_info()
            fcntl.ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, info)
        except OSError:
            continue
        finally:
            os.close(fd)
        chips.append((dev, info.label.decode("utf-8", "ignore"), info.lines))
    return chips


def find_gpiochip(label):
    """按 label 关键字找 gpiochip，返回 (设备路径, 线数)；找不到时列出全部芯片后报错"""
    chips = _list_gpiochips()
    for dev, chip_label, lines in chips:
        if label in chip_label:
            return dev, lines
    listing = ", ".join("%s[label=%s, %d线]" % c for c in chips) or "无 /dev/gpiochip*"
    raise RuntimeError("找不到 label 含 %r 的 gpiochip：现有 %s" % (label, listing))


class SysfsGPIO(object):
    """sysfs GPIO 后端；退出时不 unexport，保证程序结束后屏幕继续显示"""

    def __init__(self, pin, output=True):
        self.pin = pin
        self.path = "/sys/class/gpio/gpio%d" % pin
        if not os.path.exists(self.path):
            with open("/sys/class/gpio/export", "w") as fp:
                fp.write("%d" % pin)
            time.sleep(0.1)
        with open(self.path + "/direction", "w") as fp:
            fp.write("out" if output else "in")
        self.fd = os.open(self.path + "/value", os.O_WRONLY)

    def write(self, value):
        os.write(self.fd, b"1" if value else b"0")

    def read(self):
        with open(self.path + "/value") as fp:
            return 1 if fp.read(1) == "1" else 0

    def close(self):
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


class CharDevGPIO(object):
    """GPIO 字符设备后端（板子没有 /sys/class/gpio 时自动启用）。

    pin 是 gpiochip 内的线号；进入时请求为输出并持有句柄，写值走 ioctl。
    退出时只关闭句柄、不主动改方向（实测释放后引脚保持输出电平），屏幕继续显示。
    """

    def __init__(self, pin, output=True):
        self.pin = pin
        self.path = None
        self.fd = -1

        label = resolve_chip()
        if not label:
            raise RuntimeError("本板没有 %s，请用 OLED_GPIO_CHIP=<gpiochip label> 指定 DC/RST 所在芯片（一键脚本会自动下发）" % SYSFS_GPIO_DIR)
        self.chip, lines = find_gpiochip(label)
        if not 0 <= pin < lines:
            raise RuntimeError("线号 %d 超出 %s（共 %d 线）" % (pin, self.chip, lines))

        chip_fd = os.open(self.chip, os.O_RDONLY)
        req = _gpiohandle_request()
        req.lineoffsets[0] = pin
        req.default_values[0] = 0
        req.lines = 1
        req.flags = GPIOHANDLE_REQUEST_OUTPUT if output else GPIOHANDLE_REQUEST_INPUT
        req.consumer_label = b"oled-hat"
        try:
            fcntl.ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, req)
        finally:
            os.close(chip_fd)
        self.fd = req.fd
        if self.fd < 0:
            raise RuntimeError("请求 %s 线 %d 失败" % (self.chip, pin))

    def write(self, value):
        data = _gpiohandle_data()
        data.values[0] = 1 if value else 0
        fcntl.ioctl(self.fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, data)

    def read(self):
        data = _gpiohandle_data()
        fcntl.ioctl(self.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, data)
        return 1 if data.values[0] else 0

    def close(self):
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


# 没开 CONFIG_GPIO_SYSFS 的板自动用 chardev 后端；config.py 用同一个 GPIO 名字，无需改动
GPIO = SysfsGPIO if os.path.isdir(SYSFS_GPIO_DIR) else CharDevGPIO

### END OF FILE ###