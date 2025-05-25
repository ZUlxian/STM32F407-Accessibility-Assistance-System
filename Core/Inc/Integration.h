/**
  ******************************************************************************
  * @file    Integration.h
  * @brief   基于MQTT的图像识别集成（接口版本）
  ******************************************************************************
  */

#ifndef INTEGRATION_H
#define INTEGRATION_H

#include "main.h"
#include "esp8266.h"
#include "mqtt.h"
#include "ff.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern const char* MQTT_TOPIC_IMAGE_INFO;
extern const char* MQTT_TOPIC_IMAGE_DATA;
extern const char* MQTT_TOPIC_IMAGE_END;
extern const char* MQTT_TOPIC_STATUS;
extern const char* MQTT_TOPIC_IMAGE_STATUS;
extern const char* MQTT_TOPIC_RETRANS_REQUEST;
extern const char* MQTT_TOPIC_RETRANS_COMPLETE;

// 重传请求相关的变量
extern volatile uint8_t retransmission_requested;
extern char retransmission_chunks[512];

// Base64编码表
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// 功能函数
uint8_t Integration_Init(void);
uint8_t Integration_SendImage(const char* filename);
uint8_t Integration_WaitForResult(char* result_buffer, uint16_t buffer_size, uint32_t timeout);
uint8_t Integration_PublishStatus(const char* status_message);
uint32_t Base64_Encode(uint8_t* data, uint32_t data_len, char* output, uint32_t output_max);
void Integration_ProcessCommands(void);
void Integration_HandleRetransmissionRequest(const char* topic, const char* message);

// 硬件重置函数
void ESP8266_HardReset(void);

#endif /* INTEGRATION_H */