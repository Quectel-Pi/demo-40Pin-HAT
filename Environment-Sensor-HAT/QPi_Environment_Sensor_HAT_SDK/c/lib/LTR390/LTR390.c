/**
  ******************************************************************************
  * @file    LTR390.c
  * @author  Waveshare Team
  * @version V1.0
  * @date    Dec-2021
  * @brief   T
  
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
#include "LTR390.h"
  
#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 * interface driver                                                           *
 ******************************************************************************/
 //Raspberry 3B+ platform's default I2C device file
uint32_t uv;
uint16_t LTR390_ReadByte(uint8_t cmd)
{
  return  I2C_Read_Byte(cmd);
}

void LTR390_WriteByte(uint8_t RegAddr, uint8_t value)
{
  I2C_Write_Byte(RegAddr, value);
  return;
}

void LTR390_init()
{
  // DEV_I2C_Init(LTR390_ADDR);
  DEV_I2C_Set_SlaveAddress(LTR390_ADDR);
  uint8_t ID = LTR390_ReadByte(LTR390_PART_ID);
  if(ID != 0xB2)
      printf("read ID error!,Check the hardware...");
  LTR390_WriteByte(LTR390_MAIN_CTRL, 0x0A); //UVS in Active Mode
  LTR390_WriteByte(LTR390_MEAS_RATE, RESOLUTION_20BIT_TIME400MS | RATE_2000MS);//Resolution=18bits, Meas Rate = 100ms
  LTR390_WriteByte(LTR390_GAIN, GAIN_3);    //Gain Range=3.
}

void UVS_value()
{
  uint8_t Data[3];
  DEV_I2C_Set_SlaveAddress(LTR390_ADDR);
  Data[0] = LTR390_ReadByte(LTR390_UVSDATA);
  Data[1] = LTR390_ReadByte(LTR390_UVSDATA+1);
  Data[2] = LTR390_ReadByte(LTR390_UVSDATA+2);
  uv = (Data[2] << 16)| (Data[1] << 8) | Data[0];

}

/************* END ***********************/
#ifdef __cplusplus
}
#endif
