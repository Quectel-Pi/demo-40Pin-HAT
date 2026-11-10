/*****************************************************************************
* | File      	:   GUI_BMPfile.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*                Used to shield the underlying layers of each master
*                and enhance portability
*----------------
* |	This version:   V2.5
* | Date        :   2023-10-10
* | Info        :   
* -----------------------------------------------------------------------------
* V2.4(2020-08-17):
* 1.Add GUI_ReadBmp_65K()
* -----------------------------------------------------------------------------
* V2.3(2020-08-15):
* 1.Add GUI_ReadBmp_16Gray()
* -----------------------------------------------------------------------------
* V2.2(2020-07-08):
* 1.Add GUI_ReadBmp_RGB_7Color()
* -----------------------------------------------------------------------------
* V2.1(2019-10-10):
* 1.Add GUI_ReadBmp_4Gray()
* -----------------------------------------------------------------------------
* V2.0(2018-11-12):
* 1.Change file name: GUI_BMP.h -> GUI_BMPfile.h
* 2.fix: GUI_ReadBmp()
*   Now Xstart and Xstart can control the position of the picture normally,
*   and support the display of images of any size. If it is larger than
*   the actual display range, it will not be displayed.
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

#include "GUI_BMPfile.h"
#include "Debug.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>	//exit()
#include <string.h> //memset()
#include <math.h> //memset()
#include <stdio.h>

UBYTE GUI_ReadBmp(const char *path, UWORD Xstart, UWORD Ystart)
{
    FILE *fp;                     //Define a file pointer
    BMPFILEHEADER bmpFileHeader;  //Define a bmp file header structure
    BMPINFOHEADER bmpInfoHeader;  //Define a bmp info header structure


    // Binary file open
    if((fp = fopen(path, "rb")) == NULL) {
        Debug("Cann't open the file!\n");
        exit(0);
    }

    // Set the file pointer from the beginning
    fseek(fp, 0, SEEK_SET);
    fread(&bmpFileHeader, sizeof(BMPFILEHEADER), 1, fp);    //sizeof(BMPFILEHEADER) must be 14
    fread(&bmpInfoHeader, sizeof(BMPINFOHEADER), 1, fp);    //sizeof(BMPFILEHEADER) must be 50
    printf("pixel = %d * %d\r\n", bmpInfoHeader.biWidth, bmpInfoHeader.biHeight);

    UWORD Image_Width_Byte = (bmpInfoHeader.biWidth % 8 == 0)? (bmpInfoHeader.biWidth / 8): (bmpInfoHeader.biWidth / 8 + 1);
    UWORD Bmp_Width_Byte = (Image_Width_Byte % 4 == 0) ? Image_Width_Byte: ((Image_Width_Byte / 4 + 1) * 4);
    UBYTE Image[Image_Width_Byte * bmpInfoHeader.biHeight];
    memset(Image, 0xFF, Image_Width_Byte * bmpInfoHeader.biHeight);

    // Determine if it is a monochrome bitmap
    int readbyte = bmpInfoHeader.biBitCount;
    if(readbyte != 1) {
        Debug("the bmp Image is not a monochrome bitmap!\n");
        exit(0);
    }

    // Determine black and white based on the palette
    UWORD i;
    UWORD Bcolor, Wcolor;
    UWORD bmprgbquadsize = pow(2, bmpInfoHeader.biBitCount);// 2^1 = 2
    BMPRGBQUAD bmprgbquad[bmprgbquadsize];        //palette
    // BMPRGBQUAD bmprgbquad[2];        //palette

    for(i = 0; i < bmprgbquadsize; i++){
    // for(i = 0; i < 2; i++) {
        fread(&bmprgbquad[i], sizeof(BMPRGBQUAD), 1, fp);
    }
    if(bmprgbquad[0].rgbBlue == 0xff && bmprgbquad[0].rgbGreen == 0xff && bmprgbquad[0].rgbRed == 0xff) {
        Bcolor = BLACK;
        Wcolor = WHITE;
    } else {
        Bcolor = WHITE;
        Wcolor = BLACK;
    }

    // Read image data into the cache
    UWORD x, y;
    UBYTE Rdata;
    fseek(fp, bmpFileHeader.bOffset, SEEK_SET);
    for(y = 0; y < bmpInfoHeader.biHeight; y++) {//Total display column
        for(x = 0; x < Bmp_Width_Byte; x++) {//Show a line in the line
            if(fread((char *)&Rdata, 1, readbyte, fp) != readbyte) {
                perror("get bmpdata:\r\n");
                break;
            }
            if(x < Image_Width_Byte) { //bmp
                Image[x + (bmpInfoHeader.biHeight - y - 1) * Image_Width_Byte] =  Rdata;
                // printf("rdata = %d\r\n", Rdata);
            }
        }
    }
    fclose(fp);

    // Refresh the image to the display buffer based on the displayed orientation
    UBYTE color, temp;
    for(y = 0; y < bmpInfoHeader.biHeight; y++) {
        for(x = 0; x < bmpInfoHeader.biWidth; x++) {
            if(x > Paint.Width || y > Paint.Height) {
                break;
            }
            temp = Image[(x / 8) + (y * Image_Width_Byte)];
            color = (((temp << (x%8)) & 0x80) == 0x80) ?Bcolor:Wcolor;
            Paint_SetPixel(Xstart + x, Ystart + y, color);
        }
    }
    return 0;
}
/*************************************************************************

*************************************************************************/
UBYTE GUI_ReadBmp_65K(const char *path)
{
    FILE *fp;                     //Define a file pointer 
    BMPFILEHEADER bmpFileHeader;  //Define a bmp file header structure
    BMPINFOHEADER bmpInfoHeader;         //Define a bmp bitmap header structure 
	//UBYTE rgbReversed;
    // Binary file open
    if((fp = fopen(path, "rb")) == NULL) { 	// fp = 0x00426aa0
        Debug("Cann't open the file!\n");
        return 0;
    }
    printf("open:%s \n",path);
    
    fseek(fp, 0, SEEK_SET); 								//Set the file pointer from the beginning	
    fread(&bmpFileHeader, sizeof(BMPFILEHEADER), 1, fp);	//Gets the BMP file header
	fread(&bmpInfoHeader, sizeof(BMPINFOHEADER), 1, fp);			//Gets BMP Info Header
	printf("biBitCount=%d \n",bmpInfoHeader.biBitCount);		//Print a pixel takes several bit!!!
	
	
	//Get palette information, Max 256 color information
	//You can use the malloc function to reduce the memory required to run this program
	/*
			biBitCount	RGBPAD
				1		   2
				4		  16
				8		 256
	 */
	BMPRGBQUAD RGBPAD[256];	
	if(bmpInfoHeader.biBitCount<16)
	{	
		
		uint16_t i=0;
		for(i=0;i<(1<<(bmpInfoHeader.biBitCount));i++)
		{
			fseek(fp,(0X36)+(i*4), SEEK_SET);
			fread(&RGBPAD[i], sizeof(BMPRGBQUAD), 1, fp);			
		}
	}
	
	//In Windows each row data must be divisible by 4 byte
	//Calculate how many bytes per row of data need to be indented

	int32_t  indent;
	if(bmpInfoHeader.biBitCount<8)	indent=(4-((bmpInfoHeader.biWidth/(8/bmpInfoHeader.biBitCount))%4))%4;
	else 							indent=(4-((bmpInfoHeader.biWidth*(bmpInfoHeader.biBitCount/8))%4))%4;
	
	
	int 		row, col; 	
    short 		data;			//All data formats are converted to RGB565 format
	char		pixels,temp;		
	int len  = 	bmpInfoHeader.biBitCount / 8;
	ARGBQUAD 	argb;

	// get bmp data and show
	for(row = 0; row < bmpInfoHeader.biHeight;row++) 
	{		
		for(col = 0; col < bmpInfoHeader.biWidth; ) 
		{
			if(bmpInfoHeader.biBitCount==16)
			{
				if(fread((char *)&data, 1, len, fp) != len)
				{
					perror("get bmpdata:\r\n");
					break;
				}  
				//ARGB4444 format cannot be recognized for the time being. It can only be used to identify RGB565 format information!!
				if(bmpInfoHeader.biInfoSize==0x38)
				{	
					Paint_SetPixel_2(col, bmpInfoHeader.biHeight - row - 1, data);
				}
				//Used to identify the XRGB1555 format
				else if((bmpInfoHeader.biInfoSize==0x28)&&(bmpInfoHeader.biCompression==0x00))
				{
					data=((((long)((data>>5)&0x1f)*0X3F)/0X1F)<<5)+(data&0x1f)+((data&0xEC00)<<1);
					Paint_SetPixel_2(col, bmpInfoHeader.biHeight - row - 1, data);
				}
				col++;
			}
			//For RGB888 ARGB8888 XRGB8888 format uniform compression and removal of alpha
			else if(bmpInfoHeader.biBitCount>16)
			{
				if(fread((char *)&argb, 1, len, fp) != len)
				{
					printf("x:%d,y:%d\r\n",col,row);
					perror("get bmpdata: \r\n");
					break;
				}
				data = RGB((argb.rgbRed), (argb.rgbGreen), (argb.rgbBlue));
				Paint_SetPixel_2(col, bmpInfoHeader.biHeight - row - 1, data);
				col++;
			}	
			//biBitCount<8 format
			else		
			{
				if(fread((char *)&pixels, 1, 1, fp) != 1)
				{
					perror("get bmpdata:\r\n");
					break;
				}
				for(temp=0;(temp<(8/bmpInfoHeader.biBitCount))&&(col < bmpInfoHeader.biWidth);temp++,col++)
				{
					if(bmpInfoHeader.biBitCount==1){
						data=(pixels<<temp)>>7;
						data&=0x01;
					}
					else if(bmpInfoHeader.biBitCount==4){	
						data=(pixels<<(temp*4))>>4;
						data&=0x0f;					
					}
					else {data=pixels;}
					data=RGB((RGBPAD[data].rgbRed), (RGBPAD[data].rgbGreen), (RGBPAD[data].rgbBlue));					
					Paint_SetPixel_2(col, bmpInfoHeader.biHeight - row - 1, data);
				}				
			}	
		}
		//indent!4 byte alignment
		fseek(fp, indent, SEEK_CUR);
	}
	
	fclose(fp);
    return 0;
}

