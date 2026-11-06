/*****************************************************************************
* | File      	:   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V3.2
* | Date        :   2026-10-13
* | Info        :   Linux spidev + GPIO (sysfs 或字符设备), no third-party library
* |                 板型预设移出源码：DC/RST、gpiochip、片选由一键脚本经环境变量下发
*
******************************************************************************/
#include "DEV_Config.h"

#include <glob.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/i2c-dev.h>
#include <linux/gpio.h>

int OLED_DC  = OLED_DC_DEFAULT;
int OLED_RST = OLED_RST_DEFAULT;

static int spi_fd = -1;
static int i2c_fd = -1;
static int dc_fd  = -1;
static int rst_fd = -1;
static char dev_node[64] = "";
static int spi_speed = 1000000;

/*****************************************
        GPIO (sysfs 或字符设备)
*****************************************/
/* 后端选择：有 /sys/class/gpio 就走 sysfs（旧行为不变），内核未开
   CONFIG_GPIO_SYSFS 的板（如 Quectel H1 / QCS6490）改用 GPIO 字符设备。
   chardev 后端用的 gpiochip label 关键字由一键脚本经 OLED_GPIO_CHIP 下发 */
static int use_chardev = -1;

static int DEV_GPIO_UseCharDev(void)
{
    if (use_chardev < 0)
        use_chardev = (access("/sys/class/gpio", F_OK) != 0);
    return use_chardev;
}

/* 找 label 含关键字的 /dev/gpiochipN，找不到时把现有芯片列出来 */
static int DEV_GPIO_FindChip(const char *label, char *out, size_t len)
{
    char dev[32], listing[256] = "";
    int i, fd;

    for (i = 0; i < 16; i++) {
        struct gpiochip_info info;

        snprintf(dev, sizeof(dev), "/dev/gpiochip%d", i);
        fd = open(dev, O_RDONLY);
        if (fd < 0)
            continue;
        memset(&info, 0, sizeof(info));
        if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) == 0) {
            if (strstr(info.label, label) != NULL) {
                snprintf(out, len, "%s", dev);
                close(fd);
                return 0;
            }
            snprintf(listing + strlen(listing), sizeof(listing) - strlen(listing),
                     "%s[%s] ", dev, info.label);
        }
        close(fd);
    }
    printf("No gpiochip with label \"%s\" (found: %s)\r\n", label, listing);
    return -1;
}

/* 字符设备：把 pin 请求为输出并返回句柄 fd，之后用 ioctl 写值 */
static int DEV_GPIO_OpenCharDev(int pin, int output)
{
    static char chip_dev[32] = "";
    struct gpiohandle_request req;
    const char *env, *label;
    int fd;

    if (chip_dev[0] == '\0') {
        /* gpiochip label 由一键脚本经环境变量 OLED_GPIO_CHIP 下发 */
        env = getenv("OLED_GPIO_CHIP");
        label = (env && *env) ? env : "";
        if (*label == '\0') {
            printf("GPIO%d: no /sys/class/gpio, set OLED_GPIO_CHIP=<gpiochip label>\r\n", pin);
            return -1;
        }
        if (DEV_GPIO_FindChip(label, chip_dev, sizeof(chip_dev)) != 0)
            return -1;
    }

    fd = open(chip_dev, O_RDONLY);
    if (fd < 0) {
        printf("GPIO%d open %s failed: %s\r\n", pin, chip_dev, strerror(errno));
        return -1;
    }
    memset(&req, 0, sizeof(req));
    req.lineoffsets[0]    = pin;
    req.default_values[0] = 0;
    req.lines             = 1;
    req.flags = output ? GPIOHANDLE_REQUEST_OUTPUT : GPIOHANDLE_REQUEST_INPUT;
    snprintf(req.consumer_label, sizeof(req.consumer_label), "oled-hat");
    if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req) != 0) {
        printf("GPIO%d request on %s failed: %s\r\n", pin, chip_dev, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return req.fd;
}

/* 导出引脚并打开它的 value 文件，之后写 "1"/"0" 即可翻转电平 */
static int DEV_GPIO_Open(int pin, int output)
{
    char path[64], buf[16];
    int fd, len;

    if (DEV_GPIO_UseCharDev())
        return DEV_GPIO_OpenCharDev(pin, output);

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", pin);
    if (access(path, F_OK) != 0) {
        fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd < 0) {
            printf("GPIO%d export failed: %s\r\n", pin, strerror(errno));
            return -1;
        }
        len = snprintf(buf, sizeof(buf), "%d", pin);
        if (write(fd, buf, len) != len) {
            /* 已被别人导出时写 export 会返回 EBUSY，这里不算错 */
        }
        close(fd);
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        printf("GPIO%d is not available: %s\r\n", pin, strerror(errno));
        return -1;
    }
    write(fd, output ? "out" : "in", output ? 3 : 2);
    close(fd);

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0)
        printf("GPIO%d open value failed: %s\r\n", pin, strerror(errno));
    return fd;
}

