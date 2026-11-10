/*****************************************************************************
* | File        :   KEY_test.c
* | Author      :   Waveshare team
* | Function    :   key test demo
* | Info        :
*----------------
* | This version:   V1.0
* | Date        :   2023-10-10
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "test.h"
#include "OLED_0in96.h"
#include "LCD_2inch.h"


int key_test(void)
{
    printf("OLED LCD test demo\n");
    if (DEV_ModuleInit() != 0)
    {
        return -1;
    }

    printf("OLED LCD Init...\r\n");
    OLED_0in96_Init();
    OLED_0in96_clear();
    DEV_Delay_ms(500);
    piThreadCreate(LCD_2IN_KEY);
    // 0.Create a new image cache

    UWORD Imagesize_0_96 = ((OLED_0in96_WIDTH % 8 == 0) ? (OLED_0in96_WIDTH / 8) : (OLED_0in96_WIDTH / 8 + 1)) * OLED_0in96_HEIGHT;
    UBYTE *BlackImage_0_96;
    if ((BlackImage_0_96 = (UBYTE *)malloc(Imagesize_0_96)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
        return -1;
    }
    printf("Paint_NewImage\r\n");

    Paint_NewImage(BlackImage_0_96, OLED_0in96_WIDTH, OLED_0in96_HEIGHT, 90, BLACK);
    Paint_SetScale(2);
    Paint_Clear(BLACK);
    printf("key test\r\n");

    Paint_DrawRectangle(1, 28, 28, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(1 + 32, 28, 28 + 32, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(1 + 64, 28, 28 + 64, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(1 + 96, 28, 28 + 96, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    OLED_0in96_display(BlackImage_0_96);
    while (1)
    {
        Paint_Clear(BLACK);
        Paint_DrawRectangle(1, 28, 28, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawRectangle(1 + 32, 28, 28 + 32, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawRectangle(1 + 64, 28, 28 + 64, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawRectangle(1 + 96, 28, 28 + 96, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        OLED_0in96_display(BlackImage_0_96);
        if (DEV_Digital_Read(KEY_1) == 0)
        {
            printf("key1 press\r\n");
            Paint_DrawRectangle(1, 28, 28, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            OLED_0in96_display(BlackImage_0_96);
        }
        if (DEV_Digital_Read(KEY_2) == 0)
        {
            printf("key2 press\r\n");
            Paint_DrawRectangle(1 + 32, 28, 28 + 32, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            OLED_0in96_display(BlackImage_0_96);
        }
        if (DEV_Digital_Read(KEY_3) == 0)
        {
            printf("key3 press\r\n");
            Paint_DrawRectangle(1 + 64, 28, 28 + 64, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            OLED_0in96_display(BlackImage_0_96);
        }

        if (DEV_Digital_Read(KEY_4) == 0)
        {
            printf("key4 press\r\n");
            Paint_DrawRectangle(1 + 96, 28, 28 + 96, 40, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            OLED_0in96_display(BlackImage_0_96);
        }
        DEV_Delay_ms(10);
    }
}

PI_THREAD(LCD_2IN_KEY)
{
    LCD_2IN_Init();
    LCD_2IN_Clear(WHITE);
    DEV_SetBacklight(1024);

    UDOUBLE Imagesize_2_0 = LCD_2IN_HEIGHT * LCD_2IN_WIDTH * 2;
    UWORD *BlackImage_2_0;
    if ((BlackImage_2_0 = (UWORD *)malloc(Imagesize_2_0)) == NULL)
    {
        printf("Failed to apply for black memory...\r\n");
    }
    Paint_NewImage_2((UBYTE *)BlackImage_2_0, LCD_2IN_WIDTH, LCD_2IN_HEIGHT, 90, WHITE);
    Paint_SetScale_2(65);
    Paint_Clear_2(BLACK);
    Paint_SetRotate_2(ROTATE_270);

    Paint_DrawRectangle_2(20, 80, 70, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle_2(20 + 80, 80, 70 + 80, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle_2(20 + 160, 80, 70 + 160, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle_2(20 + 240, 80, 70 + 240, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

    LCD_2IN_Display((UBYTE *)BlackImage_2_0);
    while (1)
    {
        Paint_Clear_2(BLACK);
        Paint_DrawRectangle_2(20, 80, 70, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawRectangle_2(20 + 80, 80, 70 + 80, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawRectangle_2(20 + 160, 80, 70 + 160, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawRectangle_2(20 + 240, 80, 70 + 240, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        LCD_2IN_Display((UBYTE *)BlackImage_2_0);
        if (DEV_Digital_Read(KEY_1) == 0)
        {
            Paint_DrawRectangle_2(20, 80, 70, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            LCD_2IN_Display((UBYTE *)BlackImage_2_0);
            while(DEV_Digital_Read(KEY_1) == 0);
        }
        if (DEV_Digital_Read(KEY_2) == 0)
        {
            Paint_DrawRectangle_2(20 + 80, 80, 70 + 80, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            LCD_2IN_Display((UBYTE *)BlackImage_2_0);
            while(DEV_Digital_Read(KEY_2) == 0);
        }

        if (DEV_Digital_Read(KEY_3) == 0)
        {
            Paint_DrawRectangle_2(20 + 160, 80, 70 + 160, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            LCD_2IN_Display((UBYTE *)BlackImage_2_0);
            while(DEV_Digital_Read(KEY_3) == 0);
        }

        if (DEV_Digital_Read(KEY_4) == 0)
        {
            Paint_DrawRectangle_2(20 + 240, 80, 70 + 240, 160, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
            LCD_2IN_Display((UBYTE *)BlackImage_2_0);
            while(DEV_Digital_Read(KEY_4) == 0);
        }
        DEV_Delay_ms(10);
    }

    /* Module Exit */
    free(BlackImage_2_0);
    BlackImage_2_0 = NULL;
    DEV_ModuleExit();
}
