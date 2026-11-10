DIR_Config   = ./lib/Config
DIR_OLED     = ./lib/OLED
DIR_LCD      = ./lib/LCD
DIR_FONTS    = ./lib/Fonts
DIR_GUI      = ./lib/GUI
DIR_Examples = ./examples
DIR_BIN      = ./bin

OBJ_C = $(wildcard ${DIR_OLED}/*.c ${DIR_LCD}/*.c ${DIR_Config}/*.c ${DIR_GUI}/*.c ${DIR_Examples}/*.c ${DIR_FONTS}/*.c)
OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))

TARGET = main

CC = gcc
MSG = -g -O0 -Wall
CFLAGS += $(MSG)

$(shell mkdir -p ${DIR_BIN})

${TARGET}:${OBJ_O}
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ -lm -lpthread

${DIR_BIN}/%.o:$(DIR_Examples)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I $(DIR_Config) -I $(DIR_GUI) -I $(DIR_OLED) -I $(DIR_LCD)

${DIR_BIN}/%.o:$(DIR_OLED)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I $(DIR_Config)

${DIR_BIN}/%.o:$(DIR_LCD)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I $(DIR_Config)

${DIR_BIN}/%.o:$(DIR_FONTS)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

${DIR_BIN}/%.o:$(DIR_GUI)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I $(DIR_Config) -I $(DIR_OLED) -I $(DIR_LCD) -I $(DIR_Examples)

${DIR_BIN}/%.o:$(DIR_Config)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	rm -f $(DIR_BIN)/*.o $(TARGET)
