/**
  ******************************************************************************
  * @file    ICM20948.h
  * @author  Waveshare Team
  * @version V1.0
  * @date    Dec-2021
  * @brief   
  
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, WAVESHARE SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2018 Waveshare</center></h2>
  ******************************************************************************
  */
#ifndef __ICM20948_H__
#define __ICM20948_H__
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "DEV_Config.h"

/* define ICM-20948 Device I2C address*/
#define I2C_ADD_ICM20948            0x68
#define I2C_ADD_ICM20948_AK09916    0x0C
#define I2C_ADD_ICM20948_AK09916_READ  0x80
#define I2C_ADD_ICM20948_AK09916_WRITE 0x00
/* define ICM-20948 Register */
/* user bank 0 register */
#define REG_ADD_WIA             0x00
#define REG_VAL_WIA             0xEA
#define REG_ADD_USER_CTRL       0x03
#define REG_VAL_BIT_DMP_EN          0x80
#define REG_VAL_BIT_FIFO_EN         0x40
#define REG_VAL_BIT_I2C_MST_EN      0x20
#define REG_VAL_BIT_I2C_IF_DIS      0x10
#define REG_VAL_BIT_DMP_RST         0x08
#define REG_VAL_BIT_DIAMOND_DMP_RST 0x04
#define REG_ADD_PWR_MIGMT_1     0x06
#define REG_VAL_ALL_RGE_RESET   0x80
#define REG_VAL_RUN_MODE        0x01    //Non low-power mode
#define REG_ADD_LP_CONFIG       0x05
#define REG_ADD_PWR_MGMT_1      0x06
#define REG_ADD_PWR_MGMT_2      0x07
#define REG_ADD_ACCEL_XOUT_H    0x2D
#define REG_ADD_ACCEL_XOUT_L    0x2E
#define REG_ADD_ACCEL_YOUT_H    0x2F
#define REG_ADD_ACCEL_YOUT_L    0x30
#define REG_ADD_ACCEL_ZOUT_H    0x31
#define REG_ADD_ACCEL_ZOUT_L    0x32
#define REG_ADD_GYRO_XOUT_H     0x33
#define REG_ADD_GYRO_XOUT_L     0x34
#define REG_ADD_GYRO_YOUT_H     0x35
#define REG_ADD_GYRO_YOUT_L     0x36
#define REG_ADD_GYRO_ZOUT_H     0x37
#define REG_ADD_GYRO_ZOUT_L     0x38
#define REG_ADD_EXT_SENS_DATA_00 0x3B
#define REG_ADD_REG_BANK_SEL    0x7F
#define REG_VAL_REG_BANK_0  0x00
#define REG_VAL_REG_BANK_1  0x10
#define REG_VAL_REG_BANK_2  0x20
#define REG_VAL_REG_BANK_3  0x30

/* user bank 1 register */
/* user bank 2 register */
#define REG_ADD_GYRO_SMPLRT_DIV 0x00
#define REG_ADD_GYRO_CONFIG_1   0x01
#define REG_VAL_BIT_GYRO_DLPCFG_2   0x10 /* bit[5:3] */
#define REG_VAL_BIT_GYRO_DLPCFG_4   0x20 /* bit[5:3] */
#define REG_VAL_BIT_GYRO_DLPCFG_6   0x30 /* bit[5:3] */
#define REG_VAL_BIT_GYRO_FS_250DPS  0x00 /* bit[2:1] */
#define REG_VAL_BIT_GYRO_FS_500DPS  0x02 /* bit[2:1] */
#define REG_VAL_BIT_GYRO_FS_1000DPS 0x04 /* bit[2:1] */
#define REG_VAL_BIT_GYRO_FS_2000DPS 0x06 /* bit[2:1] */    
#define REG_VAL_BIT_GYRO_DLPF       0x01 /* bit[0]   */
#define REG_ADD_ACCEL_SMPLRT_DIV_2  0x11
#define REG_ADD_ACCEL_CONFIG        0x14
#define REG_VAL_BIT_ACCEL_DLPCFG_2  0x10 /* bit[5:3] */
#define REG_VAL_BIT_ACCEL_DLPCFG_4  0x20 /* bit[5:3] */
#define REG_VAL_BIT_ACCEL_DLPCFG_6  0x30 /* bit[5:3] */
#define REG_VAL_BIT_ACCEL_FS_2g     0x00 /* bit[2:1] */
#define REG_VAL_BIT_ACCEL_FS_4g     0x02 /* bit[2:1] */
#define REG_VAL_BIT_ACCEL_FS_8g     0x04 /* bit[2:1] */
#define REG_VAL_BIT_ACCEL_FS_16g    0x06 /* bit[2:1] */    
#define REG_VAL_BIT_ACCEL_DLPF      0x01 /* bit[0]   */

