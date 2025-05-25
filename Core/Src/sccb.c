#include "sccb.h"
#include "sys_delay.h"

//��ʼ��SCCB�ӿ� 
void SCCB_Init(void)
{              
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* ʹ��GPIOA��GPIOF��ʱ�� */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    
    /* ����XCLK����(PA8) - MCO1��� */
    GPIO_InitStruct.Pin = OV7670_XCLK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO; /* MCO���� */
    HAL_GPIO_Init(OV7670_XCLK_GPIO, &GPIO_InitStruct);
    
    /* ����MCO1���HSIʱ�� */
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_4);
    
    /* ����SCCB����(PF6, PF7) */
    GPIO_InitStruct.Pin = OV7670_SCCB_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(OV7670_SCCB_GPIO, &GPIO_InitStruct);
    
    /* ���ó�ʼ����״̬ */
    HAL_GPIO_WritePin(OV7670_SCCB_GPIO, OV7670_SCCB_Pin, GPIO_PIN_SET);
    
    /* ����SDAΪ��� */
    SCCB_SDA_OUT();    
}       

/* ����SDAΪ���� */
void SCCB_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

/* ����SDAΪ��� */
void SCCB_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

//SCCB��ʼ�ź�
//��ʱ��Ϊ�ߵ�ʱ��,�����ߵĸߵ���,ΪSCCB��ʼ�ź�
//�ڼ���״̬��,SDA��SCL��Ϊ�͵�ƽ
void SCCB_Start(void)
{
    SCCB_SDA_H;     //�����߸ߵ�ƽ    
    delay_us(500); 
    SCCB_SCL_H;     //��ʱ���߸ߵ�ʱ���������ɸ�����
    delay_us(500);  
    SCCB_SDA_L;
    delay_us(500);     
    SCCB_SCL_L;     //�����߻ָ��͵�ƽ����������������    
    delay_us(500);
}

//SCCBֹͣ�ź�
//��ʱ��Ϊ�ߵ�ʱ��,�����ߵĵ͵���,ΪSCCBֹͣ�ź�
//����״����,SDA,SCL��Ϊ�ߵ�ƽ
void SCCB_Stop(void)
{
    SCCB_SDA_L;
    delay_us(500);     
    SCCB_SCL_H;    
    delay_us(500); 
    SCCB_SDA_H;    
    delay_us(500);
}  

//����NA�ź�
void SCCB_No_Ack(void)
{
    delay_us(500);
    SCCB_SDA_H;    
    SCCB_SCL_H;    
    delay_us(500);
    SCCB_SCL_L;    
    delay_us(500);
    SCCB_SDA_L;    
    delay_us(500);
}

//SCCB,д��һ���ֽ�
//����ֵ:0,�ɹ�;1,ʧ��. 
uint8_t SCCB_WR_Byte(uint8_t dat)
{
    uint8_t j, res;     
    for(j=0; j<8; j++) //ѭ��8�η�������
    {
        if(dat & 0x80)
            SCCB_SDA_H;    
        else 
            SCCB_SDA_L;
        dat <<= 1;
        delay_us(500);
        SCCB_SCL_H;    
        delay_us(500);
        SCCB_SCL_L;       
    }             
    SCCB_SDA_IN();        //����SDAΪ���� 
    delay_us(500);
    SCCB_SCL_H;            //���յھ�λ�����ж��Ƿ��ͳɹ�
    delay_us(100);
    if(SCCB_READ_SDA)
        res = 1;  //SDA=1����ʧ�ܣ�����1
    else 
        res = 0;  //SDA=0���ͳɹ�������0
    SCCB_SCL_L;         
    SCCB_SDA_OUT();        //����SDAΪ���    
    return res;  
}     

//SCCB ��ȡһ���ֽ�
//��SCL��������,��������
//����ֵ:����������
uint8_t SCCB_RD_Byte(void)
{
    uint8_t temp = 0, j;    
    SCCB_SDA_IN();        //����SDAΪ����  
    for(j=8; j>0; j--) 	//ѭ��8�ν�������
    {                  
        delay_us(500);
        SCCB_SCL_H;
        temp = temp << 1;
        if(SCCB_READ_SDA)
            temp++;   
        delay_us(500);
        SCCB_SCL_L;
    }    
    SCCB_SDA_OUT();        //����SDAΪ���    
    return temp;
} 
                                
//д�Ĵ���
//����ֵ:0,�ɹ�;1,ʧ��.
uint8_t SCCB_WR_Reg(uint8_t reg, uint8_t data)
{
    uint8_t res = 0;
    SCCB_Start();                    //����SCCB����
    if(SCCB_WR_Byte(SCCB_ID))
        res = 1;    //д����ID    
    delay_us(100);
    if(SCCB_WR_Byte(reg))
        res = 1;        //д�Ĵ�����ַ    
    delay_us(100);
    if(SCCB_WR_Byte(data))
        res = 1;     //д����     
    SCCB_Stop();      
    return    res;
}                    
                  
//���Ĵ���
//����ֵ:�����ļĴ���ֵ
uint8_t SCCB_RD_Reg(uint8_t reg)
{
    uint8_t val = 0;
    SCCB_Start();                //����SCCB����
    SCCB_WR_Byte(SCCB_ID);        //д����ID    
    delay_us(100);     
    SCCB_WR_Byte(reg);            //д�Ĵ�����ַ    
    delay_us(100);      
    SCCB_Stop();   
    delay_us(100);       
    //���üĴ�����ַ�󣬲��Ƕ�
    SCCB_Start();
    SCCB_WR_Byte(SCCB_ID|0X01);    //���Ͷ�����    
    delay_us(100);
    val = SCCB_RD_Byte();            //��ȡ����
    SCCB_No_Ack();
    SCCB_Stop();
    return val;
}