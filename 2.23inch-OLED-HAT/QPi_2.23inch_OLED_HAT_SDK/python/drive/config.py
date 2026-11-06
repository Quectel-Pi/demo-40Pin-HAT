#!/usr/bin/python3
# -*- coding:utf-8 -*-
"""Hardware underlying interface for the 2.23inch OLED HAT.

Only /dev/spidevX.Y and sysfs GPIO are used, so spidev / gpiozero / smbus are
not required. 说明、引脚映射与排错见 2.23inch-OLED-HAT/README_<板>.md。
"""

import time

from . import hat_hw

# Pin definition（Linux GPIO 编号，非树莓派 BCM）
DC_PIN, RST_PIN = hat_hw.resolve_pins()

Device_SPI = 1
Device_I2C = 0
I2C_ADDR = 0x3C


class RaspberryPi(object):
    """保留原厂类名，方便和树莓派版代码对照"""

    def __init__(self, spi=None, spi_freq=hat_hw.SPI_SPEED_HZ,
                 rst=None, dc=None, i2c=None):
        self.INPUT = False
        self.OUTPUT = True

        self.dc_num = DC_PIN if dc is None else dc
        self.rst_num = RST_PIN if rst is None else rst

        if Device_SPI == 1:
            self.Device = Device_SPI
            self.spi = hat_hw.SPI(spi, spi_freq) if spi is None else spi
        else:
            self.Device = Device_I2C
            self.address = I2C_ADDR
            self.bus = hat_hw.I2C() if i2c is None else i2c

        self.GPIO_RST_PIN = self.gpio_mode(self.rst_num, self.OUTPUT)
        self.GPIO_DC_PIN = self.gpio_mode(self.dc_num, self.OUTPUT)

    def delay_ms(self, delaytime):
        time.sleep(delaytime / 1000.0)

    def gpio_mode(self, Pin, Mode):
        return hat_hw.GPIO(Pin, output=bool(Mode))

    def digital_write(self, Pin, value):
        Pin.write(value)

    def digital_read(self, Pin):
        return Pin.read()

    def spi_writebyte(self, data):
        self.spi.write(bytes(bytearray(data)))

    def i2c_writebyte(self, reg, value):
        self.bus.write(bytes(bytearray([reg, value])))

    def module_init(self):
        self.digital_write(self.GPIO_RST_PIN, False)
        if self.Device == Device_SPI:
            self.spi.open()
        else:
            self.bus.open()
        self.digital_write(self.GPIO_DC_PIN, False)
        return 0

    def module_exit(self):
        # 不 unexport、RST 保持高、DC 保持低，屏幕退出后继续显示
        self.digital_write(self.GPIO_DC_PIN, False)
        self.digital_write(self.GPIO_RST_PIN, True)
        if self.Device == Device_SPI:
            self.spi.close()
        else:
            self.bus.close()
        self.GPIO_RST_PIN.close()
        self.GPIO_DC_PIN.close()

### END OF FILE ###