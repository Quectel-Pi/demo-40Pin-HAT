#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_
/***********************************************************************************************************************
			------------------------------------------------------------------------
			|\\\																///|
			|\\\					Hardware interface							///|
			------------------------------------------------------------------------
***********************************************************************************************************************/
/* Linux 版硬件抽象层：spidev + i2c-dev + sysfs GPIO，不依赖 bcm2835 / wiringPi / lgpio。
   板型参数（引脚号、SPI/I2C 设备）由一键脚本经环境变量下发，源码只保留 M2 的兜底值。 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include "Debug.h"

#define IIC_CMD        0X00
#define IIC_RAM        0X40


/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

//LCD Define
/* DC/RST/BL 与四个按键是运行期解析出来的 Linux GPIO 号（环境变量优先） */
extern int LCD_RST;
extern int LCD_DC;
extern int LCD_BL;
extern int KEY_1;
extern int KEY_2;
extern int KEY_3;
extern int KEY_4;

/* 40Pin Pin24 在 M2 上是 SPI1_CE0，由 SPI 控制器做硬件片选；
   例程里原有的软件片选调用全部忽略。 */
extern int LCD_CS;

#define LCD_CS_0      do {} while (0)
#define LCD_CS_1      do {} while (0)

#define LCD_RST_0      DEV_Digital_Write(LCD_RST,0)
#define LCD_RST_1      DEV_Digital_Write(LCD_RST,1)

#define LCD_DC_0       DEV_Digital_Write(LCD_DC,0)
#define LCD_DC_1       DEV_Digital_Write(LCD_DC,1)

#define LCD_BL_0		DEV_Digital_Write(LCD_BL,0)
#define LCD_BL_1		DEV_Digital_Write(LCD_BL,1)

/* 原厂例程用 wiringPi 的线程宏；这里给出 pthread 的等价实现 */
#define PI_THREAD(X)    void *X(void *dummy)
#define piThreadCreate(X) do { \
        pthread_t pi_thread_t; \
        if (pthread_create(&pi_thread_t, NULL, X, NULL) == 0) \
            pthread_detach(pi_thread_t); \
    } while (0)

/*------------------------------------------------------------------------------------------------------*/

UBYTE DEV_ModuleInit(void);
void  DEV_ModuleExit(void);

void DEV_SetBacklight(UWORD Value);
void DEV_GPIO_Mode(int Pin, int Mode);
void DEV_Digital_Write(int Pin, UBYTE Value);
UBYTE DEV_Digital_Read(int Pin);
void DEV_Delay_ms(UDOUBLE xms);

void I2C_Write_Byte(uint8_t value, uint8_t Cmd);
void I2C_Write_nByte(uint8_t *pData, uint32_t Len, uint8_t Cmd);
void DEV_SPI_WriteByte(UBYTE Value);
void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len);

#endif
