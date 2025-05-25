#include "sys_cfg.h"
#include "sys_delay.h"

/**
  * @brief  System configuration
  * @param  None
  * @retval None
  */
void Sys_Config(void)
{
    /* Initialize delay functions */
    delay_init();
}
