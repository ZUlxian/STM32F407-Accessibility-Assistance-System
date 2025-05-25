/**
  ******************************************************************************
  * @file    esp8266.h
  * @brief   Header file for ESP8266 WiFi module with AT firmware
  ******************************************************************************
  */

#ifndef ESP8266_H
#define ESP8266_H

#include "main.h"
#include <stdint.h>

/* WiFi配置 */
#define WIFI_SSID       ""       /* 请在""之间输入WiFi名称 */
#define WIFI_PASSWORD   ""     /* 请在""之间输入WiFi密码 */

/* ESP8266初始化状态变量 */
extern uint8_t esp8266_initialized;
extern uint8_t esp8266_connected;

/* ESP8266命令超时设置及默认超时时间配置 */
#define ESP8266_DEFAULT_TIMEOUT  5000
#define ESP8266_LONG_TIMEOUT    10000
#define ESP8266_HTTPS_TIMEOUT   20000  // HTTPS请求需要更长的超时时间

/* ESP8266初始化函数 */
uint8_t ESP8266_Init(UART_HandleTypeDef *huart);

/* AT命令发送和接收相关函数 */
uint8_t ESP8266_SendCommand(char* command, char* response, uint32_t timeout);
uint8_t ESP8266_SendData(uint8_t* data, uint16_t len, uint32_t timeout);
uint8_t ESP8266_ReceiveData(uint8_t* buffer, uint16_t max_len, uint32_t timeout);

/* WiFi连接与断开相关函数 */
uint8_t ESP8266_ConnectToAP(char* ssid, char* password);
uint8_t ESP8266_DisconnectFromAP(void);
uint8_t ESP8266_CheckConnection(void);

/* IP地址和信号强度获取函数 */
uint8_t ESP8266_GetIP(char* ip_buffer, uint16_t buffer_size);
int8_t ESP8266_GetRSSI(void);

/* TCP/UDP连接与断开相关函数 */
uint8_t ESP8266_CreateTCPConnection(char* server, uint16_t port);
uint8_t ESP8266_CloseTCPConnection(void);
uint8_t ESP8266_SendTCPData(char* data, uint16_t len);

/* HTTP请求相关函数 */
uint8_t ESP8266_HTTPGet(char* server, char* path, char* result, uint16_t result_max_len);
uint8_t ESP8266_HTTPPost(char* server, char* path, char* data, char* result, uint16_t result_max_len);

/* HTTPS请求相关函数 (安全) */
uint8_t ESP8266_HTTPSGet(char* server, char* path, char* result, uint16_t result_max_len);
uint8_t ESP8266_HTTPSPost(char* server, char* path, char* data, char* result, uint16_t result_max_len);

/* 其他辅助函数 */
void ESP8266_ClearBuffer(void);
uint8_t ESP8266_Restart(void);
uint8_t ESP8266_SetMode(uint8_t mode);
uint8_t ESP8266_GetVersion(char* version, uint16_t version_max_len);

/**
  * @brief  获取最近一次AT命令的响应内容
  * @param  buffer: 用于存储响应的缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval 复制的字节数
  */
uint16_t ESP8266_GetLastResponse(char* buffer, uint16_t buffer_size);

#endif /* ESP8266_H */
/**
  * @brief  执行ESP8266硬件复位
  * @retval None
  */
void ESP8266_HardReset(void);