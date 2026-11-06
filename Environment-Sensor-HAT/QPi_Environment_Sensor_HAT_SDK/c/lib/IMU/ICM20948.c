/**
  ******************************************************************************
  * @file    Waveshare_10Dof-D.c
  * @author  Waveshare Team
  * @version V1.0
  * @date    Dec-2018
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
  * <h2><center>&copy; COPYRIGHT 2018 Waveshare</center></h2>
  ******************************************************************************
  */
#include "ICM20948.h"
#include "DEV_Config.h"
ICM20948_TypeDef gstGyroOffset ={0,0,0};  
ICM20948_TypeDef_Off ICM20948_Magn_Offset={0};
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * interface driver                                                           *
 ******************************************************************************/
 //Raspberry 3B+ platform's default I2C device file

uint8_t ICM20948_I2C_ReadOneByte(uint8_t RegAddr)
{
  uint8_t u8Ret;
  u8Ret = I2C_Read_Byte(RegAddr);
  return u8Ret;
}

void ICM20948_I2C_WriteOneByte(uint8_t RegAddr, uint8_t value)
{
  I2C_Write_Byte(RegAddr,value);
}
/******************************************************************************
 * IMU module                                                                 *
 ******************************************************************************/
#define Kp 4.50f   // proportional gain governs rate of convergence to accelerometer/magnetometer
#define Ki 1.0f    // integral gain governs rate of convergence of gyroscope biases

/******************************************************************************
 * icm20948 sensor device                                                     *
 ******************************************************************************/
void ICM20948_Init()
{
  /* user bank 0 register */
  ICM20948_I2C_WriteOneByte(REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0);
  ICM20948_I2C_WriteOneByte( REG_ADD_PWR_MIGMT_1,  REG_VAL_ALL_RGE_RESET);
  DEV_Delay_ms(10);
  ICM20948_I2C_WriteOneByte(REG_ADD_PWR_MIGMT_1,  REG_VAL_RUN_MODE);  

  /* user bank 2 register */
  ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_2);
  ICM20948_I2C_WriteOneByte( REG_ADD_GYRO_SMPLRT_DIV, 0x07);
  ICM20948_I2C_WriteOneByte( REG_ADD_GYRO_CONFIG_1,   
                  REG_VAL_BIT_GYRO_DLPCFG_6 | REG_VAL_BIT_GYRO_FS_1000DPS | REG_VAL_BIT_GYRO_DLPF);
  ICM20948_I2C_WriteOneByte(  REG_ADD_ACCEL_SMPLRT_DIV_2,  0x07);
  ICM20948_I2C_WriteOneByte(  REG_ADD_ACCEL_CONFIG,
                  REG_VAL_BIT_ACCEL_DLPCFG_6 | REG_VAL_BIT_ACCEL_FS_2g | REG_VAL_BIT_ACCEL_DLPF);

  /* user bank 0 register */
  ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); 

  DEV_Delay_ms(100);
  /* offset */
  ICM20948GyroOffset();

  ICM20948MagCheck();

  ICM20948WriteSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_WRITE,
                               REG_ADD_MAG_CNTL2, REG_VAL_MAG_MODE_20HZ);  
  return;
}

bool ICM20948_Check(void)
{
    bool bRet = false;
    if(REG_VAL_WIA == I2C_Read_Byte( REG_ADD_WIA))
    {
        bRet = true;
    }
    return bRet;
}

