#ifndef _OV7670_H
#define _OV7670_H

#include "stm32f4xx_hal.h"


#define OV7670_RST_PW_RCC        RCC_AHB1Periph_GPIOG
#define OV7670_RST_PW_Pin        (GPIO_PIN_8|GPIO_PIN_6)
#define OV7670_RST_PW_GPIO       GPIOG

#define OV7670_PWDN_H            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET)   //POWER DOWN控制信号 
#define OV7670_PWDN_L            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET) //POWER DOWN控制信号 

#define OV7670_RST_H             HAL_GPIO_WritePin(GPIOG, GPIO_PIN_8, GPIO_PIN_SET)   //复位控制信号 
#define OV7670_RST_L             HAL_GPIO_WritePin(GPIOG, GPIO_PIN_8, GPIO_PIN_RESET) //复位控制信号 

//320*240裁剪相关定义
#define PIC_START_X        0       //起始坐标x
#define PIC_START_Y        0       //起始坐标y
#define PIC_WIDTH          240 
#define PIC_HEIGHT         128 

extern uint16_t camera_buffer[PIC_WIDTH*PIC_HEIGHT];


////////////////////////////////////////////////////////////////////////////////// 
#define OV7670_MID              0X7FA2    
#define OV7670_PID              0X7673
/////////////////////////////////////////
                     
uint8_t OV7670_Init(void);               
void OV7670_RST_PW_Init(void);
void OV7670_Light_Mode(uint8_t mode);
void OV7670_Color_Saturation(uint8_t sat);
void OV7670_Brightness(uint8_t bright);
void OV7670_Contrast(uint8_t contrast);
void OV7670_Special_Effects(uint8_t eft);
void OV7670_Window_Set(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
void set_cif(void);

#endif