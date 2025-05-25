/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef  OV7670TEST_H
#define  OV7670TEST_H

/* Includes -------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Defines --------------------------------------------------------------------*/
/* UART handle declaration */
extern UART_HandleTypeDef huart2;

/* Types ----------------------------------------------------------------------*/


/* Variables ------------------------------------------------------------------*/


/* Functions ------------------------------------------------------------------*/
void OV7670_USART_Init(void);
void ShanWai_SendCamera(uint16_t *camera_buf, uint16_t length_w, uint16_t length_h);

#endif /* OV7670TEST_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/