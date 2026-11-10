/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V2.0
* | Date        :   2026-11-10
* | Info        :   Linux spidev + i2c-dev + sysfs GPIO, no third-party library
* |                 板型参数由一键脚本经环境变量下发，源码只保留 M2 的兜底值
*
******************************************************************************/
#include "DEV_Config.h"

#include <stdlib.h>
#include <fcntl.h>
#include <glob.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>

/* M2（RK3576）兜底值：Pin15/Pin13/Pin12 与 Pin7/Pin11/Pin16/Pin18 */
int LCD_DC  = 71;
int LCD_RST = 70;
int LCD_BL  = 87;
int KEY_1   = 79;
int KEY_2   = 78;
int KEY_3   = 132;
int KEY_4   = 134;
int LCD_CS  = -1;      /* Pin24 = SPI1_CE0，硬件片选，不做软件控制 */

static int spi_fd   = -1;
static int spi_speed = 8000000;
static int spi_mode  = 0;
static char spi_node[64] = "";

/* 本 HAT 有两块 0.96 OLED（0x3C / 0x3D），命令与显存数据要同时写两块 */
#define I2C_MAX 4

static int i2c_fd[I2C_MAX];
static int i2c_addr[I2C_MAX];
static int i2c_cnt = 0;
static char i2c_node[64] = "";

/*****************************************
                GPIO (sysfs)
*****************************************/
#define GPIO_MAX 8

static int gpio_pin[GPIO_MAX];
static int gpio_fd[GPIO_MAX];
static int gpio_out[GPIO_MAX];
static int gpio_cnt = 0;

static int DEV_GPIO_Slot(int pin)
{
    int i;

    for (i = 0; i < gpio_cnt; i++)
        if (gpio_pin[i] == pin)
            return i;
    return -1;
}

static void DEV_GPIO_Export(int pin)
{
    char path[64], buf[16];
    int fd, len;

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", pin);
    if (access(path, F_OK) == 0)
        return;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        printf("GPIO%d export failed: %s\r\n", pin, strerror(errno));
        return;
    }
    len = snprintf(buf, sizeof(buf), "%d", pin);
    if (write(fd, buf, len) != len) {
        /* 已被别人导出时写 export 会返回 EBUSY，这里不算错 */
    }
    close(fd);
}

void DEV_GPIO_Mode(int Pin, int Mode)
{
    char path[64];
    int fd, idx;

    if (Pin < 0)
        return;

    DEV_GPIO_Export(Pin);

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", Pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        printf("GPIO%d is not available: %s\r\n", Pin, strerror(errno));
        return;
    }
    if (write(fd, Mode ? "out" : "in", Mode ? 3 : 2) < 0)
        printf("GPIO%d set direction failed: %s\r\n", Pin, strerror(errno));
    close(fd);

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", Pin);
    fd = open(path, Mode ? O_WRONLY : O_RDONLY);
    if (fd < 0) {
        printf("GPIO%d open value failed: %s\r\n", Pin, strerror(errno));
        return;
    }

    idx = DEV_GPIO_Slot(Pin);
    if (idx < 0) {
        if (gpio_cnt >= GPIO_MAX) {
            close(fd);
            return;
        }
        idx = gpio_cnt++;
        gpio_pin[idx] = Pin;
    } else if (gpio_fd[idx] >= 0) {
        close(gpio_fd[idx]);
    }
    gpio_fd[idx]  = fd;
    gpio_out[idx] = Mode ? 1 : 0;
}

void DEV_Digital_Write(int Pin, UBYTE Value)
{
    int idx;

    if (Pin < 0)
        return;
    idx = DEV_GPIO_Slot(Pin);
    if (idx < 0 || gpio_fd[idx] < 0 || !gpio_out[idx])
        return;

    if (write(gpio_fd[idx], Value ? "1" : "0", 1) != 1)
        printf("GPIO%d write failed: %s\r\n", Pin, strerror(errno));
}

UBYTE DEV_Digital_Read(int Pin)
{
    char buf[2] = {0};
    int idx;

    if (Pin < 0)
        return 0;
    idx = DEV_GPIO_Slot(Pin);
    if (idx < 0 || gpio_fd[idx] < 0 || gpio_out[idx])
        return 0;

    if (pread(gpio_fd[idx], buf, 1, 0) != 1)
        return 0;
    return (buf[0] == '1') ? 1 : 0;
}

/* M2 的 Pin12 不是 PWM 通道，这里只做开/关背光 */
void DEV_SetBacklight(UWORD Value)
{
    DEV_Digital_Write(LCD_BL, Value ? 1 : 0);
}

