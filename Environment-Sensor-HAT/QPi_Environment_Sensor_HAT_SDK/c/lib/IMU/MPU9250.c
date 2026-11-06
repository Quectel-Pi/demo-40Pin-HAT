#include "MPU9250.h"
#include <stdbool.h>

int16_t magn[3];
int16_t accel[3], gyro[3];
MPU9250_TypeDef MPU9250_Offset={0};
MPU9250_TypeDef_Off MPU9250_Magn_Offset={0};

#ifdef __cplusplus
extern "C" {
#endif

uint8_t MPU9250_I2C_ReadOneByte(uint8_t RegAddr)
{
  uint8_t u8Ret;
  u8Ret = I2C_Read_Byte(RegAddr);
  return u8Ret;
}

void MPU9250_I2C_WriteOneByte(uint8_t RegAddr, uint8_t value)
{
  I2C_Write_Byte(RegAddr,value);
}

/**
  * @brief  Initializes MPU9250
  * @param  None
  * @retval None
  */
void MPU9250_Init(void)
{
  MPU9250_I2C_WriteOneByte(PWR_MGMT_1, 0x00);
  DEV_Delay_ms(100);
  MPU9250_I2C_WriteOneByte(PWR_MGMT_1, 0x01 );
  DEV_Delay_ms(100);
  MPU9250_I2C_WriteOneByte(CONFIG, 0x03);
  MPU9250_I2C_WriteOneByte(SMPLRT_DIV, 0x04);
  MPU9250_I2C_WriteOneByte(GYRO_CONFIG, 0x18);
  MPU9250_I2C_WriteOneByte(ACCEL_CONFIG, 0x01);
  MPU9250_I2C_WriteOneByte(ACCEL_CONFIG_2, 0x03);
  MPU9250_I2C_WriteOneByte(INT_PIN_CFG, 0x02);
  DEV_Delay_ms(100);

  DEV_I2C_Set_SlaveAddress(MAG_ADDRESS);
  MPU9250_I2C_WriteOneByte(0x0A, 0x00);
  DEV_Delay_ms(50);
  MPU9250_I2C_WriteOneByte(0x0A, 0x0F);
  DEV_Delay_ms(50);
  MPU9250_I2C_WriteOneByte(0x0A, 0x00);
  DEV_Delay_ms(50);
  MPU9250_I2C_WriteOneByte(0x0A, 0x12);
  DEV_Delay_ms(100);

  DEV_I2C_Set_SlaveAddress(DEFAULT_ADDRESS);
  MPU9250_InitGyrOffset();
}

/**
  * @brief Get accelerometer datas
  * @param  None
  * @retval None
  */
void MPU9250_READ_ACCEL(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{ 
  uint8_t u8Buf[6];
  int16_t s16Buf[3] = {0};
  uint8_t i;
  int32_t s32OutBuf[3] = {0};
  static MPU9250_ST_AVG_DATA sstAvgBuf[3];

  DEV_I2C_Set_SlaveAddress(ACCEL_ADDRESS);
  u8Buf[0]=MPU9250_I2C_ReadOneByte(ACCEL_XOUT_L); 
  u8Buf[1]=MPU9250_I2C_ReadOneByte(ACCEL_XOUT_H);
  s16Buf[0]= (u8Buf[1]<<8)|u8Buf[0];

  u8Buf[2]=MPU9250_I2C_ReadOneByte(ACCEL_YOUT_L);
  u8Buf[3]=MPU9250_I2C_ReadOneByte(ACCEL_YOUT_H);
  s16Buf[1]= (u8Buf[3]<<8)|u8Buf[2];
            
  u8Buf[4]=MPU9250_I2C_ReadOneByte(ACCEL_ZOUT_L);
  u8Buf[5]=MPU9250_I2C_ReadOneByte(ACCEL_ZOUT_H);
  s16Buf[2]= (u8Buf[5]<<8)|u8Buf[4];            
  
  for(i = 0; i < 3; i ++)  
  {
    MPU9250_CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
  }
  *ps16X = s32OutBuf[0];
  *ps16Y = s32OutBuf[1];
  *ps16Z = s32OutBuf[2];

}

/**
  * @brief Get gyroscopes datas
  * @param  None
  * @retval None
  */
void MPU9250_READ_GYRO(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{ 
  uint8_t u8Buf[6];
  int16_t s16Buf[3] = {0};
  uint8_t i;
  int32_t s32OutBuf[3] = {0};
  static MPU9250_ST_AVG_DATA sstAvgBuf[3];

  DEV_I2C_Set_SlaveAddress(GYRO_ADDRESS);
  u8Buf[0]=MPU9250_I2C_ReadOneByte(GYRO_XOUT_L); 
  u8Buf[1]=MPU9250_I2C_ReadOneByte(GYRO_XOUT_H);
  s16Buf[0]= (u8Buf[1]<<8)|u8Buf[0];
  
  u8Buf[2]=MPU9250_I2C_ReadOneByte(GYRO_YOUT_L);
  u8Buf[3]=MPU9250_I2C_ReadOneByte(GYRO_YOUT_H);
  s16Buf[1] = (u8Buf[3]<<8)|u8Buf[2];
  
  u8Buf[4]=MPU9250_I2C_ReadOneByte(GYRO_ZOUT_L);
  u8Buf[5]=MPU9250_I2C_ReadOneByte(GYRO_ZOUT_H);
  s16Buf[2] = (u8Buf[5]<<8)|u8Buf[4];  

  for(i = 0; i < 3; i ++)  
  {
    MPU9250_CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
  }
  *ps16X = s32OutBuf[0] - MPU9250_Offset.s16X;
  *ps16Y = s32OutBuf[1] - MPU9250_Offset.s16Y;
  *ps16Z = s32OutBuf[2] - MPU9250_Offset.s16Z;
}

/**
  * @brief Get compass datas
  * @param  None
  * @retval None
  */
void MPU9250_READ_MAG(int16_t* ps16X, int16_t* ps16Y, int16_t* ps16Z)
{ 
  uint8_t u8Buf[6];
  int16_t s16Buf[3] = {0};
  uint8_t i;
  int32_t s32OutBuf[3] = {0};
  static MPU9250_ST_AVG_DATA sstAvgBuf[3];

  DEV_I2C_Set_SlaveAddress(MAG_ADDRESS);
  MPU9250_I2C_WriteOneByte(0x37,0x02);//turn on Bypass Mode 
  DEV_Delay_ms(10);
  MPU9250_I2C_WriteOneByte(0x0A,0x01);  
  DEV_Delay_ms(10);

  u8Buf[0]=MPU9250_I2C_ReadOneByte(MAG_XOUT_L);
  u8Buf[1]=MPU9250_I2C_ReadOneByte(MAG_XOUT_H);
  s16Buf[1] =(u8Buf[1]<<8)|u8Buf[0];

  u8Buf[2]=MPU9250_I2C_ReadOneByte(MAG_YOUT_L);
  u8Buf[3]=MPU9250_I2C_ReadOneByte(MAG_YOUT_H);
  s16Buf[0] = (u8Buf[3]<<8)|u8Buf[2];
  
  u8Buf[4]=MPU9250_I2C_ReadOneByte(MAG_ZOUT_L);
  u8Buf[5]=MPU9250_I2C_ReadOneByte(MAG_ZOUT_H);
  s16Buf[2] = (u8Buf[5]<<8)|u8Buf[4]; 
  s16Buf[2] = -s16Buf[2];
  
  for(i = 0; i < 3; i ++)  
  {
    MPU9250_CalAvgValue(&sstAvgBuf[i].u8Index, sstAvgBuf[i].s16AvgBuffer, s16Buf[i], s32OutBuf + i);
  } 
    *ps16X = s32OutBuf[0] - MPU9250_Magn_Offset.X_Off_Err;
    *ps16Y = s32OutBuf[1] - MPU9250_Magn_Offset.Y_Off_Err;
    *ps16Z = s32OutBuf[2] - MPU9250_Magn_Offset.Z_Off_Err;
    // *ps16X = s32OutBuf[0] ;
    // *ps16Y = s32OutBuf[1] ;
    // *ps16Z = s32OutBuf[2] ;
}

/**
  * @brief  Check MPU9250,ensure communication succeed
  * @param  None
  * @retval true: communicate succeed
  *               false: communicate fualt 
  */
bool MPU9250_Check(void) 
{   
    bool bRet = false;
    if(WHO_AM_I_VAL == MPU9250_I2C_ReadOneByte(WHO_AM_I))
    {
      bRet = true;
    }
    else 
    {
      bRet = false;
    }

    return bRet;
}

/**
  * @brief  Digital filter
  * @param *pIndex:
  * @param *pAvgBuffer:
  * @param InVal:
  * @param pOutVal:
  *
  * @retval None
  *               
  */
void MPU9250_CalAvgValue(uint8_t *pIndex, int16_t *pAvgBuffer, int16_t InVal, int32_t *pOutVal)
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
/**
  * @brief  Initializes gyroscopes offset
  * @param  None
  * @retval None
  */
void MPU9250_InitGyrOffset(void)
{
  uint8_t i;
  int16_t s16Gx = 0, s16Gy = 0, s16Gz = 0;
  int32_t TempGx = 0, TempGy = 0, TempGz = 0;
  
  for(i = 0; i < 32; i ++)
  {
    MPU9250_READ_GYRO(&s16Gx, &s16Gy, &s16Gz);
    
    TempGx += s16Gx;
    TempGy += s16Gy;
    TempGz += s16Gz;

    DEV_Delay_ms(1);
  }

  MPU9250_Offset.s16X = TempGx >> 5;
  MPU9250_Offset.s16Y = TempGy >> 5;
  MPU9250_Offset.s16Z = TempGz >> 5;

}

void MPU9250_calibrateMagn(void)
{
  /*
  int16_t s16Gx = 0, s16Gy = 0, s16Gz = 0;
  int16_t temp[9];
  printf("keep 10dof-imu device horizontal and it will read x y z axis offset value after 4 seconds\n");
  DEV_Delay_ms(4000);
  printf("start read all axises offset value\n");
  MPU9250_READ_MAG(&s16Gx, &s16Gy, &s16Gz);
  temp[0] = s16Gx;
  temp[1] = s16Gy;
  temp[2] = s16Gz;
  
  printf("rotate z axis 180 degrees and it will read all axises offset value after 4 seconds\n");
  DEV_Delay_ms(4000);
  printf("start read all axises offset value\n");
  MPU9250_READ_MAG(&s16Gx, &s16Gy, &s16Gz);
  temp[3] = s16Gx;
  temp[4] = s16Gy;
  temp[5] = s16Gz;

  printf("flip 10dof-imu device and keep it horizontal and it will read all axises offset value after 4 seconds\n");
  DEV_Delay_ms(4000);
  printf("start read all axises offset value\n");
  MPU9250_READ_MAG(&s16Gx, &s16Gy, &s16Gz);
  temp[6] = s16Gx;
  temp[7] = s16Gy;
  temp[8] = s16Gz;

  MPU9250_Magn_Offset.X_Off_Err = (temp[0]+temp[3])/2;
  MPU9250_Magn_Offset.Y_Off_Err = (temp[1]+temp[4])/2;
  MPU9250_Magn_Offset.Z_Off_Err = (temp[5]+temp[8])/2;
  */
  //Comment the following three lines, remove the above function,
  //then enter the calibration mode, the default is the factory calibration value
  MPU9250_Magn_Offset.X_Off_Err = 0;
  MPU9250_Magn_Offset.Y_Off_Err = 6;
  MPU9250_Magn_Offset.Z_Off_Err = -8;
//  printf("X Y Z :%d %d %d\r\n",MPU9250_Magn_Offset.X_Off_Err,MPU9250_Magn_Offset.Y_Off_Err,MPU9250_Magn_Offset.Z_Off_Err); 
}

#ifdef __cplusplus
}
#endif