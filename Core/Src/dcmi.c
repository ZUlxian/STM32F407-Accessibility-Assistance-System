#include "dcmi.h" 
#include "ov7670.h" 
#include "stdio.h"
#include "string.h"
#include "sys_cfg.h"
#include "st7789.h"  // 用于TFT显示函数

/* 全局变量 */
DCMI_HandleTypeDef hdcmi;
DMA_HandleTypeDef hdma_dcmi;

uint8_t ov_frame = 0;                        
uint32_t datanum = 0;
uint32_t HSYNC = 0;
uint32_t VSYNC = 0;
uint8_t ov_rev_ok = 0;

/* DCMI DMA配置 - 修复DMA缓冲区和配置问题 */
void DCMI_DMA_Init(uint32_t DMA_Memory0BaseAddr, uint16_t DMA_BufferSize, uint32_t DMA_MemoryDataSize, uint32_t DMA_MemoryInc)
{ 
    /* 使能DMA2时钟 */
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    /* 配置DMA */
    hdma_dcmi.Instance = DMA2_Stream1;
    hdma_dcmi.Init.Channel = DMA_CHANNEL_1;
    hdma_dcmi.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_dcmi.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dcmi.Init.MemInc = DMA_MINC_ENABLE; // 强制使用内存增量模式
    hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    
    // 关键修复：使用DMA_NORMAL模式，而不是DMA_CIRCULAR
    hdma_dcmi.Init.Mode = DMA_NORMAL;
    
    hdma_dcmi.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_dcmi.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_dcmi.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_dcmi.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_dcmi.Init.PeriphBurst = DMA_PBURST_SINGLE;
    
    /* 初始化DMA */
    HAL_DMA_DeInit(&hdma_dcmi);
    HAL_DMA_Init(&hdma_dcmi);
    
    /* 配置DMA中断 */
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    
    /* 将DMA句柄关联到DCMI句柄 */
    __HAL_LINKDMA(&hdcmi, DMA_Handle, hdma_dcmi);
    
    /* 设置DMA存储器地址和缓冲区大小 */
    hdma_dcmi.Instance->PAR = (uint32_t)&(DCMI->DR);
    hdma_dcmi.Instance->M0AR = DMA_Memory0BaseAddr;
    
    // 关键修复：确保使用正确的缓冲区大小
    hdma_dcmi.Instance->NDTR = DMA_BufferSize;
} 

/* DMA传输完成回调函数 */
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
    if(hdma->Instance == DMA2_Stream1) {
        /* 设置帧捕获完成标志 */
        ov_rev_ok = 1;
        
        /* 增加帧计数 */
        datanum++;
    }
}

/* DCMI初始化 */
void My_DCMI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* 使能GPIO时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    /* 使能DCMI时钟 */
    __HAL_RCC_DCMI_CLK_ENABLE();
    
    /* 配置DCMI引脚 - 与标准库一致 */
    /* PA4/6 - HSYNC, PIXCLK */
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* PB6/7 - D5, VSYNC */
    GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_6;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* PC6/7 - D0, D1 (移除了PC8和PC9) */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* PE0/1/4/5/6 - D2, D3, D4, D6, D7 (新增PE0和PE1) */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* 首先取消初始化DCMI，确保完全重置 */
    HAL_DCMI_DeInit(&hdcmi);
    
    /* 配置DCMI - 与标准库一致的参数 */
    hdcmi.Instance = DCMI;
    hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
    hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_FALLING;
    hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_HIGH;
    hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
    hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
    hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
    hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
    
    /* 初始化DCMI */
    HAL_DCMI_Init(&hdcmi);
    
    /* 配置DCMI中断 */
    HAL_NVIC_SetPriority(DCMI_IRQn, 3, 2);
    HAL_NVIC_EnableIRQ(DCMI_IRQn);
    
    /* 使能DCMI帧、行和VSYNC中断 */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME | DCMI_IT_LINE | DCMI_IT_VSYNC);
    
    /* 使能DCMI */
    __HAL_DCMI_ENABLE(&hdcmi);
}

