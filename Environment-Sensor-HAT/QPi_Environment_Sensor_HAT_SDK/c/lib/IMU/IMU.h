#ifndef __IMU_H
#define __IMU_H

#include <stdint.h>
#include <math.h>

#include "ICM20948.h"
#include "MPU9250.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    IMU_EN_SENSOR_TYPE_NULL = 0,
    IMU_EN_SENSOR_TYPE_ICM20948,
    IMU_EN_SENSOR_TYPE_MPU9250,
    IMU_EN_SENSOR_TYPE_MAX
}IMU_EN_SENSOR_TYPE;

typedef struct imu_st_angles_data_tag
{
    float fYaw;
    float fPitch;
    float fRoll;
}IMU_ST_ANGLES_DATA;

typedef struct imu_st_sensor_data_tag
{
  int16_t s16X;
  int16_t s16Y;
  int16_t s16Z;
}IMU_ST_SENSOR_DATA;

void imuInit(void);
void imuDataGet(IMU_ST_ANGLES_DATA *pstAngles, 
        IMU_ST_SENSOR_DATA *pstGyroRawData,
        IMU_ST_SENSOR_DATA *pstAccelRawData,
        IMU_ST_SENSOR_DATA *pstMagnRawData); 
void imuAHRSupdate(float gx, float gy, float gz, float ax, float ay, float az, float mx, float my, float mz);
float invSqrt(float x);

#ifdef __cplusplus
}
#endif

#endif
