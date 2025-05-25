/**
  ******************************************************************************
  * @file    ov7670test.c 
  * @author  jinhao
  * @version V1.0.0
  * @date    22-April-2016
  * @brief   Main program body
  ******************************************************************************
  */
  
/* Includes -------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "sys_cfg.h"
#include "sys_delay.h"
#include "lcd.h"
#include "ov7670.h"
#include "ov7670test.h"
#include "main.h"  /* Add this include for Error_Handler declaration */

/* Defines --------------------------------------------------------------------*/

/* Variables ------------------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* Functions ------------------------------------------------------------------*/

void USART_SendByte(UART_HandleTypeDef* huart, uint8_t data)
{
    HAL_UART_Transmit(huart, &data, 1, HAL_MAX_DELAY);
}

void USART_Send2Byte(UART_HandleTypeDef* huart, uint16_t data)
{
    uint8_t temp[2];
    temp[0] = (uint8_t)data;
    temp[1] = (uint8_t)(data >> 8);
    
    HAL_UART_Transmit(huart, temp, 2, HAL_MAX_DELAY);
}

void OV7670_USART_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIO and USART2 clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    
    /* Configure USART2 pins */
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* Configure USART2 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 256000;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        /* Initialization Error */
        Error_Handler();
    }
    
    USART_Send2Byte(&huart2, 0);
}

void ShanWai_SendCamera(uint16_t *camera_buf, uint16_t length_w, uint16_t length_h)
{
    uint16_t i = 0;

    USART_Send2Byte(&huart2, 0xFE01);

    for (i = 0; i < length_w * length_h; i++)
    {
        USART_Send2Byte(&huart2, camera_buf[i]);
    }

    USART_Send2Byte(&huart2, 0x01FE);
}