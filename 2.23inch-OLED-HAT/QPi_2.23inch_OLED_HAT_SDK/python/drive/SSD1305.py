#coding=utf-8
from . import config
from . import fonts
import time

Device_SPI = config.Device_SPI
Device_I2C = config.Device_I2C

OLED_WIDTH   = 132 #OLED width
OLED_HEIGHT  = 64  #OLED height

#******Rolling Direction******
VERTICAL = True
HORIZONTAL = False
    
# Scrolling constants
SSD1305_ACTIVATE_SCROLL = 0x2F
SSD1305_DEACTIVATE_SCROLL = 0x2E
SSD1305_SET_VERTICAL_SCROLL_AREA = 0xA3
SSD1305_RIGHT_HORIZONTAL_SCROLL = 0x26
SSD1305_LEFT_HORIZONTAL_SCROLL = 0x27
SSD1305_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL = 0x29
SSD1305_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL = 0x2A

class SSD1305(object):
    def __init__(self):
        # Call base class constructor.
        self.width = OLED_WIDTH
        self.height = OLED_HEIGHT
        self._pages = self.height // 8
        self._buffer = [0]*(self.width*self._pages)
        #Initialize DC RST pin
        self.RPI = config.RaspberryPi()
        self._dc = self.RPI.GPIO_DC_PIN
        self._rst = self.RPI.GPIO_RST_PIN
        self.Device = self.RPI.Device

    def command(self, cmd):
        if(self.Device == Device_SPI):
            self.RPI.digital_write(self._dc,False)
            self.RPI.spi_writebyte([cmd])
        else:
            self.RPI.i2c_writebyte(0x00, cmd)


    def Init(self):
        if (self.RPI.module_init() != 0):
            return -1
        """Initialize dispaly"""    
        self.reset()

        # 128x32 pixel specific initialization.
        self.command(0xAE)#--turn off oled panel
        self.command(0x04)#--Set Lower Column Start Address for Page Addressing Mode	
        self.command(0x10)#--Set Higher Column Start Address for Page Addressing Mode
        self.command(0x40)#--Set Display Start Line
        self.command(0x81)#--Set Contrast Control for BANK0
        self.command(0x80)#--Contrast control register is set
        self.command(0xA1)#--Set Segment Re-map
        self.command(0xA6)#--Set Normal/Inverse Display
        self.command(0xA8)#--Set Multiplex Ratio
        self.command(0x1F)
        self.command(0xC8)#--Set COM Output Scan Direction
        self.command(0xD3)#--Set Display Offset
        self.command(0x00)
        self.command(0xD5)#--Set Display Clock Divide Ratio/ Oscillator Frequency
        self.command(0xF0)
        self.command(0xD8)#--Set Area Color Mode ON/OFF & Low Power Display Mode
        self.command(0x05)
        self.command(0xD9)#--Set pre-charge period
        self.command(0xC2)
        self.command(0xDA)#--Set COM Pins Hardware Configuration
        self.command(0x12)
        self.command(0xDB)#--Set VCOMH Deselect Level
        self.command(0x3C)#--Set VCOM Deselect Level（与 C 例程保持一致）
        self.command(0xAF)#--Normal Brightness Display ON
        if self.Device == Device_SPI:
            print("SPI: %s  mode%d  8bit  %dHz"
                  % (self.RPI.spi.dev, self.RPI.spi.mode, self.RPI.spi.speed))
        else:
            print("I2C: bus %d  addr 0x%02X" % (self.RPI.bus.bus, self.RPI.bus.addr))
        print("GPIO: DC=%d  RST=%d" % (config.DC_PIN, config.RST_PIN))

    def reset(self):
        """Reset the display"""
        self.RPI.digital_write(self._rst,True)
        time.sleep(0.1)
        self.RPI.digital_write(self._rst,False)
        time.sleep(0.1)
        self.RPI.digital_write(self._rst,True)
        time.sleep(0.1)
    
    def getbuffer(self, image):
        """Set buffer to value of Python Imaging Library image.  The image should
        be in 1 bit mode and a size equal to the display size.
        """
        if image.mode != '1':
            raise ValueError('Image must be in mode 1.')
        imwidth, imheight = image.size
        if imwidth != self.width or imheight != self.height:
            raise ValueError('Image must be same dimensions as display ({0}x{1}).' \
                .format(self.width, self.height))
        # Grab all the pixels from the image, faster than getpixel.
        pix = image.load()
        # Iterate through the memory pages
        index = 0
        for page in range(self._pages):
            # Iterate through all x axis columns.
            for x in range(self.width):
                # Set the bits for the column of pixels at the current position.
                bits = 0
                # Don't use range here as it's a bit slow
                for bit in [0, 1, 2, 3, 4, 5, 6, 7]:
                    bits = bits << 1
                    bits |= 0 if pix[(x, page*8+7-bit)] == 0 else 1
                # Update buffer byte and increment to next byte.
                self._buffer[index] = bits
                index += 1

    
    def ShowImage(self):
        for page in range(0,self._pages):
            # set page address #
            self.command(0xB0 + page)
            # set low column address #
            self.command(0x04); 
            # set high column address #
            self.command(0x10); 
            # write data #
            # time.sleep(0.01)
            if(self.Device == Device_SPI):
                self.RPI.digital_write(self._dc,True)
            for i in range(0,self.width):
                if(self.Device == Device_SPI):
                    # self.RPI.digital_write(self._dc,True)
                    self.RPI.spi_writebyte([self._buffer[i+self.width*page]]); 
                else :
                    self.RPI.i2c_writebyte(0x40, self._buffer[i+self.width*page])

    def clear(self):
        """Clear contents of image buffer"""
        self.clear_buffer()
        self.ShowImage()

    def clear_buffer(self):
        """只清显存缓冲，不刷屏"""
        self._buffer = [0] * (self.width * self._pages)

    def pixel(self, x, y, color):
        """在显存缓冲里点一个像素（无需 PIL）"""
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return
        index = x + (y // 8) * self.width
        if color:
            self._buffer[index] |= 1 << (y % 8)
        else:
            self._buffer[index] &= ~(1 << (y % 8)) & 0xFF

    def char(self, x, y, ch, size=12, mode=1):
        """用内置 8x12 点阵字库画一个字符，算法与 C 版 SSD1305_char 相同"""
        if size != 12:
            raise ValueError('only the built-in 8x12 font (size=12) is supported')
        index = (ord(ch) - 0x20) * 12
        if index < 0 or index + 12 > len(fonts.FONT1206):
            index = 0
        y0 = y
        for i in range(size):
            temp = fonts.FONT1206[index + i]
            if not mode:
                temp = (~temp) & 0xFF
            for _ in range(8):
                self.pixel(x, y, 1 if (temp & 0x80) else 0)
                temp = (temp << 1) & 0xFF
                y += 1
                if (y - y0) == size:
                    y = y0
                    x += 1
                    break

    def string(self, x, y, text, size=12, mode=1):
        """整串文字，自动换行，算法与 C 版 SSD1305_string 相同"""
        for ch in text:
            if x > (self.width - size // 2):
                x = 0
                y += size
                if y > (self.height - size):
                    x = y = 0
            self.char(x, y, ch, size, mode)
            x += size // 2
    
    def SSD1305_Scrolling_Set(self):
        self.command(SSD1305_DEACTIVATE_SCROLL)
        if HORIZONTAL: 
            self.command(SSD1305_LEFT_HORIZONTAL_SCROLL)
            self.command(0x01)#Set number of column scroll offset
            self.command(0x00)#Define start page address 
            self.command(0x00)#Set time interval between each scroll step in terms of frame frequency
            self.command(0x07) #Define end page address 
        elif VERTICAL:
            self.command(SSD1305_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL)
            self.command(0x00)#Set number of column scroll offset 
            self.command(0x00)#Define start page address
            self.command(0x00)#Set time interval between each scroll step in terms of frame frequency
            self.command(0x07)#Define end page address 
            self.command(0x01)#Vertical scrolling offset
        else:    
            self.command(SSD1305_LEFT_HORIZONTAL_SCROLL)
            self.command(0x00) #Set number of column scroll offset
            self.command(0x00) #Define start page address 
            self.command(0x00) #Set time interval between each scroll step in terms of frame frequency
            self.command(0x07) #Define end page address
    
    def SSD1305_Scrolling_Start(self):
        self.command(SSD1305_ACTIVATE_SCROLL)












