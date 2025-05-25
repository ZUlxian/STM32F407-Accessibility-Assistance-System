#ifndef HC_SR04_H
#define HC_SR04_H

#include "main.h"

// 前方超声波传感器
#define TRIG1_PORT   GPIOA
#define TRIG1_PIN    GPIO_PIN_7
#define ECHO1_PORT   GPIOB
#define ECHO1_PIN    GPIO_PIN_4

// 侧面超声波传感器
#define TRIG2_PORT   GPIOA
#define TRIG2_PIN    GPIO_PIN_5
#define ECHO2_PORT   GPIOB
#define ECHO2_PIN    GPIO_PIN_7

// 传感器编号
typedef enum {
    HC_SR04_FRONT = 0,
    HC_SR04_SIDE = 1
} HC_SR04_Sensor;

void HC_SR04_Init(void);
float HC_SR04_ReadDistance(HC_SR04_Sensor sensor); // 返回值单位：厘米

#endif /* HC_SR04_H */