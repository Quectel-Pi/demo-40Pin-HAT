/**
  ******************************************************************************
  * @file    BME280.c
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
#include "BME280.h"
  
#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 * interface driver                                                           *
 ******************************************************************************/
 //Raspberry 3B+ platform's default I2C device file
uint16_t BME280_ReadByte(uint8_t RegAddr)
{	
	return I2C_Read_Byte(RegAddr);
}

void BME280_WriteByte(uint8_t RegAddr, uint8_t value)
{

  I2C_Write_Byte(RegAddr, value);
  return;
}

void BME280_Write_NByte(uint8_t RegAddr, uint8_t value)
{
  I2C_Write_Byte(RegAddr, value);
  return;
}
/***********  BME280_TEST  ****************/

int32_t digT[3],digP[9],digH[6];
int32_t t_fine = 0.0;
double pres_raw[3];
void get_calib_param()
{
	uint8_t calib[32];
	for(int i=0;i<24;i++)
	{
		calib[i] = BME280_ReadByte(0x88 + i);
	}
	calib[24] = BME280_ReadByte(0xA1);
	for(int i=25,o=0;i<32;i++,o++)
	{
		calib[i] = BME280_ReadByte(0xE1 + o);
	}

	digT[0] = (calib[1] << 8) | calib[0];
	digT[1] = (calib[3] << 8) | calib[2];
	digT[2] = (calib[5] << 8) | calib[4];

	digP[0] = (calib[7] << 8) | calib[6];
	digP[1] = (calib[9] << 8) | calib[8];
	digP[2] = (calib[11] << 8) | calib[10];
	digP[3] = (calib[13] << 8) | calib[12];
	digP[4] = (calib[15] << 8) | calib[14];
	digP[5] = (calib[17] << 8) | calib[16];
	digP[6] = (calib[19] << 8) | calib[18];
	digP[7] = (calib[21] << 8) | calib[20];
	digP[8] = (calib[23] << 8) | calib[22];

	digH[0] = calib[24];
	digH[1] = (calib[26] << 8) | calib[25];
	digH[2] = calib[27];
	digH[3] = (calib[28] << 4) | (0x0f & calib[29]);
	digH[4] = (calib[30] << 4) | ((calib[29] >> 4) & 0x0f);
	digH[5] = calib[31];
	
	for(int i=1;i<2;i++)
			if((digT[i] & 0x8000) != 0)
				digT[i] = (-digT[i] ^ 0xFFFF) + 1;

	for(int i=1;i<8;i++)		
			if ((digP[i] & 0x8000) != 0)					
					digP[i]=(-digP[i] ^ 0xFFFF) + 1	;			

	for(int i=0;i<6;i++)			
			if ((digH[i] & 0x8000) != 0)
				digH[i] = (-digH[i] ^ 0xFFFF) + 1;
	
		
}
void BME280_Init()
{
	// DEV_I2C_Init(BME280_ADDR);
	DEV_I2C_Set_SlaveAddress(BME280_ADDR);
	BME280_WriteByte(0xF2,ctrl_hum_reg); 
	BME280_WriteByte(0xF4,ctrl_meas_reg);
	BME280_WriteByte(0xF5,config_reg);
	get_calib_param();
}
double compensate_P(int32_t adc_P)
{
	double pressure = 0.0;
	double v1,v2;
	v1 = (t_fine / 2.0) - 64000.0;
	v2 = (((v1 / 4.0) * (v1 / 4.0)) / 2048) * digP[5];
	v2 = v2 + ((v1 * digP[4]) * 2.0);
	v2 = (v2 / 4.0) + (digP[3] * 65536.0);
	v1 = (((digP[2] * (((v1 / 4.0) * (v1 / 4.0)) / 8192)) / 8)  + ((digP[1] * v1) / 2.0)) / 262144;
	v1 = ((32768 + v1) * digP[0]) / 32768;
	if(v1 == 0)
		return 0;
	pressure = ((1048576 - adc_P) - (v2 / 4096)) * 3125;
	if (pressure < 0x80000000)
			pressure = (pressure * 2.0) / v1;
	else
			pressure = (pressure / v1) * 2;
	v1 = (digP[8] * (((pressure / 8.0) * (pressure / 8.0)) / 8192.0)) / 4096;
	v2 = ((pressure / 4.0) * digP[7]) / 8192.0;
	pressure = pressure + ((v1 + v2 + digP[6]) / 16.0) ; 
	//printf("%7.2f \n",pressure);
	return (pressure/100);
}

double compensate_T(int32_t adc_T)
{
	double temperature = 0.0;
	double v1,v2;
	v1 = (adc_T / 16384.0 - digT[0] / 1024.0) * digT[1];
	v2 = (adc_T / 131072.0 - digT[0] / 8192.0) * (adc_T / 131072.0 - digT[0] / 8192.0) * digT[2];
	t_fine = v1 + v2;
	temperature = t_fine / 5120.0;
	return temperature;
}

double compensate_H(int32_t adc_H)
{
	double var_h = t_fine - 76800.0;
	if (var_h != 0)
			var_h = (adc_H - (digH[3] * 64.0 + digH[4]/16384.0 * var_h)) * (digH[1] / 65536.0 * (1.0 + digH[5] / 67108864.0 * var_h * (1.0 + digH[2] / 67108864.0 * var_h)));
	else
		return 0;
	var_h = var_h * (1.0 - digH[0] * var_h / 524288.0);
	if (var_h > 100.0)
		var_h = 100.0;
	else if (var_h < 0.0)
		var_h = 0.0;
	return var_h;
}

void BME280_value()
{
	uint8_t data[8];
	DEV_I2C_Set_SlaveAddress(BME280_ADDR);
	for(int i=0;i<8;i++)
		data[i] = BME280_ReadByte(0xF7 + i);
	 
	pres_raw[0] = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
	pres_raw[1] = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
	pres_raw[2] = (data[6] << 8)  |  data[7];

	pres_raw[0] = compensate_P(pres_raw[0]);
	pres_raw[1] = compensate_T(pres_raw[1]);
	pres_raw[2] = compensate_H(pres_raw[2]);
	// printf("pressure : %7.2fhPa\n",compensate_P(pres_raw[0]));
	// printf("temp :%7.2f℃\n",compensate_T(pres_raw[1]));
	// printf("hum :%7.2f％\n",compensate_H(pres_raw[2]));
}

/************* END ***********************/
#ifdef __cplusplus
}
#endif
