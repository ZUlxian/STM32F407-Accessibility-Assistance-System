#ifndef _SCCB_H
#define _SCCB_H

#include "stm32f4xx_hal.h"
#include "ov7670config.h"  // 包含SCCB_ID定义

// 定义OV7670 XCLK引脚
#define OV7670_XCLK_RCC     RCC_AHB1Periph_GPIOA
#define OV7670_XCLK_Pin     GPIO_PIN_8
#define OV7670_XCLK_GPIO    GPIOA
#define STM32_MCO1_DIV      RCC_MCODIV_4

// 定义SCCB引脚
#define OV7670_SCCB_RCC     RCC_AHB1Periph_GPIOF
#define OV7670_SCCB_Pin     (GPIO_PIN_6 | GPIO_PIN_7)
#define OV7670_SCCB_GPIO    GPIOF

// SCCB访问宏
#define SCCB_SCL_H     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_SET)
#define SCCB_SCL_L     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_RESET)
#define SCCB_SDA_H     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_SET)
#define SCCB_SDA_L     HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_RESET)
#define SCCB_READ_SDA  HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_6)

// 函数声明
void SCCB_Init(void);
void SCCB_Start(void);
void SCCB_Stop(void);
void SCCB_No_Ack(void);
void SCCB_SDA_IN(void);
void SCCB_SDA_OUT(void);
uint8_t SCCB_WR_Byte(uint8_t dat);
uint8_t SCCB_RD_Byte(void);
uint8_t SCCB_WR_Reg(uint8_t reg, uint8_t data);
uint8_t SCCB_RD_Reg(uint8_t reg);

#endif /* _SCCB_H */