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

/* WiFi���� */
#define WIFI_SSID       ""       /* ����""֮������WiFi���� */
#define WIFI_PASSWORD   ""     /* ����""֮������WiFi���� */

/* ESP8266��ʼ��״̬���� */
extern uint8_t esp8266_initialized;
extern uint8_t esp8266_connected;

/* ESP8266���ʱ���ü�Ĭ�ϳ�ʱʱ������ */
#define ESP8266_DEFAULT_TIMEOUT  5000
#define ESP8266_LONG_TIMEOUT    10000
#define ESP8266_HTTPS_TIMEOUT   20000  // HTTPS������Ҫ�����ĳ�ʱʱ��

/* ESP8266��ʼ������ */
uint8_t ESP8266_Init(UART_HandleTypeDef *huart);

/* AT����ͺͽ�����غ��� */
uint8_t ESP8266_SendCommand(char* command, char* response, uint32_t timeout);
uint8_t ESP8266_SendData(uint8_t* data, uint16_t len, uint32_t timeout);
uint8_t ESP8266_ReceiveData(uint8_t* buffer, uint16_t max_len, uint32_t timeout);

/* WiFi������Ͽ���غ��� */
uint8_t ESP8266_ConnectToAP(char* ssid, char* password);
uint8_t ESP8266_DisconnectFromAP(void);
uint8_t ESP8266_CheckConnection(void);

/* IP��ַ���ź�ǿ�Ȼ�ȡ���� */
uint8_t ESP8266_GetIP(char* ip_buffer, uint16_t buffer_size);
int8_t ESP8266_GetRSSI(void);

/* TCP/UDP������Ͽ���غ��� */
uint8_t ESP8266_CreateTCPConnection(char* server, uint16_t port);
uint8_t ESP8266_CloseTCPConnection(void);
uint8_t ESP8266_SendTCPData(char* data, uint16_t len);

/* HTTP������غ��� */
uint8_t ESP8266_HTTPGet(char* server, char* path, char* result, uint16_t result_max_len);
uint8_t ESP8266_HTTPPost(char* server, char* path, char* data, char* result, uint16_t result_max_len);

/* HTTPS������غ��� (��ȫ) */
uint8_t ESP8266_HTTPSGet(char* server, char* path, char* result, uint16_t result_max_len);
uint8_t ESP8266_HTTPSPost(char* server, char* path, char* data, char* result, uint16_t result_max_len);

/* ������������ */
void ESP8266_ClearBuffer(void);
uint8_t ESP8266_Restart(void);
uint8_t ESP8266_SetMode(uint8_t mode);
uint8_t ESP8266_GetVersion(char* version, uint16_t version_max_len);

/**
  * @brief  ��ȡ���һ��AT�������Ӧ����
  * @param  buffer: ���ڴ洢��Ӧ�Ļ�����
  * @param  buffer_size: ��������С
  * @retval ���Ƶ��ֽ���
  */
uint16_t ESP8266_GetLastResponse(char* buffer, uint16_t buffer_size);

#endif /* ESP8266_H */
/**
  * @brief  ִ��ESP8266Ӳ����λ
  * @retval None
  */
void ESP8266_HardReset(void);