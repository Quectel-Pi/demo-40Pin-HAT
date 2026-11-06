#include "main.h"

void  Handler(int signo)
{
    //System Exit
    printf("\r\nHandler:exit\r\n");
    DEV_ModuleExit();

    exit(0);
}

IMU_ST_ANGLES_DATA stAngles;
IMU_ST_SENSOR_DATA stGyroRawData;
IMU_ST_SENSOR_DATA stAccelRawData;
IMU_ST_SENSOR_DATA stMagnRawData;

int main(void)
{
    // Exception handling:ctrl + c
    signal(SIGINT, Handler);
    if (DEV_ModuleInit() != 0)
        return 1;
    uint8_t light;
    BME280_Init();
    TSL2591_Init();
    LTR390_init();
    SGP40_init();
    imuInit();
    while(1){      
        BME280_value();
        light = TSL2591_Read_Lux();
        UVS_value();
        SGP40_value();      
        imuDataGet( &stAngles, &stGyroRawData, &stAccelRawData, &stMagnRawData);
        printf("\r\n /-------------------------------------------------------------/ \r\n");
        printf("\r\n pressure : %7.2fhPa\r\n",pres_raw[0]);
	    printf("\r\n temp :%7.2f℃\r\n",pres_raw[1]);
	    printf("\r\n hum :%7.2f％\r\n",pres_raw[2]);
        printf("\r\n Lux = %d\r\n",light);
        printf("\r\n UVS: %d\r\n" ,uv);
        printf("\r\n gas : %d\r\n",gas) ;
		printf("\r\n Roll: %.2f Pitch: %.2f Yaw: %.2f \r\n",stAngles.fRoll, stAngles.fPitch, stAngles.fYaw);
		printf("\r\n Acceleration: X: %d Y: %d Z: %d \r\n",stAccelRawData.s16X, stAccelRawData.s16Y, stAccelRawData.s16Z);
		printf("\r\n Gyroscope: X: %d Y: %d Z: %d \r\n",stGyroRawData.s16X, stGyroRawData.s16Y, stGyroRawData.s16Z);
		printf("\r\n Magnetic: X: %d Y: %d Z: %d \r\n",stMagnRawData.s16X, stMagnRawData.s16Y, stMagnRawData.s16Z);
        // DEV_Delay_ms(500);
    }
	//System Exit
	DEV_ModuleExit();
	return 0;
	
}

