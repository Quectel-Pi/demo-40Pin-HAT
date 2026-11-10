#include <stdlib.h>     //exit()
#include <signal.h>     //signal()
#include "test.h"
#include <string.h>

void  Handler(int signo)
{
    //System Exit
    printf("\r\nHandler:exit\r\n");
    DEV_ModuleExit();

    exit(0);
}

int main(int argc, char *argv[])
{
    int ret = 0;

    // Exception handling:ctrl + c
    signal(SIGINT, Handler);
    
    if (argc != 2){
        printf("please input OLED size and type! \r\n");
        printf("example: sudo ./main 0.96 or sudo ./main 2 \r\n");
        exit(1);
    }
    
    printf("%s OLED Moudule\r\n", argv[1]);
        
    if(strcmp(argv[1], "0.96") == 0)
        ret = OLED_0in96_test();
    else if(strcmp(argv[1], "2") == 0)
        LCD_2IN_test();
    else if(strcmp(argv[1], "all") == 0)
        ret = Triple_OLED_LCD();   
    else if(strcmp(argv[1], "key") == 0)
        ret = key_test();  
    else {
        printf("error: can not find the OLED\r\n");
        ret = 1;
    }
    
    return ret;
    
}
