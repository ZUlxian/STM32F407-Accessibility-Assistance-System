#include "hc_sr04.h"
#include "stm32f4xx_hal.h"

// 启用DWT计数器，用于精确延时
static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
}

// 微秒级延时
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000000) * us;
    while((DWT->CYCCNT - start) < cycles);
}

void HC_SR04_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 初始化DWT计数器
    DWT_Init();
    
    // 启用GPIOB时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // 配置前方传感器TRIG引脚为输出
    GPIO_InitStruct.Pin = TRIG1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(TRIG1_PORT, &GPIO_InitStruct);
    
    // 配置前方传感器ECHO引脚为输入
    GPIO_InitStruct.Pin = ECHO1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ECHO1_PORT, &GPIO_InitStruct);
    
    // 配置侧面传感器TRIG引脚为输出
    GPIO_InitStruct.Pin = TRIG2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(TRIG2_PORT, &GPIO_InitStruct);
    
    // 配置侧面传感器ECHO引脚为输入
    GPIO_InitStruct.Pin = ECHO2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(ECHO2_PORT, &GPIO_InitStruct);
    
    // 初始化完成，TRIG引脚置低
    HAL_GPIO_WritePin(TRIG1_PORT, TRIG1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TRIG2_PORT, TRIG2_PIN, GPIO_PIN_RESET);
    
    // 短暂延时确保初始化完成
    HAL_Delay(10);
}

float HC_SR04_ReadDistance(HC_SR04_Sensor sensor)
{
    GPIO_TypeDef* trig_port;
    uint16_t trig_pin;
    GPIO_TypeDef* echo_port;
    uint16_t echo_pin;
    
    // 选择传感器
    if (sensor == HC_SR04_FRONT) {
        trig_port = TRIG1_PORT;
        trig_pin = TRIG1_PIN;
        echo_port = ECHO1_PORT;
        echo_pin = ECHO1_PIN;
    } else {
        trig_port = TRIG2_PORT;
        trig_pin = TRIG2_PIN;
        echo_port = ECHO2_PORT;
        echo_pin = ECHO2_PIN;
    }
    
    uint32_t start_time, echo_time;
    float distance;
    
    // 发送10us的触发脉冲
    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_SET);
    delay_us(15);  // 确保至少10us的触发脉冲
    HAL_GPIO_WritePin(trig_port, trig_pin, GPIO_PIN_RESET);
    
    // 等待Echo引脚变高，表示收到回波信号
    uint32_t timeout1 = HAL_GetTick() + 10; // 10ms超时
    while (HAL_GPIO_ReadPin(echo_port, echo_pin) == GPIO_PIN_RESET)
    {
        if (HAL_GetTick() > timeout1) {
            return -1.0f; // 超时，返回错误值
        }
    }
    
    // 记录Echo信号开始时间
    start_time = DWT->CYCCNT;
    
    // 等待Echo引脚变低，表示回波信号结束
    uint32_t timeout2 = HAL_GetTick() + 40; // 40ms超时
    while (HAL_GPIO_ReadPin(echo_port, echo_pin) == GPIO_PIN_SET)
    {
        if (HAL_GetTick() > timeout2) {
            return -2.0f; // 超时，返回错误值
        }
    }
    
    // 计算Echo信号持续时间（单位：时钟周期）
    echo_time = DWT->CYCCNT - start_time;
    
    // 转换为微秒
    echo_time = echo_time / (SystemCoreClock / 1000000);
    
    // 计算距离（厘米）= 时间（微秒）* 声速（340米/秒）/ 2 / 10000
    // 简化：距离（厘米）= 时间（微秒）* 0.017
    distance = echo_time * 0.017f;
    
    // 限制最大测量距离为400cm
    if (distance > 400.0f) {
        return -3.0f; // 超出范围，返回错误值
    }
    
    return distance;
}