DIR_OBJ = ./lib
DIR_BIN = ./bin
DIR_Config = ./lib/Config
DIR_TSL2591 = ./lib/TSL2591
DIR_SGP40 = ./lib/SGP40
DIR_BME280 = ./lib/BME280
DIR_LTR390 = ./lib/LTR390
DIR_IMU = ./lib/IMU
DIR_Examples = ./examples

OBJ_C = $(wildcard ${DIR_OBJ}/*.c ${DIR_Examples}/*.c ${DIR_Config}/*.c ${DIR_TSL2591}/*.c ${DIR_SGP40}/*.c ${DIR_BME280}/*.c ${DIR_LTR390}/*.c ${DIR_IMU}/*.c)
OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))

TARGET = main

CC = gcc

CFLAGS = -g -O0 -Wall
INCLUDES = -I $(DIR_OBJ) -I $(DIR_Config) -I $(DIR_TSL2591) -I $(DIR_SGP40) -I $(DIR_BME280) -I $(DIR_LTR390) -I $(DIR_IMU)
# 只依赖 libm；I2C 直接走 /dev/i2c-N
LIB = -lm

$(shell mkdir -p $(DIR_BIN))

${TARGET}:${OBJ_O}
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIB)

${DIR_BIN}/%.o : $(DIR_Examples)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

${DIR_BIN}/%.o : $(DIR_Config)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

${DIR_BIN}/%.o : $(DIR_TSL2591)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

${DIR_BIN}/%.o : $(DIR_SGP40)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

${DIR_BIN}/%.o : $(DIR_BME280)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

${DIR_BIN}/%.o : $(DIR_LTR390)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

${DIR_BIN}/%.o : $(DIR_IMU)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean :
	rm -f $(DIR_BIN)/*.o
	rm -f $(TARGET)