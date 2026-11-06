DIR_Config   = ./lib/Config
DIR_OLED     = ./lib/OLED
DIR_Examples = ./examples
DIR_BIN      = ./bin

OBJ_C = $(wildcard ${DIR_OLED}/*.c ${DIR_Config}/*.c ${DIR_Examples}/*.c)
OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))

TARGET = main

CC = gcc
MSG = -g -O0 -Wall
CFLAGS += $(MSG)

$(shell mkdir -p ${DIR_BIN})

${TARGET}:${OBJ_O}
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ -lm

${DIR_BIN}/%.o:$(DIR_Examples)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I $(DIR_Config) -I $(DIR_OLED)

${DIR_BIN}/%.o:$(DIR_OLED)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I $(DIR_Config)

${DIR_BIN}/%.o:$(DIR_Config)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f $(DIR_BIN)/*.o $(TARGET)