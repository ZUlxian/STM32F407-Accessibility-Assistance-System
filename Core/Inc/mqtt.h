/**
  ******************************************************************************
  * @file    mqtt.h
  * @brief   MQTT client implementation for ESP8266
  ******************************************************************************
  */

#ifndef MQTT_H
#define MQTT_H

#include "esp8266.h"
#include <stdint.h>
#include <stdlib.h>

// MQTT 配置
#define MQTT_BROKER        ""  // 请在""之间输入MQTT服务器
#define MQTT_PORT          1883              // 标准MQTT端口
#define MQTT_CLIENT_ID     ""    // 请在""之间输入客户端ID
#define MQTT_USERNAME      ""                // 用户名（留空表示不需要认证）
#define MQTT_PASSWORD      ""                // 密码（留空表示不需要认证）

// MQTT 主题
#define MQTT_TOPIC_RESULT      "stm32/result"      // 发送结果消息主题
#define MQTT_TOPIC_COMMAND     "stm32/command"     // 接收命令消息主题

extern const char* MQTT_TOPIC_IMAGE_INFO;      // 发送图片信息消息主题
extern const char* MQTT_TOPIC_IMAGE_DATA;      // 发送图片数据消息主题
extern const char* MQTT_TOPIC_IMAGE_END;       // 图片传输完成标志主题
extern const char* MQTT_TOPIC_STATUS;          // 发送状态信息消息主题
extern const char* MQTT_TOPIC_IMAGE_STATUS;    // 发送图片处理状态信息
extern const char* MQTT_TOPIC_RETRANS_REQUEST; // 接收重传请求
extern const char* MQTT_TOPIC_RETRANS_COMPLETE; // 重传完成确认消息

// 功能函数
uint8_t MQTT_Init(void);
uint8_t MQTT_Connect(void);
uint8_t MQTT_Publish(const char* topic, const char* message);
uint8_t MQTT_PublishBinary(const char* topic, uint8_t* data, uint16_t len);
uint8_t MQTT_Subscribe(const char* topic);
uint8_t MQTT_Check(void);
uint8_t MQTT_Disconnect(void);

// 接收处理函数
void MQTT_ProcessIncomingData(void);
uint8_t MQTT_WaitForResult(char* result_buffer, uint16_t buffer_size, uint32_t timeout);

// 命令处理回调函数
void MQTT_CommandCallback(const char* command);

// 重传处理回调函数
void Integration_HandleRetransmissionRequest(const char* topic, const char* message);

#endif /* MQTT_H */