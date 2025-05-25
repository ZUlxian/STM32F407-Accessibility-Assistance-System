/**
  * @file    haptic.h
  * @brief   STM32ֱ����������������ģ��
  */
  
#ifndef __HAPTIC_H
#define __HAPTIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ��ģ�鶨�� */
#define VIBRATOR_1              1   // ǰ����ģ��
#define VIBRATOR_2              2   // ������ģ��

/* ���Ŷ��� - ����ʵ�������޸� */
#define BUZZER_PORT             GPIOB
#define BUZZER_PIN              GPIO_PIN_5

#define VIBRATOR1_PORT          GPIOD
#define VIBRATOR1_PIN           GPIO_PIN_0

#define VIBRATOR2_PORT          GPIOD
#define VIBRATOR2_PIN           GPIO_PIN_1

/**
  * @brief  ��ʼ������������ģ��
  */
void Haptic_Init(void);

/**
  * @brief  ����������
  * @param  times: ��������
  */
void Beep(uint8_t times);

/**
  * @brief  �̴ٷ���
  */
void Buzzer_Beep_Short(void);

/**
  * @brief  ǿ���̴ٷ���������ʱ���������������
  */
void Buzzer_Beep_Strong(void);

/**
  * @brief  ��η���
  * @param  times: ��������
  * @param  interval: �������(ms)
  */
void Buzzer_Beep_Times(uint8_t times, uint16_t interval);

/**
  * @brief  ���ǿ����������������
  * @param  times: ��������
  * @param  interval: �������(ms)
  */
void Buzzer_Beep_Times_Strong(uint8_t times, uint16_t interval);

/**
  * @brief  ��������
  * @param  duration: ����ʱ��(ms)
  */
void BeepContinuous(uint16_t duration);

/**
  * @brief  ����ʽ������������ǿ�����棩
  * @param  duration: �ܳ���ʱ��(ms)
  */
void BeepContinuousStrong(uint16_t duration);

/**
  * @brief  ��ģ�鴥��
  * @param  vibrator: ��ģ���� (VIBRATOR_1 �� VIBRATOR_2)
  * @param  duration: ����ʱ��(ms)
  */
void Vibrate(uint8_t vibrator, uint16_t duration);

/**
  * @brief  ǿ���񶯣�ͨ������ռ�ձ�ʵ�֣�
  * @param  vibrator: ��ģ���� (VIBRATOR_1 �� VIBRATOR_2)
  * @param  duration: ����ʱ��(ms)
  */
void VibrateStrong(uint8_t vibrator, uint16_t duration);

/**
  * @brief  ���Է���������ģ��
  */
void Haptic_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* __HAPTIC_H */