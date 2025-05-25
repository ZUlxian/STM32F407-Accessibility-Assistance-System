#include "sys_delay.h"

static uint16_t fac_us = 0;  /* us延时倍乘数 */
static uint16_t fac_ms = 0;  /* ms延时倍乘数 */

/**
  * @brief  Initialize delay function
  * @param  None
  * @retval None
  */
void delay_init(void)
{
    fac_us = HAL_RCC_GetHCLKFreq() / 1000000;
    fac_ms = fac_us * 1000;
}

/**
  * @brief  Delay milliseconds
  * @param  ms: Number of milliseconds to delay
  * @retval None
  */
void delay_ms(uint16_t ms)
{
    HAL_Delay(ms);
}

/**
  * @brief  Delay microseconds
  * @param  us: Number of microseconds to delay
  * @retval None
  */
void delay_us(uint32_t us)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    
    ticks = us * fac_us;
    told = SysTick->VAL;
    while(1)
    {
        tnow = SysTick->VAL;
        if(tnow != told)
        {
            if(tnow < told)
                tcnt += told - tnow;
            else
                tcnt += reload - tnow + told;
            told = tnow;
            if(tcnt >= ticks)
                break;
        }
    }
}