/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V3.0
* | Date        :   2026-09-24
* | Info        :   I2C 改为直接操作 /dev/i2c-N（i2c-dev + I2C_RDWR）
*
******************************************************************************/
#include "DEV_Config.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

/* 用 BME280 当探针：地址 0x76，读 0xD0 应得到 0x60 */
#define PROBE_ADDR    0x76
#define PROBE_ID_REG  0xD0
#define PROBE_ID_VAL  0x60

#define I2C_BUS_MAX   16

static int     I2C_Handle       = -1;   /* 打开的总线文件描述符 */
static uint8_t I2C_SlaveAddress = 0;    /* 当前从机地址 */

/******************************************************************************
function:	一次 I2C 传输（可只写、只读、或先写后读）
parameter:	wbuf/wlen : 写阶段数据（wlen 为 0 表示不发写时序）
            rbuf/rlen : 读阶段数据（rlen 为 0 表示不读）
Info:		返回 0 成功，-1 失败
******************************************************************************/
static int i2c_transfer(uint8_t *wbuf, int wlen, uint8_t *rbuf, int rlen)
{
	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data data;
	int n = 0;

	if (I2C_Handle < 0)
		return -1;

	if (wlen > 0) {
		msgs[n].addr  = I2C_SlaveAddress;
		msgs[n].flags = 0;
		msgs[n].len   = wlen;
		msgs[n].buf   = wbuf;
		n++;
	}
	if (rlen > 0) {
		msgs[n].addr  = I2C_SlaveAddress;
		msgs[n].flags = I2C_M_RD;
		msgs[n].len   = rlen;
		msgs[n].buf   = rbuf;
		n++;
	}
	if (n == 0)
		return -1;

	data.msgs  = msgs;
	data.nmsgs = n;

	if (ioctl(I2C_Handle, I2C_RDWR, &data) < 0)
		return -1;

	return 0;
}

/******************************************************************************
function:	在指定总线上探测是否存在本 HAT（BME280 0x76 / 0xD0 == 0x60）
parameter:	bus : 总线编号，对应 /dev/i2c-bus
Info:		返回 1 表示命中
******************************************************************************/
static int i2c_probe_bus(int bus)
{
	char path[32];
	int fd, ret = 0;
	uint8_t reg = PROBE_ID_REG, val = 0;
	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data data;

	snprintf(path, sizeof(path), "/dev/i2c-%d", bus);
	fd = open(path, O_RDWR);
	if (fd < 0)
		return 0;

	msgs[0].addr  = PROBE_ADDR;
	msgs[0].flags = 0;
	msgs[0].len   = 1;
	msgs[0].buf   = &reg;
	msgs[1].addr  = PROBE_ADDR;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len   = 1;
	msgs[1].buf   = &val;
	data.msgs  = msgs;
	data.nmsgs = 2;

	if (ioctl(fd, I2C_RDWR, &data) == 2 && val == PROBE_ID_VAL)
		ret = 1;

	close(fd);
	return ret;
}

/******************************************************************************
function:	探测本 HAT 挂在哪条 I2C 总线上
parameter:
Info:		优先使用环境变量 HAT_I2C_BUS，否则 0~15 逐个扫描
******************************************************************************/
static int i2c_detect_bus(void)
{
	const char *env = getenv("HAT_I2C_BUS");
	int bus;

	if (env != NULL && *env != '\0') {
		bus = (int)strtol(env, NULL, 0);
		printf("使用环境变量 HAT_I2C_BUS 指定的总线: /dev/i2c-%d\r\n", bus);
		return bus;
	}

	for (bus = 0; bus < I2C_BUS_MAX; bus++) {
		if (i2c_probe_bus(bus)) {
			printf("自动识别到 Environment Sensor HAT 在 /dev/i2c-%d\r\n", bus);
			return bus;
		}
	}

	printf("没有找到 Environment Sensor HAT：请确认 40Pin 排针插好，"
	       "或用 HAT_I2C_BUS=n 手动指定 I2C 总线\r\n");
	return -1;
}

/**
 * delay x ms
**/
void DEV_Delay_ms(UDOUBLE xms)
{
	usleep((useconds_t)xms * 1000);
}

/******************************************************************************
function:	I2C 初始化 / 切换从机地址
parameter:
Info:
******************************************************************************/
void DEV_I2C_Init(uint8_t Add)
{
	I2C_SlaveAddress = Add;
}

void DEV_I2C_Set_SlaveAddress(uint8_t Add)
{
	I2C_SlaveAddress = Add;
}

void I2C_Write_Byte(uint8_t Cmd, uint8_t value)
{
	uint8_t wbuf[2] = { Cmd, value };
	i2c_transfer(wbuf, 2, NULL, 0);
}

void I2C_Write_Word(uint8_t Cmd, uint16_t value)
{
	uint8_t wbuf[3] = { Cmd, (uint8_t)(value >> 8), (uint8_t)(value & 0xff) };
	i2c_transfer(wbuf, 3, NULL, 0);
}

void I2C_Write_NByte(uint8_t *Cmd, uint16_t num)
{
	i2c_transfer(Cmd, num, NULL, 0);
}

int I2C_Read_Byte(uint8_t Cmd)
{
	uint8_t val = 0;

	if (i2c_transfer(&Cmd, 1, &val, 1) < 0)
		return -1;

	return val;
}

int I2C_Read_Word(uint8_t Cmd)
{
	uint8_t rbuf[2] = { 0, 0 };

	if (i2c_transfer(&Cmd, 1, rbuf, 2) < 0)
		return -1;

	/* 低字节在前，与 TSL2591 等芯片的寄存器顺序一致 */
	return (rbuf[1] << 8) | rbuf[0];
}

void I2C_Read_NByte(uint8_t Cmd, uint8_t *rbuf, uint8_t num)
{
	i2c_transfer(&Cmd, 1, rbuf, num);
}

void I2C_Read_Raw(uint8_t *rbuf, uint8_t num)
{
	i2c_transfer(NULL, 0, rbuf, num);
}

/******************************************************************************
function:	Module Initialize, 打开 I2C 总线
parameter:
Info:
******************************************************************************/
uint8_t DEV_ModuleInit(void)
{
	int bus = i2c_detect_bus();
	char path[32];

	if (bus < 0)
		return 1;

	snprintf(path, sizeof(path), "/dev/i2c-%d", bus);
	I2C_Handle = open(path, O_RDWR);
	if (I2C_Handle < 0) {
		printf("打开 %s 失败: %s\r\n", path, strerror(errno));
		return 1;
	}

	I2C_SlaveAddress = 0;
	return 0;
}

/******************************************************************************
function:	Module exits, 关闭 I2C 总线
parameter:
Info:
******************************************************************************/
void DEV_ModuleExit(void)
{
	if (I2C_Handle >= 0) {
		close(I2C_Handle);
		I2C_Handle = -1;
	}
}