#!/usr/bin/python3
# -*- coding:utf-8 -*-
"""Environment Sensor HAT —— I2C 总线自动识别

官方例程写死 SMBus(1)（树莓派排针在 i2c-1），其它开发板可能挂在别的控制器上，
这里优先读环境变量 HAT_I2C_BUS，否则逐条探测。详见 Environment-Sensor-HAT/README_<板>.md。
"""
import os
import glob

import smbus2

BME280_ADDR = 0x76
BME280_ID_REG = 0xD0
BME280_ID_VAL = 0x60


def _bus_number(path):
    """从 /dev/i2c-3 取出总线号 3（不能用 \\D 取数字，'i2c' 里的 2 会被算进去）"""
    return int(path.rsplit('-', 1)[1])


def _probe(bus):
    """能读到 BME280 就认为排针在这条总线上"""
    try:
        with smbus2.SMBus(bus) as i2c:
            return i2c.read_byte_data(BME280_ADDR, BME280_ID_REG) == BME280_ID_VAL
    except Exception:
        return False


def detect_bus():
    env = os.environ.get('HAT_I2C_BUS')
    if env:
        return int(env, 0)

    for path in sorted(glob.glob('/dev/i2c-*'), key=_bus_number):
        bus = _bus_number(path)
        if _probe(bus):
            return bus

    raise RuntimeError('没有找到 Environment Sensor HAT：请确认排针插好，'
                       '或用 HAT_I2C_BUS=n 手动指定 I2C 总线')


# 各传感器驱动模块统一从这里取总线号
BUS = detect_bus()