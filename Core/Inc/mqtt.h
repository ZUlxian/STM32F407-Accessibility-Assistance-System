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

// MQTT ����
#define MQTT_BROKER        ""  // ����""֮������MQTT������
#define MQTT_PORT          1883              // ��׼MQTT�˿�
#define MQTT_CLIENT_ID     ""    // ����""֮������ͻ���ID
#define MQTT_USERNAME      ""                // �û��������ձ�ʾ����Ҫ��֤��
#define MQTT_PASSWORD      ""                // ���루���ձ�ʾ����Ҫ��֤��

// MQTT ����
#define MQTT_TOPIC_RESULT      "stm32/result"      // ���ͽ����Ϣ����
#define MQTT_TOPIC_COMMAND     "stm32/command"     // ����������Ϣ����

extern const char* MQTT_TOPIC_IMAGE_INFO;      // ����ͼƬ��Ϣ��Ϣ����
extern const char* MQTT_TOPIC_IMAGE_DATA;      // ����ͼƬ������Ϣ����
extern const char* MQTT_TOPIC_IMAGE_END;       // ͼƬ������ɱ�־����
extern const char* MQTT_TOPIC_STATUS;          // ����״̬��Ϣ��Ϣ����
extern const char* MQTT_TOPIC_IMAGE_STATUS;    // ����ͼƬ����״̬��Ϣ
extern const char* MQTT_TOPIC_RETRANS_REQUEST; // �����ش�����
extern const char* MQTT_TOPIC_RETRANS_COMPLETE; // �ش����ȷ����Ϣ

// ���ܺ���
uint8_t MQTT_Init(void);
uint8_t MQTT_Connect(void);
uint8_t MQTT_Publish(const char* topic, const char* message);
uint8_t MQTT_PublishBinary(const char* topic, uint8_t* data, uint16_t len);
uint8_t MQTT_Subscribe(const char* topic);
uint8_t MQTT_Check(void);
uint8_t MQTT_Disconnect(void);

// ���մ�����
void MQTT_ProcessIncomingData(void);
uint8_t MQTT_WaitForResult(char* result_buffer, uint16_t buffer_size, uint32_t timeout);

// �����ص�����
void MQTT_CommandCallback(const char* command);

// �ش�����ص�����
void Integration_HandleRetransmissionRequest(const char* topic, const char* message);

#endif /* MQTT_H */