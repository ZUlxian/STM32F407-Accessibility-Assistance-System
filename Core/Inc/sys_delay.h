#ifndef __SYS_DELAY_H
#define __SYS_DELAY_H

#include "stm32f4xx_hal.h"

void delay_init(void);
void delay_ms(uint16_t ms);
void delay_us(uint32_t us);

#endif