/* DCMI启动传输 - 仅对DMA配置进行修复 */
void DCMI_Start(void)
{ 
    /* 清除DMA标志 */
    __HAL_DMA_CLEAR_FLAG(&hdma_dcmi, DMA_FLAG_TCIF1_5 | DMA_FLAG_HTIF1_5 | 
                         DMA_FLAG_TEIF1_5 | DMA_FLAG_DMEIF1_5 | DMA_FLAG_FEIF1_5);
    
    /* 禁用DMA并重新配置 */
    __HAL_DMA_DISABLE(&hdma_dcmi);
    
    /* 重新设置DMA地址和长度 */
    hdma_dcmi.Instance->PAR = (uint32_t)&(DCMI->DR);
    hdma_dcmi.Instance->M0AR = (uint32_t)&camera_buffer;
    hdma_dcmi.Instance->NDTR = PIC_WIDTH * PIC_HEIGHT;
    
    /* 使能DMA Stream */
    __HAL_DMA_ENABLE(&hdma_dcmi);
    
    /* 启动DCMI捕获 */
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)&camera_buffer, PIC_WIDTH * PIC_HEIGHT);
}

/* DCMI停止传输 */
void DCMI_Stop(void)
{ 
    /* 停止DCMI */
    HAL_DCMI_Stop(&hdcmi);
    
    /* 等待传输结束 */
    uint32_t timeout = HAL_GetTick() + 100; // 100ms超时
    while((hdcmi.Instance->CR & DCMI_CR_CAPTURE) && HAL_GetTick() < timeout);
    
    /* 禁止DMA */
    __HAL_DMA_DISABLE(&hdma_dcmi);
} 

/* DCMI行事件回调函数 */
void HAL_DCMI_LineEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    ov_frame++;
}

/* DCMI帧事件回调函数 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    /* 帧捕获完成 */
    // 不在这里设置ov_rev_ok，让DMA完成传输触发
}

/* DCMI VSYNC事件回调函数 */
void HAL_DCMI_VsyncEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    /* VSYNC检测到 */
    VSYNC++;
}

/* DCMI错误回调函数 */
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi)
{
    /* 处理错误 */
}

/* DCMI设置显示窗口 */
void DCMI_Set_Window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    DCMI_Stop(); 
    
    /* 重新启用DMA */
    __HAL_DMA_ENABLE(&hdma_dcmi);
    
    /* 启动DCMI捕获 */
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)&camera_buffer, PIC_WIDTH * PIC_HEIGHT);
}

void DCMI_IRQHandler(void)
{
    HAL_DCMI_IRQHandler(&hdcmi);
    
    // 检查是否是帧完成中断
    if(__HAL_DCMI_GET_FLAG(&hdcmi, DCMI_FLAG_FRAMERI) != RESET) {
        // 清除中断标志
        __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI);
    }
    
    // 行中断处理 - 计数器增加
    if(__HAL_DCMI_GET_FLAG(&hdcmi, DCMI_FLAG_LINERI) != RESET) {
        // 清除中断标志
        __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_LINERI);
        
        // 可以在这里记录行计数
        ov_frame++;
    }
}

/* 修复DMA中断处理函数 - 关键部分 */
void DMA2_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_dcmi);
    
    // 检查传输完成标志 - 关键修改
    if(__HAL_DMA_GET_FLAG(&hdma_dcmi, DMA_FLAG_TCIF1_5) != RESET) {
        // 设置数据准备好标志
        ov_rev_ok = 1;
        
        // 清除标志
        __HAL_DMA_CLEAR_FLAG(&hdma_dcmi, DMA_FLAG_TCIF1_5);
        
        // 增加数据计数
        datanum++;
    }
}