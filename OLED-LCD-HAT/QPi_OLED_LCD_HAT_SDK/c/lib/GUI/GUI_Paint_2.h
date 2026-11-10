#ifndef __GUI_PAINT_2_H
#define __GUI_PAINT_2_H

#include "DEV_Config.h"
#include "GUI_Paint.h"
#include "../Fonts/fonts.h"

extern PAINT Paint_2;


//init and Clear
void Paint_NewImage_2(UBYTE *image, UWORD Width, UWORD Height, UWORD Rotate, UWORD Color);
void Paint_SelectImage_2(UBYTE *image);
void Paint_SetRotate_2(UWORD Rotate);
void Paint_SetMirroring_2(UBYTE mirror);
void Paint_SetPixel_2(UWORD Xpoint, UWORD Ypoint, UWORD Color);
void Paint_SetScale_2(UBYTE scale);

void Paint_Clear_2(UWORD Color);
void Paint_ClearWindows_2(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color);

//Drawing
void Paint_DrawPoint_2(UWORD Xpoint, UWORD Ypoint, UWORD Color, DOT_PIXEL Dot_Pixel, DOT_STYLE Dot_FillWay);
void Paint_DrawLine_2(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color, DOT_PIXEL Line_width, LINE_STYLE Line_Style);
void Paint_DrawRectangle_2(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill);
void Paint_DrawCircle_2(UWORD X_Center, UWORD Y_Center, UWORD Radius, UWORD Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill);

//Display string
void Paint_DrawChar_2(UWORD Xstart, UWORD Ystart, const char Acsii_Char, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawString_EN_2(UWORD Xstart, UWORD Ystart, const char * pString, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawString_CN_2(UWORD Xstart, UWORD Ystart, const char * pString, cFONT* font, UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawNum_2(UWORD Xpoint, UWORD Ypoint, double Nummber, sFONT* Font, UWORD Digit,UWORD Color_Foreground, UWORD Color_Background);
void Paint_DrawTime_2(UWORD Xstart, UWORD Ystart, PAINT_TIME *pTime, sFONT* Font, UWORD Color_Foreground, UWORD Color_Background);

//pic
void Paint_DrawBitMap_2(const unsigned char* image_buffer);
void Paint_DrawBitMap_Block_2(const unsigned char* image_buffer, UBYTE Region);

void Paint_DrawImage_2(const unsigned char *image, UWORD xStart, UWORD yStart, UWORD W_Image, UWORD H_Image) ;
void Paint_DrawImage1_2(const unsigned char *image, UWORD xStart, UWORD yStart, UWORD W_Image, UWORD H_Image);
 void Paint_BmpWindows_2(unsigned char x,unsigned char y,const unsigned char *pBmp,\
					unsigned char chWidth,unsigned char chHeight);


#endif





