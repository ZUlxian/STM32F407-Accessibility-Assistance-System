#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f4xx_hal.h"

// 蜂鸣器初始化
void Buzzer_Init(void);

// 蜂鸣器短响
void Buzzer_Beep_Short(void);

// 蜂鸣器长响
void Buzzer_Beep_Long(void);

// 蜂鸣器特定次数
void Buzzer_Beep_Times(uint8_t times, uint16_t duration);

#endif /* __BUZZER_H */