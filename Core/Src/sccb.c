#include "sccb.h"
#include "sys_delay.h"

//初始化SCCB接口 
void SCCB_Init(void)
{              
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能GPIOA和GPIOF的时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    
    /* 配置XCLK引脚(PA8) - MCO1输出 */
    GPIO_InitStruct.Pin = OV7670_XCLK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_MCO; /* MCO功能 */
    HAL_GPIO_Init(OV7670_XCLK_GPIO, &GPIO_InitStruct);
    
    /* 配置MCO1输出HSI时钟 */
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_4);
    
    /* 配置SCCB引脚(PF6, PF7) */
    GPIO_InitStruct.Pin = OV7670_SCCB_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(OV7670_SCCB_GPIO, &GPIO_InitStruct);
    
    /* 设置初始引脚状态 */
    HAL_GPIO_WritePin(OV7670_SCCB_GPIO, OV7670_SCCB_Pin, GPIO_PIN_SET);
    
    /* 配置SDA为输出 */
    SCCB_SDA_OUT();    
}       

/* 配置SDA为输入 */
void SCCB_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

/* 配置SDA为输出 */
void SCCB_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

//SCCB起始信号
//当时钟为高的时候,数据线的高到低,为SCCB起始信号
//在激活状态下,SDA和SCL均为低电平
void SCCB_Start(void)
{
    SCCB_SDA_H;     //数据线高电平    
    delay_us(500); 
    SCCB_SCL_H;     //在时钟线高的时候数据线由高至低
    delay_us(500);  
    SCCB_SDA_L;
    delay_us(500);     
    SCCB_SCL_L;     //数据线恢复低电平，单操作函数必须    
    delay_us(500);
}

//SCCB停止信号
//当时钟为高的时候,数据线的低到高,为SCCB停止信号
//空闲状况下,SDA,SCL均为高电平
void SCCB_Stop(void)
{
    SCCB_SDA_L;
    delay_us(500);     
    SCCB_SCL_H;    
    delay_us(500); 
    SCCB_SDA_H;    
    delay_us(500);
}  

//产生NA信号
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

//SCCB,写入一个字节
//返回值:0,成功;1,失败. 
uint8_t SCCB_WR_Byte(uint8_t dat)
{
    uint8_t j, res;     
    for(j=0; j<8; j++) //循环8次发送数据
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
    SCCB_SDA_IN();        //设置SDA为输入 
    delay_us(500);
    SCCB_SCL_H;            //接收第九位，以判断是否发送成功
    delay_us(100);
    if(SCCB_READ_SDA)
        res = 1;  //SDA=1发送失败，返回1
    else 
        res = 0;  //SDA=0发送成功，返回0
    SCCB_SCL_L;         
    SCCB_SDA_OUT();        //设置SDA为输出    
    return res;  
}     

//SCCB 读取一个字节
//在SCL的上升沿,数据锁存
//返回值:读到的数据
uint8_t SCCB_RD_Byte(void)
{
    uint8_t temp = 0, j;    
    SCCB_SDA_IN();        //设置SDA为输入  
    for(j=8; j>0; j--) 	//循环8次接收数据
    {                  
        delay_us(500);
        SCCB_SCL_H;
        temp = temp << 1;
        if(SCCB_READ_SDA)
            temp++;   
        delay_us(500);
        SCCB_SCL_L;
    }    
    SCCB_SDA_OUT();        //设置SDA为输出    
    return temp;
} 
                                
//写寄存器
//返回值:0,成功;1,失败.
uint8_t SCCB_WR_Reg(uint8_t reg, uint8_t data)
{
    uint8_t res = 0;
    SCCB_Start();                    //启动SCCB传输
    if(SCCB_WR_Byte(SCCB_ID))
        res = 1;    //写器件ID    
    delay_us(100);
    if(SCCB_WR_Byte(reg))
        res = 1;        //写寄存器地址    
    delay_us(100);
    if(SCCB_WR_Byte(data))
        res = 1;     //写数据     
    SCCB_Stop();      
    return    res;
}                    
                  
//读寄存器
//返回值:读到的寄存器值
uint8_t SCCB_RD_Reg(uint8_t reg)
{
    uint8_t val = 0;
    SCCB_Start();                //启动SCCB传输
    SCCB_WR_Byte(SCCB_ID);        //写器件ID    
    delay_us(100);     
    SCCB_WR_Byte(reg);            //写寄存器地址    
    delay_us(100);      
    SCCB_Stop();   
    delay_us(100);       
    //设置寄存器地址后，才是读
    SCCB_Start();
    SCCB_WR_Byte(SCCB_ID|0X01);    //发送读命令    
    delay_us(100);
    val = SCCB_RD_Byte();            //读取数据
    SCCB_No_Ack();
    SCCB_Stop();
    return val;
}