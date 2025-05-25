#ifndef _DCMI_H
#define _DCMI_H

#include "stm32f4xx_hal.h"

//////////////////////////////////////////////////////////////////////////////////	 
// DCMI驱动代码	   
//////////////////////////////////////////////////////////////////////////////////

/* 全局变量 */
extern DCMI_HandleTypeDef hdcmi;
extern DMA_HandleTypeDef hdma_dcmi;
extern uint8_t ov_rev_ok;

/* 函数声明 */
void My_DCMI_Init(void);
void DCMI_DMA_Init(uint32_t DMA_Memory0BaseAddr, uint16_t DMA_BufferSize, uint32_t DMA_MemoryDataSize, uint32_t DMA_MemoryInc);
void DCMI_Start(void);
void DCMI_Stop(void);

void DCMI_Set_Window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
void DCMI_CR_Set(uint8_t pclk, uint8_t hsync, uint8_t vsync);

#endif /* _DCMI_H */