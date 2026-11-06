#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_
/***********************************************************************************************************************
			------------------------------------------------------------------------
			|\\\																///|
			|\\\					Hardware interface							///|
			------------------------------------------------------------------------
***********************************************************************************************************************/
#include "Debug.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define USE_SPI 1
#define USE_IIC 0

#define IIC_CMD        0X00
#define IIC_RAM        0X40

/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

//OLED Define
/* 两个控制脚的编号：
 *   sysfs 板（有 /sys/class/gpio）是 Linux 全局 GPIO 号；
 *   chardev 板（内核未开 CONFIG_GPIO_SYSFS，如 H1/QCS6490）是 gpiochip 内的线号。
 * 板型差异由 a.Waveshare 根目录的一键脚本 <板>_2.23inch-OLED-HAT.sh 经环境变量
 * OLED_DC_GPIO / OLED_RST_GPIO / OLED_GPIO_CHIP 下发；下面的默认值只在直接运行
 * 且未设环境变量时兜底（M2/RK3576 的值）。详见 2.23inch-OLED-HAT/README_<板>.md */
#ifndef OLED_DC_DEFAULT
#define OLED_DC_DEFAULT  134    /* Quectel Pi M2: 40Pin pin18 = GPIO4_A6 */
#endif
#ifndef OLED_RST_DEFAULT
#define OLED_RST_DEFAULT 76     /* Quectel Pi M2: 40Pin pin22 = GPIO2_B4 */
#endif

extern int OLED_DC;
extern int OLED_RST;

#define OLED_DC_0       DEV_Digital_Write(OLED_DC,0)
#define OLED_DC_1       DEV_Digital_Write(OLED_DC,1)

#define OLED_RST_0      DEV_Digital_Write(OLED_RST,0)
#define OLED_RST_1      DEV_Digital_Write(OLED_RST,1)

/*------------------------------------------------------------------------------------------------------*/

UBYTE DEV_ModuleInit(void);
void  DEV_ModuleExit(void);

void DEV_GPIO_Mode(UWORD Pin, UWORD Mode);
void DEV_Digital_Write(UWORD Pin, UBYTE Value);
UBYTE DEV_Digital_Read(UWORD Pin);
void DEV_Delay_ms(UDOUBLE xms);

void I2C_Write_Byte(uint8_t value, uint8_t Cmd);
void DEV_SPI_WriteByte(UBYTE Value);
void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len);

#endif