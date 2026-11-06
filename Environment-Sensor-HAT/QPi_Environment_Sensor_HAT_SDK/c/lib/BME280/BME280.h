/**
  ******************************************************************************
  * @file    SGP40.h
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
#ifndef __BME280_H__
#define __BME280_H__

#include "DEV_Config.h"

#define true  1
#define false 0
/***********  BME280_TEST  ****************/

#define BME280_ADDR 0x76

#define ctrl_meas_reg 0x27
#define config_reg 0xA0
#define ctrl_hum_reg 0x01
extern double pres_raw[3];
void BME280_Init();
void BME280_value();
/***********  END  ****************/

#endif 