/* user bank 3 register */
#define REG_ADD_I2C_SLV0_ADDR   0x03
#define REG_ADD_I2C_SLV0_REG    0x04
#define REG_ADD_I2C_SLV0_CTRL   0x05
#define REG_VAL_BIT_SLV0_EN     0x80
#define REG_VAL_BIT_MASK_LEN    0x07
#define REG_ADD_I2C_SLV0_DO     0x06
#define REG_ADD_I2C_SLV1_ADDR   0x07
#define REG_ADD_I2C_SLV1_REG    0x08
#define REG_ADD_I2C_SLV1_CTRL   0x09
#define REG_ADD_I2C_SLV1_DO     0x0A

/* define ICM-20948 Register  end */

/* define ICM-20948 MAG Register  */
#define REG_ADD_MAG_WIA1    0x00
#define REG_VAL_MAG_WIA1    0x48
#define REG_ADD_MAG_WIA2    0x01
#define REG_VAL_MAG_WIA2    0x09
#define REG_ADD_MAG_ST2     0x10
#define REG_ADD_MAG_DATA    0x11
#define REG_ADD_MAG_CNTL2   0x31
#define REG_VAL_MAG_MODE_PD     0x00
#define REG_VAL_MAG_MODE_SM     0x01
#define REG_VAL_MAG_MODE_10HZ   0x02
#define REG_VAL_MAG_MODE_20HZ   0x04
#define REG_VAL_MAG_MODE_50HZ   0x05
#define REG_VAL_MAG_MODE_100HZ  0x08
#define REG_VAL_MAG_MODE_ST     0x10
/* define ICM-20948 MAG Register  end */

#define MAG_DATA_LEN    6

typedef struct
{
  int16_t s16X;
  int16_t s16Y;
  int16_t s16Z;
}ICM20948_TypeDef;

typedef struct
{
  int16_t X_Off_Err;
  int16_t Y_Off_Err;
  int16_t Z_Off_Err;
}ICM20948_TypeDef_Off;

typedef struct icm20948_st_avg_data_tag
{
  uint8_t u8Index;
  int16_t s16AvgBuffer[8];
}ICM20948_ST_AVG_DATA;

void ICM20948_Init(void);
bool ICM20948_Check(void);
void ICM20948_calibrateMagn(void);
void ICM20948_READ_GYRO(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z);
void ICM20948_READ_ACCEL(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z);
void ICM20948_READ_MAG(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z);
bool ICM20948MagCheck(void);
void ICM20948CalAvgValue(uint8_t *pIndex, int16_t *pAvgBuffer, int16_t InVal, int32_t *pOutVal);
void ICM20948GyroOffset(void);
void ICM20948ReadSecondary(uint8_t u8I2CAddr, uint8_t u8RegAddr, uint8_t u8Len, uint8_t *pu8data);
void ICM20948WriteSecondary(uint8_t u8I2CAddr, uint8_t u8RegAddr, uint8_t u8data);

#ifdef __cplusplus
}
#endif

#endif //__Waveshare_10DOF_D_H__
