#include "dcmi.h" 
#include "ov7670.h" 
#include "stdio.h"
#include "string.h"
#include "sys_cfg.h"
#include "st7789.h"  // ����TFT��ʾ����

/* ȫ�ֱ��� */
DCMI_HandleTypeDef hdcmi;
DMA_HandleTypeDef hdma_dcmi;

uint8_t ov_frame = 0;                        
uint32_t datanum = 0;
uint32_t HSYNC = 0;
uint32_t VSYNC = 0;
uint8_t ov_rev_ok = 0;

/* DCMI DMA���� - �޸�DMA���������������� */
void DCMI_DMA_Init(uint32_t DMA_Memory0BaseAddr, uint16_t DMA_BufferSize, uint32_t DMA_MemoryDataSize, uint32_t DMA_MemoryInc)
{ 
    /* ʹ��DMA2ʱ�� */
    __HAL_RCC_DMA2_CLK_ENABLE();
    
    /* ����DMA */
    hdma_dcmi.Instance = DMA2_Stream1;
    hdma_dcmi.Init.Channel = DMA_CHANNEL_1;
    hdma_dcmi.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_dcmi.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dcmi.Init.MemInc = DMA_MINC_ENABLE; // ǿ��ʹ���ڴ�����ģʽ
    hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_dcmi.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    
    // �ؼ��޸���ʹ��DMA_NORMALģʽ��������DMA_CIRCULAR
    hdma_dcmi.Init.Mode = DMA_NORMAL;
    
    hdma_dcmi.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_dcmi.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_dcmi.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_dcmi.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_dcmi.Init.PeriphBurst = DMA_PBURST_SINGLE;
    
    /* ��ʼ��DMA */
    HAL_DMA_DeInit(&hdma_dcmi);
    HAL_DMA_Init(&hdma_dcmi);
    
    /* ����DMA�ж� */
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
    
    /* ��DMA���������DCMI��� */
    __HAL_LINKDMA(&hdcmi, DMA_Handle, hdma_dcmi);
    
    /* ����DMA�洢����ַ�ͻ�������С */
    hdma_dcmi.Instance->PAR = (uint32_t)&(DCMI->DR);
    hdma_dcmi.Instance->M0AR = DMA_Memory0BaseAddr;
    
    // �ؼ��޸���ȷ��ʹ����ȷ�Ļ�������С
    hdma_dcmi.Instance->NDTR = DMA_BufferSize;
} 

/* DMA������ɻص����� */
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma)
{
    if(hdma->Instance == DMA2_Stream1) {
        /* ����֡������ɱ�־ */
        ov_rev_ok = 1;
        
        /* ����֡���� */
        datanum++;
    }
}

/* DCMI��ʼ�� */
void My_DCMI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* ʹ��GPIOʱ�� */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    
    /* ʹ��DCMIʱ�� */
    __HAL_RCC_DCMI_CLK_ENABLE();
    
    /* ����DCMI���� - ���׼��һ�� */
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
    
    /* PC6/7 - D0, D1 (�Ƴ���PC8��PC9) */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* PE0/1/4/5/6 - D2, D3, D4, D6, D7 (����PE0��PE1) */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Alternate = GPIO_AF13_DCMI;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    /* ����ȡ����ʼ��DCMI��ȷ����ȫ���� */
    HAL_DCMI_DeInit(&hdcmi);
    
    /* ����DCMI - ���׼��һ�µĲ��� */
    hdcmi.Instance = DCMI;
    hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
    hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_FALLING;
    hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_HIGH;
    hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
    hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
    hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
    hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
    
    /* ��ʼ��DCMI */
    HAL_DCMI_Init(&hdcmi);
    
    /* ����DCMI�ж� */
    HAL_NVIC_SetPriority(DCMI_IRQn, 3, 2);
    HAL_NVIC_EnableIRQ(DCMI_IRQn);
    
    /* ʹ��DCMI֡���к�VSYNC�ж� */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME | DCMI_IT_LINE | DCMI_IT_VSYNC);
    
    /* ʹ��DCMI */
    __HAL_DCMI_ENABLE(&hdcmi);
}