/**
 * delay x ms
**/
void DEV_Delay_ms(UDOUBLE xms)
{
    struct timespec ts;

    ts.tv_sec  = xms / 1000;
    ts.tv_nsec = (xms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/*****************************************
                SPI (spidev)
*****************************************/
static int DEV_SPI_MatchSuffix(glob_t *g, const char *suffix, char *out, size_t len)
{
    size_t i, sn = strlen(suffix);

    for (i = 0; i < g->gl_pathc; i++) {
        size_t n = strlen(g->gl_pathv[i]);
        if (n >= sn && strcmp(g->gl_pathv[i] + n - sn, suffix) == 0) {
            snprintf(out, len, "%s", g->gl_pathv[i]);
            return 0;
        }
    }
    return -1;
}

/* 优先取 CE0（HAT 的 CS 接 Pin24），没有则退回 CE1，再没有就取第一个 */
static int DEV_SPI_FindDevice(char *out, size_t len)
{
    glob_t g;
    int ret = -1;

    if (glob("/dev/spidev*.*", 0, NULL, &g) != 0)
        return -1;

    ret = DEV_SPI_MatchSuffix(&g, ".0", out, len);
    if (ret != 0)
        ret = DEV_SPI_MatchSuffix(&g, ".1", out, len);
    if (ret != 0 && g.gl_pathc > 0) {
        snprintf(out, len, "%s", g.gl_pathv[0]);
        ret = 0;
    }
    globfree(&g);
    return ret;
}

static int DEV_SPI_Open(const char *dev)
{
    uint8_t  mode  = (uint8_t)spi_mode;
    uint8_t  bits  = 8;
    uint32_t speed = (uint32_t)spi_speed;
    int fd = open(dev, O_RDWR);

    if (fd < 0)
        return -1;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        printf("%s: set mode%d/8bit/%dHz failed: %s\r\n",
               dev, spi_mode, spi_speed, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

void DEV_SPI_WriteByte(UBYTE Value)
{
    DEV_SPI_Write_nByte(&Value, 1);
}

void DEV_SPI_Write_nByte(uint8_t *pData, uint32_t Len)
{
    int i;

    if (spi_fd < 0)
        return;
    for (i = 0; i < 3; i++) {
        if (write(spi_fd, pData, Len) == (ssize_t)Len)
            return;
        if (errno != EIO)
            break;
        DEV_Delay_ms(1);
    }
    printf("SPI write failed: %s\r\n", strerror(errno));
}

/*****************************************
                I2C (i2c-dev)
*****************************************/
void I2C_Write_Byte(uint8_t value, uint8_t Cmd)
{
    uint8_t buf[2];
    static int warned[I2C_MAX];
    int i;

    buf[0] = Cmd;
    buf[1] = value;

    for (i = 0; i < i2c_cnt; i++) {
        if (i2c_fd[i] < 0)
            continue;
        if (write(i2c_fd[i], buf, 2) != 2 && !warned[i]) {
            printf("I2C 0x%02X write failed: %s (OLED no ACK? check the HAT and R1)\r\n",
                   i2c_addr[i], strerror(errno));
            warned[i] = 1;
        }
    }
}

/* 整块写显存：一次 [控制字节 + Len 个数据] 事务，避免逐字节事务把整屏刷新拖到几百毫秒 */
void I2C_Write_nByte(uint8_t *pData, uint32_t Len, uint8_t Cmd)
{
    static uint8_t buf[1025];
    static int warned[I2C_MAX];
    int i;

    if (Len > sizeof(buf) - 1)
        Len = sizeof(buf) - 1;
    buf[0] = Cmd;
    memcpy(buf + 1, pData, Len);

    for (i = 0; i < i2c_cnt; i++) {
        if (i2c_fd[i] < 0)
            continue;
        if (write(i2c_fd[i], buf, Len + 1) != (ssize_t)(Len + 1) && !warned[i]) {
            printf("I2C 0x%02X bulk write failed: %s\r\n",
                   i2c_addr[i], strerror(errno));
            warned[i] = 1;
        }
    }
}

/*****************************************
            运行期参数（环境变量）
*****************************************/
static int DEV_EnvInt(const char *name, int def)
{
    const char *env = getenv(name);

    return (env && *env) ? atoi(env) : def;
}

/* OLED_I2C_ADDR 支持逗号分隔的地址列表，默认本 HAT 的两块屏 0x3C,0x3D */
static void DEV_ParseI2CAddr(const char *list)
{
    char buf[64];
    char *p, *tok;

    i2c_cnt = 0;
    snprintf(buf, sizeof(buf), "%s", list);
    for (p = buf; p && *p && i2c_cnt < I2C_MAX; p = tok) {
        tok = strchr(p, ',');
        if (tok)
            *tok++ = '\0';
        i2c_addr[i2c_cnt++] = (int)strtol(p, NULL, 0);
    }
}

static void DEV_ResolveConfig(void)
{
    LCD_DC  = DEV_EnvInt("LCD_DC_GPIO",  LCD_DC);
    LCD_RST = DEV_EnvInt("LCD_RST_GPIO", LCD_RST);
    LCD_BL  = DEV_EnvInt("LCD_BL_GPIO",  LCD_BL);
    KEY_1   = DEV_EnvInt("KEY1_GPIO",    KEY_1);
    KEY_2   = DEV_EnvInt("KEY2_GPIO",    KEY_2);
    KEY_3   = DEV_EnvInt("KEY3_GPIO",    KEY_3);
    KEY_4   = DEV_EnvInt("KEY4_GPIO",    KEY_4);

    spi_mode  = DEV_EnvInt("LCD_SPI_MODE",  spi_mode);
    spi_speed = DEV_EnvInt("LCD_SPI_SPEED", spi_speed);
}

/******************************************************************************
function:	Module Initialize, the library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
UBYTE DEV_ModuleInit(void)
{
    const char *env;
    int i;

    DEV_ResolveConfig();

    DEV_GPIO_Mode(LCD_RST, 1);
    DEV_GPIO_Mode(LCD_DC, 1);
    DEV_GPIO_Mode(LCD_BL, 1);
    DEV_GPIO_Mode(KEY_1, 0);
    DEV_GPIO_Mode(KEY_2, 0);
    DEV_GPIO_Mode(KEY_3, 0);
    DEV_GPIO_Mode(KEY_4, 0);

    LCD_RST_1;
    LCD_DC_0;
    LCD_BL_1;
    printf("GPIO: DC=%d RST=%d BL=%d KEY=%d/%d/%d/%d (sysfs)\r\n",
           LCD_DC, LCD_RST, LCD_BL, KEY_1, KEY_2, KEY_3, KEY_4);

    env = getenv("LCD_SPI_DEV");
    if (env && *env) {
        snprintf(spi_node, sizeof(spi_node), "%s", env);
    } else if (DEV_SPI_FindDevice(spi_node, sizeof(spi_node)) != 0) {
        printf("No /dev/spidevX.Y found. Check the 40Pin SPI is enabled\r\n");
        printf("(Quectel Pi M2: sudo qpi-config spi1 enable, then reboot)\r\n");
        return 1;
    }

    spi_fd = DEV_SPI_Open(spi_node);
    if (spi_fd < 0) {
        printf("Open %s failed: %s\r\n", spi_node, strerror(errno));
        return 1;
    }
    printf("SPI: %s mode%d 8bit %dHz\r\n", spi_node, spi_mode, spi_speed);

    env = getenv("OLED_I2C_DEV");
    snprintf(i2c_node, sizeof(i2c_node), "%s", (env && *env) ? env : "/dev/i2c-1");

    env = getenv("OLED_I2C_ADDR");
    DEV_ParseI2CAddr((env && *env) ? env : "0x3C,0x3D");

    printf("I2C: %s addr", i2c_node);
    for (i = 0; i < i2c_cnt; i++) {
        int fd = open(i2c_node, O_RDWR);

        i2c_fd[i] = -1;
        if (fd < 0 || ioctl(fd, I2C_SLAVE, i2c_addr[i]) < 0) {
            printf(" 0x%02X(x)", i2c_addr[i]);
            if (fd >= 0)
                close(fd);
        } else {
            i2c_fd[i] = fd;
            printf(" 0x%02X", i2c_addr[i]);
        }
    }
    printf("\r\n");
    return 0;
}

/******************************************************************************
function:	Module exits, closes SPI/I2C/GPIO
parameter:
Info:		RST 保持高、DC 保持低、背光保持点亮，退出后屏幕继续显示。
			sysfs 引脚不 unexport（需释放时手动 echo <pin> > /sys/class/gpio/unexport）。
******************************************************************************/
void DEV_ModuleExit(void)
{
    int i;

    LCD_RST_1;
    LCD_DC_0;

    if (spi_fd >= 0) { close(spi_fd); spi_fd = -1; }

    for (i = 0; i < i2c_cnt; i++) {
        if (i2c_fd[i] >= 0) {
            close(i2c_fd[i]);
            i2c_fd[i] = -1;
        }
    }

    for (i = 0; i < gpio_cnt; i++) {
        if (gpio_fd[i] >= 0) {
            close(gpio_fd[i]);
            gpio_fd[i] = -1;
        }
    }
}
