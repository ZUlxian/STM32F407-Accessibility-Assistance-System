#ifndef __SYS_CFG_H
#define __SYS_CFG_H

#include "stm32f4xx_hal.h"

/* System typedefs - add these to fix lcd.h errors */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* Function Prototypes */
void Sys_Config(void);

#endif /* __SYS_CFG_H */