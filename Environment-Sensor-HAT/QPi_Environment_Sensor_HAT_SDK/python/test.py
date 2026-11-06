#!/usr/bin/python3
# -*- coding:utf-8 -*-
"""Environment Sensor HAT 综合例程（适配非树莓派开发板）

与官方 python/test.py 的区别：
  1. I2C 总线不再写死 i2c-1，由 hat_bus 自动探测（也可用环境变量 HAT_I2C_BUS 指定）
  2. 改用纯 Python 的 smbus2，不再依赖 python3-smbus / RPi.GPIO / Pillow
  3. MPU925x 按需导入，本 HAT 板载的是 ICM20948

用法：
    python3 test.py
    HAT_I2C_BUS=3 python3 test.py
"""
import smbus2 as smbus

from hat_bus import BUS

import ICM20948  # 陀螺仪/加速度/磁力计
import BME280    # 气压/温度/湿度
import LTR390    # 紫外
import TSL2591   # 光照
import SGP40     # VOC

MPU_VAL_WIA = 0x71
MPU_ADD_WIA = 0x75
ICM_VAL_WIA = 0xEA
ICM_ADD_WIA = 0x00
ICM_SLAVE_ADDRESS = 0x68

bus = smbus.SMBus(BUS)

bme280 = BME280.BME280()
bme280.get_calib_param()
light = TSL2591.TSL2591()
uv = LTR390.LTR390()
sgp = SGP40.SGP40()

device_id1 = bus.read_byte_data(ICM_SLAVE_ADDRESS, ICM_ADD_WIA)
device_id2 = bus.read_byte_data(ICM_SLAVE_ADDRESS, MPU_ADD_WIA)
if device_id1 == ICM_VAL_WIA:
    mpu = ICM20948.ICM20948()
    print("ICM20948 9-DOF I2C address:0X68")
elif device_id2 == MPU_VAL_WIA:
    import MPU925x
    mpu = MPU925x.MPU925x()
    print("MPU925x 9-DOF I2C address:0X68")
else:
    raise RuntimeError("I2C 地址 0x68 上既不是 ICM20948 也不是 MPU925x，"
                       "读取到的 ID: 0x%02X / 0x%02X" % (device_id1, device_id2))

print("I2C Bus: i2c-%d" % BUS)
print("TSL2591 Light I2C address:0X29")
print("LTR390 UV I2C address:0X53")
print("SGP40 VOC I2C address:0X59")
print("bme280 T&H I2C address:0X76")

try:
    while True:
        bme = []
        bme = bme280.readData()
        pressure = round(bme[0], 2)
        temp = round(bme[1], 2)
        hum = round(bme[2], 2)

        lux = round(light.Lux(), 2)

        UVS = uv.UVS()

        gas = round(sgp.raw(), 2)

        icm = []
        icm = mpu.getdata()

        print("==================================================")
        print("pressure : %7.2f hPa" % pressure)
        print("temp : %-6.2f ℃" % temp)
        print("hum : %6.2f ％" % hum)
        print("lux : %d " % lux)
        print("uv : %d " % UVS)
        print("gas : %6.2f " % gas)
        print("Roll = %.2f , Pitch = %.2f , Yaw = %.2f" % (icm[0], icm[1], icm[2]))
        print("Acceleration: X = %d, Y = %d, Z = %d" % (icm[3], icm[4], icm[5]))
        print("Gyroscope:     X = %d , Y = %d , Z = %d" % (icm[6], icm[7], icm[8]))
        print("Magnetic:      X = %d , Y = %d , Z = %d" % (icm[9], icm[10], icm[11]))

except KeyboardInterrupt:
    exit()