
#include "LCD_2inch.h"
#include "test.h"

void LCD_2IN_test(void)
{
    /* Module Init */
	if(DEV_ModuleInit() != 0){
        DEV_ModuleExit();
        exit(1);
    }
	
    /* LCD Init */
	printf("2inch LCD demo...\r\n");
	LCD_2IN_Init();
   
	LCD_2IN_Clear(BLUE); 
    DEV_SetBacklight(1024);
   
    UDOUBLE Imagesize = LCD_2IN_HEIGHT * LCD_2IN_WIDTH * 2; // 240 * 320 * 2
    UWORD *BlackImage;
     
    if((BlackImage = (UWORD *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
        exit(1);
    }
	
    // /*1.Create a new image cache named IMAGE_RGB and fill it with white*/
    Paint_NewImage_2((UBYTE *)BlackImage, LCD_2IN_WIDTH, LCD_2IN_HEIGHT, 90, WHITE);
   
    Paint_SetScale_2(65); 
    Paint_Clear_2(WHITE);
	Paint_SetRotate_2(ROTATE_270);

    // /* GUI */
    // /*2.Drawing on the image*/
    Paint_DrawPoint_2(5, 10, BLACK, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawPoint_2(5, 25, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    Paint_DrawPoint_2(5, 40, BLACK, DOT_PIXEL_3X3, DOT_STYLE_DFT);
    Paint_DrawPoint_2(5, 55, BLACK, DOT_PIXEL_4X4, DOT_STYLE_DFT);

    Paint_DrawLine_2(20, 10, 70, 60, RED, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine_2(70, 10, 20, 60, RED, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine_2(170, 15, 170, 55, RED, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine_2(150, 35, 190, 35, RED, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);

    Paint_DrawRectangle_2(20, 10, 70, 60, BLUE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle_2(85, 10, 130, 60, BLUE, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    Paint_DrawCircle_2(170, 35, 20, GREEN, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle_2(170, 85, 20, GREEN, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    Paint_DrawString_EN_2(5, 70, "hello world", &Font16, WHITE, BLACK);
    Paint_DrawString_EN_2(5, 90, "waveshare", &Font20, RED, IMAGE_BACKGROUND);

    Paint_DrawNum_2(5, 160, 123.456789, &Font20, 4, GREEN, IMAGE_BACKGROUND);
	Paint_DrawString_CN_2(5,200, "Î¢Ñ©µç×Ó",  &Font24CN,IMAGE_BACKGROUND,BLUE);   
    // /*3.Refresh the picture in RAM to LCD*/
    printf("drawing...\r\n");
    LCD_2IN_Display((UBYTE *)BlackImage); 

	DEV_Delay_ms(2000);
    // /* show bmp */
	printf("show bmp\r\n");
	
	GUI_ReadBmp_65K("./pic/LCD_2inch.bmp");    
    LCD_2IN_Display((UBYTE *)BlackImage);
    DEV_Delay_ms(2000);
	

	
    /* Module Exit */
    free(BlackImage);
    BlackImage = NULL;
	DEV_ModuleExit();
}

