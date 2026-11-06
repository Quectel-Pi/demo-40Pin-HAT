#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_
/***********************************************************************************************************************
			------------------------------------------------------------------------
			|\\\																///|
			|\\\					Hardware interface							///|
			------------------------------------------------------------------------
	I2C 直接操作 /dev/i2c-N，不依赖 bcm2835 / wiringPi / lgpio。
***********************************************************************************************************************/
#include <stdint.h>
#include "Debug.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DEV_SPI 0
#define DEV_I2C 1
#define DEV_UART 0

/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

/*-------------------------------------------------*/
uint8_t DEV_ModuleInit(void);
void    DEV_ModuleExit(void);

void DEV_Delay_ms(UDOUBLE xms);

#if DEV_I2C
    void DEV_I2C_Init(uint8_t Add);
    void DEV_I2C_Set_SlaveAddress(uint8_t Add);
    void I2C_Write_Byte(uint8_t Cmd, uint8_t value);
    void I2C_Write_Word(uint8_t Cmd, uint16_t value);
    void I2C_Write_NByte(uint8_t *Cmd, uint16_t num);
    int I2C_Read_Byte(uint8_t Cmd);
    int I2C_Read_Word(uint8_t Cmd);
    void I2C_Read_NByte(uint8_t Cmd, uint8_t *rbuf, uint8_t num);
    /* 裸读：不发寄存器地址，直接读 num 字节 */
    void I2C_Read_Raw(uint8_t *rbuf, uint8_t num);
#endif

#endif