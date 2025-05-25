/**
  * @file    haptic.c
  * @brief   STM32ֱ����������������ģ��
  */
  
#include "haptic.h"

/**
  * @brief  ��ʼ������������ģ��
  */
void Haptic_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // ����GPIOʱ�� - ���GPIODʱ��
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  
  // ��ʼ������������ - ���ģʽ
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);
  
  // ��ʼ״̬Ϊ�͵�ƽ���رգ�
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
  
  // ��ʼ����ģ��1����
  GPIO_InitStruct.Pin = VIBRATOR1_PIN;
  HAL_GPIO_Init(VIBRATOR1_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(VIBRATOR1_PORT, VIBRATOR1_PIN, GPIO_PIN_RESET);
  
  // ��ʼ����ģ��2����
  GPIO_InitStruct.Pin = VIBRATOR2_PIN;
  HAL_GPIO_Init(VIBRATOR2_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(VIBRATOR2_PORT, VIBRATOR2_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  ����������
  * @param  times: ��������
  */
void Beep(uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // ����
    HAL_Delay(100);  // ����100ms
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // �ر�
    
    if (i < times - 1) {
      HAL_Delay(100);  // ���100ms
    }
  }
}

/**
  * @brief  �̴ٷ���
  */
void Buzzer_Beep_Short(void) {
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // ����
  HAL_Delay(50);  // ����50ms
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // �ر�
}

/**
  * @brief  ǿ���̴ٷ���������ʱ���������������
  */
void Buzzer_Beep_Strong(void) {
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // ����
  HAL_Delay(120);  // ����120ms�������ĳ���ʱ��ʹ��������
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // �ر�
}

/**
  * @brief  ��η���
  * @param  times: ��������
  * @param  interval: �������(ms)
  */
void Buzzer_Beep_Times(uint8_t times, uint16_t interval) {
  for (uint8_t i = 0; i < times; i++) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // ����
    HAL_Delay(interval / 2);  // ����ʱ��Ϊ�����һ��
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // �ر�
    
    if (i < times - 1) {
      HAL_Delay(interval / 2);  // ���ʱ��Ϊ�����һ��
    }
  }
}

/**
  * @brief  ���ǿ����������������
  * @param  times: ��������
  * @param  interval: �������(ms)
  */
void Buzzer_Beep_Times_Strong(uint8_t times, uint16_t interval) {
  for (uint8_t i = 0; i < times; i++) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // ����
    HAL_Delay(3 * interval / 4);  // ����ʱ�������ռ�ձ���ߵ�75%
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // �ر�
    
    if (i < times - 1) {
      HAL_Delay(interval / 4);  // ���ʱ�����
    }
  }
}

/**
  * @brief  ��������
  * @param  duration: ����ʱ��(ms)
  */
void BeepContinuous(uint16_t duration) {
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // ����
  HAL_Delay(duration);  // ����ָ��ʱ��
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // �ر�
}

/**
  * @brief  ����ʽ������������ǿ�����棩
  * @param  duration: �ܳ���ʱ��(ms)
  */
void BeepContinuousStrong(uint16_t duration) {
  uint32_t startTime = HAL_GetTick();
  
  // ʹ������ģʽ����ǿ��������������ԭ�����Ƶ���������
  while (HAL_GetTick() - startTime < duration) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // ����
    HAL_Delay(30);  // 30ms��
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // �ر�
    HAL_Delay(10);  // 10ms�أ�����75%��ռ�ձ�
  }
}

/**
  * @brief  ��ģ�鴥��
  * @param  vibrator: ��ģ���� (VIBRATOR_1 �� VIBRATOR_2)
  * @param  duration: ����ʱ��(ms)
  */
void Vibrate(uint8_t vibrator, uint16_t duration) {
  if (vibrator == VIBRATOR_1) {
    HAL_GPIO_WritePin(VIBRATOR1_PORT, VIBRATOR1_PIN, GPIO_PIN_SET);   // ����
    HAL_Delay(duration);  // ����ָ��ʱ��
    HAL_GPIO_WritePin(VIBRATOR1_PORT, VIBRATOR1_PIN, GPIO_PIN_RESET); // �ر�
  } else if (vibrator == VIBRATOR_2) {
    HAL_GPIO_WritePin(VIBRATOR2_PORT, VIBRATOR2_PIN, GPIO_PIN_SET);   // ����
    HAL_Delay(duration);  // ����ָ��ʱ��
    HAL_GPIO_WritePin(VIBRATOR2_PORT, VIBRATOR2_PIN, GPIO_PIN_RESET); // �ر�
  }
}

/**
  * @brief  ǿ���񶯣�ͨ������ռ�ձ�ʵ�֣�
  * @param  vibrator: ��ģ���� (VIBRATOR_1 �� VIBRATOR_2)
  * @param  duration: ����ʱ��(ms)
  */
void VibrateStrong(uint8_t vibrator, uint16_t duration) {
  GPIO_TypeDef* vibrator_port;
  uint16_t vibrator_pin;
  
  if (vibrator == VIBRATOR_1) {
    vibrator_port = VIBRATOR1_PORT;
    vibrator_pin = VIBRATOR1_PIN;
  } else if (vibrator == VIBRATOR_2) {
    vibrator_port = VIBRATOR2_PORT;
    vibrator_pin = VIBRATOR2_PIN;
  } else {
    return;
  }

  uint32_t startTime = HAL_GetTick();
  
  // ʹ�ø���Ƶ������������ǿ��
  while (HAL_GetTick() - startTime < duration) {
    HAL_GPIO_WritePin(vibrator_port, vibrator_pin, GPIO_PIN_SET);   // ����
    HAL_Delay(15);  // 15ms��
    HAL_GPIO_WritePin(vibrator_port, vibrator_pin, GPIO_PIN_RESET); // �ر�
    HAL_Delay(5);   // 5ms�أ�75%ռ�ձȣ�
  }
}

/**
  * @brief  ���Է���������ģ��
  */
void Haptic_Test(void) {
  // ���Է�����
  Beep(2);
  HAL_Delay(500);
  
  // ������ģ��1
  Vibrate(VIBRATOR_1, 300);
  HAL_Delay(500);
  
  // ������ģ��2
  Vibrate(VIBRATOR_2, 300);
}