/* DCMI�������� - ����DMA���ý����޸� */
void DCMI_Start(void)
{ 
    /* ���DMA��־ */
    __HAL_DMA_CLEAR_FLAG(&hdma_dcmi, DMA_FLAG_TCIF1_5 | DMA_FLAG_HTIF1_5 | 
                         DMA_FLAG_TEIF1_5 | DMA_FLAG_DMEIF1_5 | DMA_FLAG_FEIF1_5);
    
    /* ����DMA���������� */
    __HAL_DMA_DISABLE(&hdma_dcmi);
    
    /* ��������DMA��ַ�ͳ��� */
    hdma_dcmi.Instance->PAR = (uint32_t)&(DCMI->DR);
    hdma_dcmi.Instance->M0AR = (uint32_t)&camera_buffer;
    hdma_dcmi.Instance->NDTR = PIC_WIDTH * PIC_HEIGHT;
    
    /* ʹ��DMA Stream */
    __HAL_DMA_ENABLE(&hdma_dcmi);
    
    /* ����DCMI���� */
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)&camera_buffer, PIC_WIDTH * PIC_HEIGHT);
}

/* DCMIֹͣ���� */
void DCMI_Stop(void)
{ 
    /* ֹͣDCMI */
    HAL_DCMI_Stop(&hdcmi);
    
    /* �ȴ�������� */
    uint32_t timeout = HAL_GetTick() + 100; // 100ms��ʱ
    while((hdcmi.Instance->CR & DCMI_CR_CAPTURE) && HAL_GetTick() < timeout);
    
    /* ��ֹDMA */
    __HAL_DMA_DISABLE(&hdma_dcmi);
} 

/* DCMI���¼��ص����� */
void HAL_DCMI_LineEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    ov_frame++;
}

/* DCMI֡�¼��ص����� */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    /* ֡������� */
    // ������������ov_rev_ok����DMA��ɴ��䴥��
}

/* DCMI VSYNC�¼��ص����� */
void HAL_DCMI_VsyncEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    /* VSYNC��⵽ */
    VSYNC++;
}

/* DCMI����ص����� */
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi)
{
    /* ������� */
}

/* DCMI������ʾ���� */
void DCMI_Set_Window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    DCMI_Stop(); 
    
    /* ��������DMA */
    __HAL_DMA_ENABLE(&hdma_dcmi);
    
    /* ����DCMI���� */
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT, (uint32_t)&camera_buffer, PIC_WIDTH * PIC_HEIGHT);
}

void DCMI_IRQHandler(void)
{
    HAL_DCMI_IRQHandler(&hdcmi);
    
    // ����Ƿ���֡����ж�
    if(__HAL_DCMI_GET_FLAG(&hdcmi, DCMI_FLAG_FRAMERI) != RESET) {
        // ����жϱ�־
        __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI);
    }
    
    // ���жϴ��� - ����������
    if(__HAL_DCMI_GET_FLAG(&hdcmi, DCMI_FLAG_LINERI) != RESET) {
        // ����жϱ�־
        __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_LINERI);
        
        // �����������¼�м���
        ov_frame++;
    }
}

/* �޸�DMA�жϴ����� - �ؼ����� */
void DMA2_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_dcmi);
    
    // ��鴫����ɱ�־ - �ؼ��޸�
    if(__HAL_DMA_GET_FLAG(&hdma_dcmi, DMA_FLAG_TCIF1_5) != RESET) {
        // ��������׼���ñ�־
        ov_rev_ok = 1;
        
        // �����־
        __HAL_DMA_CLEAR_FLAG(&hdma_dcmi, DMA_FLAG_TCIF1_5);
        
        // �������ݼ���
        datanum++;
    }
}