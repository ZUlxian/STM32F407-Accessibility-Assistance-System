/**
  * @file    haptic.h
  * @brief   STM32直接驱动蜂鸣器和振动模块
  */
  
#ifndef __HAPTIC_H
#define __HAPTIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* 振动模块定义 */
#define VIBRATOR_1              1   // 前方振动模块
#define VIBRATOR_2              2   // 侧面振动模块

/* 引脚定义 - 根据实际连接修改 */
#define BUZZER_PORT             GPIOB
#define BUZZER_PIN              GPIO_PIN_5

#define VIBRATOR1_PORT          GPIOD
#define VIBRATOR1_PIN           GPIO_PIN_0

#define VIBRATOR2_PORT          GPIOD
#define VIBRATOR2_PIN           GPIO_PIN_1

/**
  * @brief  初始化蜂鸣器和振动模块
  */
void Haptic_Init(void);

/**
  * @brief  蜂鸣器发声
  * @param  times: 蜂鸣次数
  */
void Beep(uint8_t times);

/**
  * @brief  短促蜂鸣
  */
void Buzzer_Beep_Short(void);

/**
  * @brief  强力短促蜂鸣（持续时间更长，声音更大）
  */
void Buzzer_Beep_Strong(void);

/**
  * @brief  多次蜂鸣
  * @param  times: 蜂鸣次数
  * @param  interval: 蜂鸣间隔(ms)
  */
void Buzzer_Beep_Times(uint8_t times, uint16_t interval);

/**
  * @brief  多次强力蜂鸣（声音更大）
  * @param  times: 蜂鸣次数
  * @param  interval: 蜂鸣间隔(ms)
  */
void Buzzer_Beep_Times_Strong(uint8_t times, uint16_t interval);

/**
  * @brief  持续蜂鸣
  * @param  duration: 持续时间(ms)
  */
void BeepContinuous(uint16_t duration);

/**
  * @brief  脉冲式持续蜂鸣（增强音量版）
  * @param  duration: 总持续时间(ms)
  */
void BeepContinuousStrong(uint16_t duration);

/**
  * @brief  振动模块触发
  * @param  vibrator: 振动模块编号 (VIBRATOR_1 或 VIBRATOR_2)
  * @param  duration: 持续时间(ms)
  */
void Vibrate(uint8_t vibrator, uint16_t duration);

/**
  * @brief  强力振动（通过更高占空比实现）
  * @param  vibrator: 振动模块编号 (VIBRATOR_1 或 VIBRATOR_2)
  * @param  duration: 持续时间(ms)
  */
void VibrateStrong(uint8_t vibrator, uint16_t duration);

/**
  * @brief  测试蜂鸣器和振动模块
  */
void Haptic_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* __HAPTIC_H */