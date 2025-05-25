#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f4xx_hal.h"

// ��������ʼ��
void Buzzer_Init(void);

// ����������
void Buzzer_Beep_Short(void);

// ����������
void Buzzer_Beep_Long(void);

// �������ض�����
void Buzzer_Beep_Times(uint8_t times, uint16_t duration);

#endif /* __BUZZER_H */