void ICM20948_READ_GYRO(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{
   uint8_t u8Buf[6];
    int16_t s16Buf[3] = {0}; 
    uint8_t i;
    int32_t s32OutBuf[3] = {0};
    static ICM20948_ST_AVG_DATA sstAvgBuf[3];
    static int16_t ss16c = 0;
    ss16c++;

    u8Buf[0]=ICM20948_I2C_ReadOneByte(REG_ADD_GYRO_XOUT_L); 
    u8Buf[1]=ICM20948_I2C_ReadOneByte(REG_ADD_GYRO_XOUT_H);
    s16Buf[0]=  (u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=ICM20948_I2C_ReadOneByte(REG_ADD_GYRO_YOUT_L); 
    u8Buf[1]=ICM20948_I2C_ReadOneByte(REG_ADD_GYRO_YOUT_H);
    s16Buf[1]=  (u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=ICM20948_I2C_ReadOneByte(REG_ADD_GYRO_ZOUT_L); 
    u8Buf[1]=ICM20948_I2C_ReadOneByte(REG_ADD_GYRO_ZOUT_H);
    s16Buf[2]=  (u8Buf[1]<<8)|u8Buf[0];

    for(i = 0; i < 3; i ++) 
    {
        ICM20948CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
    }
    *ps16X = s32OutBuf[0] - gstGyroOffset.s16X;
    *ps16Y = s32OutBuf[1] - gstGyroOffset.s16Y;
    *ps16Z = s32OutBuf[2] - gstGyroOffset.s16Z;
    
    return;
}
void ICM20948_READ_ACCEL(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{
    uint8_t u8Buf[2];
    int16_t s16Buf[3] = {0}; 
    uint8_t i;
    int32_t s32OutBuf[3] = {0};
    static ICM20948_ST_AVG_DATA sstAvgBuf[3];

    u8Buf[0]=ICM20948_I2C_ReadOneByte(REG_ADD_ACCEL_XOUT_L); 
    u8Buf[1]=ICM20948_I2C_ReadOneByte(REG_ADD_ACCEL_XOUT_H);
    s16Buf[0]=  (u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=ICM20948_I2C_ReadOneByte(REG_ADD_ACCEL_YOUT_L); 
    u8Buf[1]=ICM20948_I2C_ReadOneByte(REG_ADD_ACCEL_YOUT_H);
    s16Buf[1]=  (u8Buf[1]<<8)|u8Buf[0];

    u8Buf[0]=ICM20948_I2C_ReadOneByte(REG_ADD_ACCEL_ZOUT_L); 
    u8Buf[1]=ICM20948_I2C_ReadOneByte(REG_ADD_ACCEL_ZOUT_H);
    s16Buf[2]=  (u8Buf[1]<<8)|u8Buf[0];

    for(i = 0; i < 3; i ++) 
    {
        ICM20948CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
    }
    *ps16X = s32OutBuf[0];
    *ps16Y = s32OutBuf[1];
    *ps16Z = s32OutBuf[2];
  
    return;

}
void ICM20948_READ_MAG(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{
    uint8_t counter = 20;
    uint8_t u8Data[MAG_DATA_LEN];
    int16_t s16Buf[3] = {0}; 
    uint8_t i;
    int32_t s32OutBuf[3] = {0};
    static ICM20948_ST_AVG_DATA sstAvgBuf[3];
    while( counter>0 )
    {
        DEV_Delay_ms(10);
        ICM20948ReadSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_READ, 
                                    REG_ADD_MAG_ST2, 1, u8Data);
        
        if ((u8Data[0] & 0x01) != 0)
            break;
        
        counter--;
    }
    
    if(counter != 0)
    {
        ICM20948ReadSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_READ, 
                                    REG_ADD_MAG_DATA, 
                                    MAG_DATA_LEN,
                                    u8Data);
        s16Buf[0] = ((int16_t)u8Data[1]<<8) | u8Data[0];
        s16Buf[1] = ((int16_t)u8Data[3]<<8) | u8Data[2];
        s16Buf[2] = ((int16_t)u8Data[5]<<8) | u8Data[4];       
    }

    for(i = 0; i < 3; i ++) 
    {
        ICM20948CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
    }
    
    *ps16X =  s32OutBuf[0];
    *ps16Y = -s32OutBuf[1];
    *ps16Z = -s32OutBuf[2];
    return;
}

void ICM20948ReadSecondary(uint8_t u8I2CAddr, uint8_t u8RegAddr, uint8_t u8Len, uint8_t *pu8data)
{
    uint8_t i;
    uint8_t u8Temp;

    ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL,  REG_VAL_REG_BANK_3); //swtich bank3
    ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV0_ADDR, u8I2CAddr);
    ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV0_REG,  u8RegAddr);
    ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV0_CTRL, REG_VAL_BIT_SLV0_EN|u8Len);

    ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0
    
    u8Temp = ICM20948_I2C_ReadOneByte(REG_ADD_USER_CTRL);
    u8Temp |= REG_VAL_BIT_I2C_MST_EN;
    ICM20948_I2C_WriteOneByte( REG_ADD_USER_CTRL, u8Temp);
    DEV_Delay_ms(5);
    u8Temp &= ~REG_VAL_BIT_I2C_MST_EN;
    ICM20948_I2C_WriteOneByte( REG_ADD_USER_CTRL, u8Temp);
    
    for(i=0; i<u8Len; i++)
    {
        *(pu8data+i) = ICM20948_I2C_ReadOneByte( REG_ADD_EXT_SENS_DATA_00+i);
        
    }
    ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_3); //swtich bank3
    
    u8Temp = ICM20948_I2C_ReadOneByte(REG_ADD_I2C_SLV0_CTRL);
    u8Temp &= ~((REG_VAL_BIT_I2C_MST_EN)&(REG_VAL_BIT_MASK_LEN));
    ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV0_CTRL,  u8Temp);
    
    ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0

}

