/**
  ******************************************************************************
  * @file    LTR390.h
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
  * <h2><center>&copy; COPYRIGHT 2021 Waveshare</center></h2>
  ******************************************************************************
  */
#ifndef __LTR390_H__
#define __LTR390_H__

#include "DEV_Config.h"

#define true  1
#define false 0
/***********  LTR390_TEST  ****************/

#define LTR390_ADDR 0X53 
#define LTR390_MAIN_CTRL    0x00  // Main control register
#define LTR390_MEAS_RATE    0x04  // Resolution and data rate
#define LTR390_GAIN         0x05  // ALS and UVS gain range
#define LTR390_PART_ID      0x06  // Part id/revision register
#define LTR390_MAIN_STATUS  0x07  // Main status register
#define LTR390_ALSDATA      0x0D  // ALS data lowest byte, 3 byte
#define LTR390_UVSDATA      0x10  // UVS data lowest byte, 3 byte
#define LTR390_INT_CFG      0x19  // Interrupt configuration
#define LTR390_INT_PST      0x1A  // Interrupt persistance config
#define LTR390_THRESH_UP    0x21  // Upper threshold, low byte, 3 byte
#define LTR390_THRESH_LOW   0x24  // Lower threshold, low byte, 3 byte

// ALS/UVS measurement resolution, Gain setting, measurement rate
#define RESOLUTION_20BIT_TIME400MS    0X00
#define RESOLUTION_19BIT_TIME200MS    0X10
#define RESOLUTION_18BIT_TIME100MS    0X20// default
#define RESOLUTION_17BIT_TIME50MS     0x30
#define RESOLUTION_16BIT_TIME25MS     0x40
#define RESOLUTION_13BIT_TIME12_5MS   0x50
#define RATE_25MS   (0x0)
#define RATE_50MS   (0x1)
#define RATE_100MS  (0x2)// default
#define RATE_200MS  (0x3)
#define RATE_500MS  (0x4)
#define RATE_1000MS (0x5)
#define RATE_2000MS (0x6)

// measurement Gain Range.
#define GAIN_1    (0x0)
#define GAIN_3    (0x1)// default
#define GAIN_6    (0x2)
#define GAIN_9    (0x3)
#define GAIN_18   (0x4)

extern uint32_t uv;

void LTR390_init();
void UVS_value();
/***********  END  ****************/

#endif 