void DEV_GPIO_Mode(UWORD Pin, UWORD Mode)
{
    int fd = DEV_GPIO_Open(Pin, Mode ? 1 : 0);

    if (Pin == OLED_DC) {
        if (dc_fd >= 0) close(dc_fd);
        dc_fd = fd;
    } else if (Pin == OLED_RST) {
        if (rst_fd >= 0) close(rst_fd);
        rst_fd = fd;
    } else if (fd >= 0) {
        close(fd);
    }
}

void DEV_Digital_Write(UWORD Pin, UBYTE Value)
{
    int fd = (Pin == OLED_DC) ? dc_fd : (Pin == OLED_RST) ? rst_fd : -1;

    if (fd < 0)
        return;

    if (DEV_GPIO_UseCharDev()) {
        struct gpiohandle_data data;

        memset(&data, 0, sizeof(data));
        data.values[0] = Value ? 1 : 0;
        if (ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) != 0)
            printf("GPIO%d write failed: %s\r\n", Pin, strerror(errno));
    } else if (write(fd, Value ? "1" : "0", 1) != 1) {
        printf("GPIO%d write failed: %s\r\n", Pin, strerror(errno));
    }
}

UBYTE DEV_Digital_Read(UWORD Pin)
{
    char path[64], buf[4] = {0};
    int fd;

    if (DEV_GPIO_UseCharDev()) {
        struct gpiohandle_data data;

        fd = (Pin == OLED_DC) ? dc_fd : (Pin == OLED_RST) ? rst_fd : -1;
        if (fd < 0)
            return 0;
        memset(&data, 0, sizeof(data));
        if (ioctl(fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) != 0)
            return 0;
        return data.values[0] ? 1 : 0;
    }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", Pin);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    if (read(fd, buf, sizeof(buf) - 1) < 0)
        buf[0] = '0';
    close(fd);
    return (buf[0] == '1') ? 1 : 0;
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
/* 在 glob 结果里找以 suffix 结尾（如 ".0" / ".1"）的节点 */
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

/* 交互选择：CS0/CS1 都存在且标准输入是终端时，提示输入 0/1 */
static int DEV_SPI_AskCS(const char *node0, const char *node1, char *out, size_t len)
{
    char line[32];

    printf("CS0 = %s\r\n", node0);
    printf("CS1 = %s\r\n", node1);
    for (;;) {
        printf("选择片选 [0/1]（回车默认 CS0）: ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;                              /* EOF：用默认 */
        if (line[0] == '0' || line[0] == '\n' || line[0] == '\r')
            break;
        if (line[0] == '1') {
            snprintf(out, len, "%s", node1);
            return 0;
        }
        printf("请输入 0 或 1\r\n");
    }
    snprintf(out, len, "%s", node0);
    return 0;
}

/* 挑 SPI 设备节点。
   选择片选：OLED_SPI_CS=0|1 按片选号找 /dev/spidevX.<n>（如 H1 的 10.0=CE0 / 10.1=CE1）；
   终端里直接运行时，若 CS0/CS1 两个节点都存在，提示输入 0/1 选择（回车默认 CS0）；
   默认：优先 CS0（HAT 通常接 CE0），没有 CS0 时退回 CS1，再没有就取任意一个；
   指定完整节点：OLED_SPI_DEV=/dev/spidevX.Y（在 ModuleInit 里优先级最高）。 */
static int DEV_SPI_FindDevice(char *out, size_t len)
{
    static const char *default_prefer[] = { ".0", ".1" };
    const char *env = getenv("OLED_SPI_CS");
    char suffix[3] = { '.', '\0', '\0' };
    glob_t g;
    size_t k;
    int ret = -1;

    if (glob("/dev/spidev*.*", 0, NULL, &g) != 0)
        return -1;

    /* OLED_SPI_CS 指定了片选号：只认这个片选，找不到就报错 */
    if (env && *env >= '0' && *env <= '9') {
        suffix[1] = *env;
        if (DEV_SPI_MatchSuffix(&g, suffix, out, len) != 0)
            printf("OLED_SPI_CS=%s: no /dev/spidevX.%s found\r\n", env, suffix + 1);
        else
            ret = 0;
        globfree(&g);
        return ret;
    }

    /* 终端里运行且 CS0/CS1 都在：让用户输入 0/1 选片选 */
    if (isatty(STDIN_FILENO)) {
        char n0[64] = "", n1[64] = "";

        if (DEV_SPI_MatchSuffix(&g, ".0", n0, sizeof(n0)) == 0 &&
            DEV_SPI_MatchSuffix(&g, ".1", n1, sizeof(n1)) == 0) {
            globfree(&g);
            return DEV_SPI_AskCS(n0, n1, out, len);
        }
    }

    /* 默认顺序：CS0 -> CS1 -> 任意节点 */
    for (k = 0; k < sizeof(default_prefer) / sizeof(default_prefer[0]) && ret != 0; k++)
        ret = DEV_SPI_MatchSuffix(&g, default_prefer[k], out, len);

    if (ret != 0 && g.gl_pathc > 0) {
        snprintf(out, len, "%s", g.gl_pathv[0]);
        ret = 0;
    }
    globfree(&g);
    return ret;
}

static int DEV_SPI_Open(const char *dev)
{
    uint8_t  mode  = SPI_MODE_3;
    uint8_t  bits  = 8;
    uint32_t speed = spi_speed;
    int fd = open(dev, O_RDWR);

    if (fd < 0)
        return -1;
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
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
    /* Qualcomm GENI/GSI 控制器偶发首个传输返回 EIO，重试即可 */
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

    if (i2c_fd < 0)
        return;
    /* SSD1305: 第一个字节 0x00=命令 0x40=显存数据 */
    buf[0] = Cmd;
    buf[1] = value;
    if (write(i2c_fd, buf, 2) != 2)
        printf("I2C write failed: %s\r\n", strerror(errno));
}

/******************************************************************************
function:	解析 DC/RST 引脚
parameter:
Info:		优先级：环境变量 OLED_DC_GPIO / OLED_RST_GPIO > 编译期默认值。
			板型差异（各板的引脚号、gpiochip label）由 a.Waveshare 根目录的
			一键脚本 <板>_2.23inch-OLED-HAT.sh 以环境变量下发，源码不再内置板型预设。
******************************************************************************/
static void DEV_ResolvePins(void)
{
    const char *env;

    env = getenv("OLED_DC_GPIO");
    if (env && *env) OLED_DC = atoi(env);
    env = getenv("OLED_RST_GPIO");
    if (env && *env) OLED_RST = atoi(env);
}

/******************************************************************************
function:	Module Initialize, the library and initialize the pins, SPI protocol
parameter:
Info:
******************************************************************************/
UBYTE DEV_ModuleInit(void)
{
    const char *env;

    DEV_ResolvePins();

#if USE_SPI
    printf("USE_SPI\r\n");
    env = getenv("OLED_SPI_DEV");
    if (env && *env) {
        snprintf(dev_node, sizeof(dev_node), "%s", env);
    } else if (DEV_SPI_FindDevice(dev_node, sizeof(dev_node)) != 0) {
        printf("No /dev/spidevX.Y found. Check the 40Pin SPI is enabled\r\n");
        printf("(Quectel Pi M2: sudo qpi-config spi1 enable, then reboot)\r\n");
        return 1;
    }

    spi_fd = DEV_SPI_Open(dev_node);
    if (spi_fd < 0) {
        printf("Open %s failed: %s\r\n", dev_node, strerror(errno));
        return 1;
    }
    printf("SPI: %s  mode3  8bit  %dHz\r\n", dev_node, spi_speed);

    printf("GPIO: DC=%d  RST=%d  (%s)\r\n", OLED_DC, OLED_RST,
           DEV_GPIO_UseCharDev() ? "chardev" : "sysfs");
    DEV_GPIO_Mode(OLED_RST, 1);
    DEV_GPIO_Mode(OLED_DC, 1);
#elif USE_IIC
    env = getenv("OLED_I2C_DEV");
    snprintf(dev_node, sizeof(dev_node), "%s", (env && *env) ? env : "/dev/i2c-1");
    printf("USE_IIC\r\n");
    i2c_fd = open(dev_node, O_RDWR);
    if (i2c_fd < 0) {
        printf("Open %s failed: %s\r\n", dev_node, strerror(errno));
        return 1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, 0x3c) < 0) {
        printf("Set I2C slave 0x%02X failed: %s\r\n", 0x3c, strerror(errno));
        return 1;
    }
    DEV_GPIO_Mode(OLED_RST, 1);
    DEV_GPIO_Mode(OLED_DC, 1);
    OLED_DC_0;
#endif
    return 0;
}

/******************************************************************************
function:	Module exits, closes SPI and GPIO
parameter:
Info:		RST 保持高、DC 保持低，退出后屏幕继续显示。
			sysfs 后端不 unexport（需释放时手动 echo <pin> > /sys/class/gpio/unexport）；
			chardev 后端释放句柄后引脚同样保持电平（已实测）。
******************************************************************************/
void DEV_ModuleExit(void)
{
    OLED_DC_0;
    OLED_RST_1;

    if (spi_fd >= 0) { close(spi_fd); spi_fd = -1; }
    if (i2c_fd >= 0) { close(i2c_fd); i2c_fd = -1; }
    if (dc_fd  >= 0) { close(dc_fd);  dc_fd  = -1; }
    if (rst_fd >= 0) { close(rst_fd); rst_fd = -1; }
}