void ICM20948WriteSecondary(uint8_t u8I2CAddr, uint8_t u8RegAddr, uint8_t u8data)
{
  uint8_t u8Temp;
  ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL,  REG_VAL_REG_BANK_3); //swtich bank3
  ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV1_ADDR, u8I2CAddr);
  ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV1_REG,  u8RegAddr);
  ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV1_DO,   u8data);
  ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV1_CTRL, REG_VAL_BIT_SLV0_EN|1);

  ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0

  u8Temp = ICM20948_I2C_ReadOneByte(REG_ADD_USER_CTRL);
  u8Temp |= REG_VAL_BIT_I2C_MST_EN;
  ICM20948_I2C_WriteOneByte(REG_ADD_USER_CTRL, u8Temp);
  DEV_Delay_ms(5);
  u8Temp &= ~REG_VAL_BIT_I2C_MST_EN;
  ICM20948_I2C_WriteOneByte( REG_ADD_USER_CTRL, u8Temp);

  ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_3); //swtich bank3

  u8Temp = ICM20948_I2C_ReadOneByte(REG_ADD_I2C_SLV0_CTRL);
  u8Temp &= ~((REG_VAL_BIT_I2C_MST_EN)&(REG_VAL_BIT_MASK_LEN));
  ICM20948_I2C_WriteOneByte( REG_ADD_I2C_SLV0_CTRL,  u8Temp);

  ICM20948_I2C_WriteOneByte( REG_ADD_REG_BANK_SEL, REG_VAL_REG_BANK_0); //swtich bank0
    
    return;
}

void ICM20948CalAvgValue(uint8_t *pIndex, int16_t *pAvgBuffer, int16_t InVal, int32_t *pOutVal)
{ 
  uint8_t i;
  
  *(pAvgBuffer + ((*pIndex) ++)) = InVal;
    *pIndex &= 0x07;
    
    *pOutVal = 0;
  for(i = 0; i < 8; i ++) 
    {
      *pOutVal += *(pAvgBuffer + i);
    }
    *pOutVal >>= 3;
}

void ICM20948GyroOffset(void)
{
  uint8_t i;
  int16_t s16Gx = 0, s16Gy = 0, s16Gz = 0;
  int32_t s32TempGx = 0, s32TempGy = 0, s32TempGz = 0;
  for(i = 0; i < 32; i ++)
  {
    ICM20948_READ_GYRO(&s16Gx, &s16Gy, &s16Gz);
    s32TempGx += s16Gx;
    s32TempGy += s16Gy;
    s32TempGz += s16Gz;
    DEV_Delay_ms(10);
  }
  gstGyroOffset.s16X = s32TempGx >> 5;
  gstGyroOffset.s16Y = s32TempGy >> 5;
  gstGyroOffset.s16Z = s32TempGz >> 5;
  return;
}

bool ICM20948MagCheck(void)
{
    bool bRet = false;
    uint8_t u8Ret[2];
    
    ICM20948ReadSecondary( I2C_ADD_ICM20948_AK09916|I2C_ADD_ICM20948_AK09916_READ,
                                REG_ADD_MAG_WIA1, 2,u8Ret);
    if( (u8Ret[0] == REG_VAL_MAG_WIA1) && ( u8Ret[1] == REG_VAL_MAG_WIA2) )
    {
        bRet = true;
    }
    
    return bRet;
}

void ICM20948_calibrateMagn(void)
{
  /*
  int16_t s16Gx = 0, s16Gy = 0, s16Gz = 0;
  int16_t temp[9];
  printf("keep 10dof-imu device horizontal and it will read x y z axis offset value after 4 seconds\n");
  DEV_Delay_ms(4000);
  printf("start read all axises offset value\n");
  ICM20948_READ_MAG(&s16Gx, &s16Gy, &s16Gz);
  temp[0] = s16Gx;
  temp[1] = s16Gy;
  temp[2] = s16Gz;
  
  printf("rotate z axis 180 degrees and it will read all axises offset value after 4 seconds\n");
  DEV_Delay_ms(4000);
  printf("start read all axises offset value\n");
  ICM20948_READ_MAG(&s16Gx, &s16Gy, &s16Gz);
  temp[3] = s16Gx;
  temp[4] = s16Gy;
  temp[5] = s16Gz;

  printf("flip 10dof-imu device and keep it horizontal and it will read all axises offset value after 4 seconds\n");
  DEV_Delay_ms(4000);
  printf("start read all axises offset value\n");
  ICM20948_READ_MAG(&s16Gx, &s16Gy, &s16Gz);
  temp[6] = s16Gx;
  temp[7] = s16Gy;
  temp[8] = s16Gz;

  ICM20948_Magn_Offset.X_Off_Err = (temp[0]+temp[3])/2;
  ICM20948_Magn_Offset.Y_Off_Err = (temp[1]+temp[4])/2;
  ICM20948_Magn_Offset.Z_Off_Err = (temp[5]+temp[8])/2;
  */
  //Comment the following three lines, remove the above function,
  //then enter the calibration mode, the default is the factory calibration value
  ICM20948_Magn_Offset.X_Off_Err = 15;
  ICM20948_Magn_Offset.Y_Off_Err = 17;
  ICM20948_Magn_Offset.Z_Off_Err = -90;
//  printf("X Y Z :%d %d %d\r\n",ICM20948_Magn_Offset.X_Off_Err,ICM20948_Magn_Offset.Y_Off_Err,ICM20948_Magn_Offset.Z_Off_Err); 
}

#ifdef __cplusplus
}
#endif
