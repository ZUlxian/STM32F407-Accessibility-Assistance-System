/**
  * @file    haptic.c
  * @brief   STM32直接驱动蜂鸣器和振动模块
  */
  
#include "haptic.h"

/**
  * @brief  初始化蜂鸣器和振动模块
  */
void Haptic_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // 启用GPIO时钟 - 添加GPIOD时钟
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  
  // 初始化蜂鸣器引脚 - 输出模式
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);
  
  // 初始状态为低电平（关闭）
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
  
  // 初始化振动模块1引脚
  GPIO_InitStruct.Pin = VIBRATOR1_PIN;
  HAL_GPIO_Init(VIBRATOR1_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(VIBRATOR1_PORT, VIBRATOR1_PIN, GPIO_PIN_RESET);
  
  // 初始化振动模块2引脚
  GPIO_InitStruct.Pin = VIBRATOR2_PIN;
  HAL_GPIO_Init(VIBRATOR2_PORT, &GPIO_InitStruct);
  HAL_GPIO_WritePin(VIBRATOR2_PORT, VIBRATOR2_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  蜂鸣器发声
  * @param  times: 蜂鸣次数
  */
void Beep(uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // 开启
    HAL_Delay(100);  // 蜂鸣100ms
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // 关闭
    
    if (i < times - 1) {
      HAL_Delay(100);  // 间隔100ms
    }
  }
}

/**
  * @brief  短促蜂鸣
  */
void Buzzer_Beep_Short(void) {
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // 开启
  HAL_Delay(50);  // 蜂鸣50ms
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // 关闭
}

/**
  * @brief  强力短促蜂鸣（持续时间更长，声音更大）
  */
void Buzzer_Beep_Strong(void) {
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // 开启
  HAL_Delay(120);  // 蜂鸣120ms，更长的持续时间使声音更大
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // 关闭
}

/**
  * @brief  多次蜂鸣
  * @param  times: 蜂鸣次数
  * @param  interval: 蜂鸣间隔(ms)
  */
void Buzzer_Beep_Times(uint8_t times, uint16_t interval) {
  for (uint8_t i = 0; i < times; i++) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // 开启
    HAL_Delay(interval / 2);  // 蜂鸣时间为间隔的一半
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // 关闭
    
    if (i < times - 1) {
      HAL_Delay(interval / 2);  // 间隔时间为间隔的一半
    }
  }
}

/**
  * @brief  多次强力蜂鸣（声音更大）
  * @param  times: 蜂鸣次数
  * @param  interval: 蜂鸣间隔(ms)
  */
void Buzzer_Beep_Times_Strong(uint8_t times, uint16_t interval) {
  for (uint8_t i = 0; i < times; i++) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // 开启
    HAL_Delay(3 * interval / 4);  // 蜂鸣时间更长，占空比提高到75%
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // 关闭
    
    if (i < times - 1) {
      HAL_Delay(interval / 4);  // 间隔时间更短
    }
  }
}

/**
  * @brief  持续蜂鸣
  * @param  duration: 持续时间(ms)
  */
void BeepContinuous(uint16_t duration) {
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // 开启
  HAL_Delay(duration);  // 持续指定时间
  HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // 关闭
}

/**
  * @brief  脉冲式持续蜂鸣（增强音量版）
  * @param  duration: 总持续时间(ms)
  */
void BeepContinuousStrong(uint16_t duration) {
  uint32_t startTime = HAL_GetTick();
  
  // 使用脉冲模式来增强音量，但保持与原来相似的声音特性
  while (HAL_GetTick() - startTime < duration) {
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);   // 开启
    HAL_Delay(30);  // 30ms开
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET); // 关闭
    HAL_Delay(10);  // 10ms关，保持75%的占空比
  }
}

/**
  * @brief  振动模块触发
  * @param  vibrator: 振动模块编号 (VIBRATOR_1 或 VIBRATOR_2)
  * @param  duration: 持续时间(ms)
  */
void Vibrate(uint8_t vibrator, uint16_t duration) {
  if (vibrator == VIBRATOR_1) {
    HAL_GPIO_WritePin(VIBRATOR1_PORT, VIBRATOR1_PIN, GPIO_PIN_SET);   // 开启
    HAL_Delay(duration);  // 持续指定时间
    HAL_GPIO_WritePin(VIBRATOR1_PORT, VIBRATOR1_PIN, GPIO_PIN_RESET); // 关闭
  } else if (vibrator == VIBRATOR_2) {
    HAL_GPIO_WritePin(VIBRATOR2_PORT, VIBRATOR2_PIN, GPIO_PIN_SET);   // 开启
    HAL_Delay(duration);  // 持续指定时间
    HAL_GPIO_WritePin(VIBRATOR2_PORT, VIBRATOR2_PIN, GPIO_PIN_RESET); // 关闭
  }
}

/**
  * @brief  强力振动（通过更高占空比实现）
  * @param  vibrator: 振动模块编号 (VIBRATOR_1 或 VIBRATOR_2)
  * @param  duration: 持续时间(ms)
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
  
  // 使用更高频率脉冲增加振动强度
  while (HAL_GetTick() - startTime < duration) {
    HAL_GPIO_WritePin(vibrator_port, vibrator_pin, GPIO_PIN_SET);   // 开启
    HAL_Delay(15);  // 15ms开
    HAL_GPIO_WritePin(vibrator_port, vibrator_pin, GPIO_PIN_RESET); // 关闭
    HAL_Delay(5);   // 5ms关（75%占空比）
  }
}

/**
  * @brief  测试蜂鸣器和振动模块
  */
void Haptic_Test(void) {
  // 测试蜂鸣器
  Beep(2);
  HAL_Delay(500);
  
  // 测试振动模块1
  Vibrate(VIBRATOR_1, 300);
  HAL_Delay(500);
  
  // 测试振动模块2
  Vibrate(VIBRATOR_2, 300);
}