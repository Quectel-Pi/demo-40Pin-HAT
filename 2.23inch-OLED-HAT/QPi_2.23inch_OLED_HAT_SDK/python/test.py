#!/usr/bin/python3
# -*- coding:utf-8 -*-
"""2.23inch OLED HAT 板端例程（纯标准库，不需要 PIL/spidev/gpiozero）

用法: sudo python3 python/test.py
效果与 C 版 c/examples/main.c 一致。详见 2.23inch-OLED-HAT/README_<板>.md。
"""

import os
import signal
import sys

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

from drive import SSD1305

disp = SSD1305.SSD1305()


def _exit(signo, frame):
    print("\nHandler:exit")
    disp.RPI.module_exit()
    sys.exit(0)


signal.signal(signal.SIGINT, _exit)

disp.Init()

disp.clear()
disp.string(0, 0, "Welcome to use", 12, 1)
disp.string(0, 12, "Waveshare Electronic", 12, 1)
disp.string(0, 24, "2.23inch OLED", 12, 1)
disp.string(0, 36, "128x32 Pixels", 12, 1)
disp.SSD1305_Scrolling_Set()
disp.ShowImage()
disp.SSD1305_Scrolling_Start()

print("done")