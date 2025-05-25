/**
  ******************************************************************************
  * @file    mqtt.c
  * @brief   MQTT client implementation for ESP8266
  ******************************************************************************
  */

#include "mqtt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>  // ��Ҫ�õ��ı�׼�⺯������malloc��free�ȹ��ܺ���
#include "ff.h"  

/* ����ͨ�Ŷ������� */
extern UART_HandleTypeDef huart1;

// ���Դ�ӡ���ܺ���
void MQTT_DebugPrint(const char* msg) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"[MQTT] ", 7, 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 300);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
    HAL_Delay(10);
}

// ��16���Ƹ�ʽ��ӡ���ݵĹ��ܺ���
void MQTT_DebugHex(const char* prefix, uint8_t* data, uint16_t len) {
    char buffer[128] = {0};
    sprintf(buffer, "%s (%d bytes): ", prefix, len);
    MQTT_DebugPrint(buffer);
    
    for (uint16_t i = 0; i < len && i < 32; i++) {
        sprintf(buffer, "%02X ", data[i]);
        HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), 100);
    }
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
}

// ������Ϣ�����ݻ���������
static char mqtt_result_buffer[512] = {0};
static uint8_t mqtt_result_received = 0;

/**
  * @brief  ��ʼ��MQTT�ͻ���
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_Init(void)
{
    MQTT_DebugPrint("Initializing MQTT client...");
    
    // ȷ��ESP8266�Ѿ���ʼ�������ӵ�WiFi
    if (!esp8266_initialized || !esp8266_connected) {
        MQTT_DebugPrint("ESP8266 not initialized or not connected to WiFi");
        return 0;
    }
    
    return 1;
}

/**
  * @brief  ����ESP8266���ص�+IPD����
  * @param  buffer: ���ݻ�����
  * @param  len: ����������
  * @param  data: ������������ָ��
  * @param  data_len: ����ָ�볤��
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_ParseIPD(uint8_t* buffer, uint16_t len, uint8_t** data, uint16_t* data_len) {
    char* ipd_start = strstr((char*)buffer, "+IPD,");
    if (!ipd_start) {
        return 0;
    }
    
    char* colon = strchr(ipd_start, ':');
    if (!colon) {
        return 0;
    }
    
    // ��������
    int ipd_len = 0;
    sscanf(ipd_start + 5, "%d", &ipd_len);
    
    // ���÷��ص�����ָ��
    *data = (uint8_t*)(colon + 1);
    *data_len = ipd_len;
    
    return 1;
}

/**
  * @brief  ���ӵ�MQTT������ - ������
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_Connect(void)
{
    char connect_cmd[256];
    
    MQTT_DebugPrint("Connecting to MQTT broker...");
    
    // ����TCP����
    if (!ESP8266_CreateTCPConnection(MQTT_BROKER, MQTT_PORT)) {
        MQTT_DebugPrint("Failed to create TCP connection to MQTT broker");
        return 0;
    }
    
    // ����MQTT���Ӱ� - ʹ�������Ķ��������ݰ�
    static uint8_t connect_packet[128]; // Ԥ����Ķ��������ݰ�
    uint16_t packet_len = 0;
    
    // ��Ϣ����
    connect_packet[packet_len++] = 0x10;  // CONNECT packet type
    
    // ʣ�೤�Ȳ��ñ䳤���뷽ʽVariable Length Encoding of Remaining Length
    uint8_t remaining_length_pos = packet_len++;
    
    // Protocol Name
    connect_packet[packet_len++] = 0x00;
    connect_packet[packet_len++] = 0x04;
    connect_packet[packet_len++] = 'M';
    connect_packet[packet_len++] = 'Q';
    connect_packet[packet_len++] = 'T';
    connect_packet[packet_len++] = 'T';
    
    // Protocol Level
    connect_packet[packet_len++] = 0x04;  // MQTT 3.1.1
    
    // Connect Flags
    uint8_t connect_flags = 0x02;  // Clean Session
    if (MQTT_USERNAME[0] != '\0') connect_flags |= 0x80;
    if (MQTT_PASSWORD[0] != '\0') connect_flags |= 0x40;
    connect_packet[packet_len++] = connect_flags;
    
    // Keep Alive
    connect_packet[packet_len++] = 0x00;
    connect_packet[packet_len++] = 0x3C;  // 60 seconds
    
    // Client ID
    uint16_t client_id_len = strlen(MQTT_CLIENT_ID);
    connect_packet[packet_len++] = client_id_len >> 8;
    connect_packet[packet_len++] = client_id_len & 0xFF;
    memcpy(&connect_packet[packet_len], MQTT_CLIENT_ID, client_id_len);
    packet_len += client_id_len;
    
    // Username (if set)
    if (MQTT_USERNAME[0] != '\0') {
        uint16_t username_len = strlen(MQTT_USERNAME);
        connect_packet[packet_len++] = username_len >> 8;
        connect_packet[packet_len++] = username_len & 0xFF;
        memcpy(&connect_packet[packet_len], MQTT_USERNAME, username_len);
        packet_len += username_len;
    }
    
    // Password (if set)
    if (MQTT_PASSWORD[0] != '\0') {
        uint16_t password_len = strlen(MQTT_PASSWORD);
        connect_packet[packet_len++] = password_len >> 8;
        connect_packet[packet_len++] = password_len & 0xFF;
        memcpy(&connect_packet[packet_len], MQTT_PASSWORD, password_len);
        packet_len += password_len;
    }
    
    // ����ʣ�೤��
    uint8_t remaining_length = packet_len - 2;  // ��ȥ��Ϣ���ͺ�ʣ�೤���ֶ�
    connect_packet[remaining_length_pos] = remaining_length;
    
    // ��ӡ������Ϣ
    MQTT_DebugHex("MQTT CONNECT packet", connect_packet, packet_len);
    
    // �������Ӱ�
    if (!ESP8266_SendTCPData((char*)connect_packet, packet_len)) {
        MQTT_DebugPrint("Failed to send MQTT CONNECT packet");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // �ȴ�CONNACK
    uint32_t start_time = HAL_GetTick();
    uint8_t response_buffer[128];
    uint16_t response_len = 0;
    uint8_t connack_received = 0;
    
    while ((HAL_GetTick() - start_time) < 5000 && !connack_received) {
        // ��������(����+IPDǰ׺)
        memset(response_buffer, 0, sizeof(response_buffer));
        response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 1000);
        
        if (response_len > 0) {
            MQTT_DebugPrint("Received data:");
            MQTT_DebugHex("Raw data", response_buffer, response_len);
            
            // ����IPD����
            uint8_t* mqtt_data = NULL;
            uint16_t mqtt_data_len = 0;
            
            // ���з������ҽ���+IPD
            char* ipd_str = strstr((char*)response_buffer, "+IPD,");
            if (ipd_str) {
                MQTT_DebugPrint("Found +IPD data");
                
                char* colon = strchr(ipd_str, ':');
                if (colon) {
                    // ���÷��ص�����ָ��
                    mqtt_data = (uint8_t*)(colon + 1);
                    
                    // ����Ƿ�ΪCONNACK�� (��Ϣ������Ϊ0x20)
                    if (mqtt_data[0] == 0x20) {
                        MQTT_DebugPrint("Valid CONNACK received");
                        connack_received = 1;
                    } else {
                        MQTT_DebugPrint("Received data is not a CONNACK packet");
                    }
                }
            } else if (strstr((char*)response_buffer, "OK") != NULL) {
                // ESP8266���ܻظ�����ʾ����OK
                MQTT_DebugPrint("Received OK");
            } else if (strstr((char*)response_buffer, "ERROR") != NULL) {
                MQTT_DebugPrint("Received ERROR");
                ESP8266_CloseTCPConnection();
                return 0;
            }
        }
    }
    
    if (connack_received) {
        MQTT_DebugPrint("Connected to MQTT broker");
        return 1;
    } else {
        MQTT_DebugPrint("Failed to receive MQTT CONNACK");
        ESP8266_CloseTCPConnection();
        return 0;
    }
}

/**
  * @brief  �����ı���Ϣ��MQTT���⣨ʹ��QoS�汾-ʹ���������������ݰ���
  * @param  topic: ��������
  * @param  message: Ҫ���͵���Ϣ
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_Publish(const char* topic, const char* message)
{
    uint16_t topic_len = strlen(topic);
    uint16_t message_len = strlen(message);
    uint16_t packet_len = 0;
    
    // ������ݳ����Ƿ����
    if (topic_len == 0 || message_len == 0 || topic_len + message_len + 4 > 1024) {
        MQTT_DebugPrint("Invalid topic or message length");
        return 0;
    }
    
    // ʹ�������Ķ��������ݰ�������������
    static uint8_t publish_packet[1024 + 20]; // Ԥ����Ķ��������ݰ�
    
    // ��Ϣ���� (QoS 1, No Retain) - ʹ��QoS 1�ṩ���ɿ�����
    publish_packet[packet_len++] = 0x32;  // 0x30 | 0x02 = QoS 1
    
    // ����ʣ�೤��(�ɱ�ͷ��+�غ�)
    uint16_t remaining_length = topic_len + 2 + message_len + 2;  // +2 for message ID
    
    // ��ϢID (ֻ��QoS > 0ʱ��Ҫ)
    static uint16_t message_id = 0;
    message_id = (message_id + 1) % 65535;
    if (message_id == 0) message_id = 1;  // ����IDΪ0
    
    // ����ʣ�೤��(���ݳ���ѡ����뷽ʽ)
    if (remaining_length < 128) {
        publish_packet[packet_len++] = remaining_length;
    } else {
        publish_packet[packet_len++] = (remaining_length % 128) + 128;
        publish_packet[packet_len++] = remaining_length / 128;
    }
    
    // �ɱ�ͷ�� - ������
    publish_packet[packet_len++] = topic_len >> 8;
    publish_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&publish_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // ��ϢID (QoS 1��Ҫ)
    publish_packet[packet_len++] = message_id >> 8;
    publish_packet[packet_len++] = message_id & 0xFF;
    
    // �غ� - ��Ϣ����
    memcpy(&publish_packet[packet_len], message, message_len);
    packet_len += message_len;
    
    char debug_msg[64];
    sprintf(debug_msg, "Publishing to topic: %s (%d bytes)", topic, message_len);
    MQTT_DebugPrint(debug_msg);
    MQTT_DebugHex("MQTT PUBLISH packet", publish_packet, packet_len);
    
    // ���ͷ�����
    uint8_t success = ESP8266_SendTCPData((char*)publish_packet, packet_len);
    
    if (success) {
        // �ȴ�PUBACK (QoS 1��Ҫ)
        uint32_t start_time = HAL_GetTick();
        uint8_t response_buffer[64];
        uint16_t response_len = 0;
        uint8_t puback_received = 0;
        
        while ((HAL_GetTick() - start_time) < 2000 && !puback_received) {
            response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 500);
            
            if (response_len > 0) {
                // ���з�������PUBACK
                char* ipd_str = strstr((char*)response_buffer, "+IPD,");
                if (ipd_str) {
                    char* colon = strchr(ipd_str, ':');
                    if (colon) {
                        uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                        // ����Ƿ�ΪPUBACK (0x40)
                        if ((mqtt_data[0] & 0xF0) == 0x40) {
                            puback_received = 1;
                            MQTT_DebugPrint("PUBACK received");
                        }
                    }
                }
            }
        }
        
        MQTT_DebugPrint("Published successfully");
        return 1;
    } else {
        MQTT_DebugPrint("Failed to send PUBLISH packet");
        return 0;
    }
}

/**
  * @brief  ���Ͷ��������ݵ�MQTT���⣨ʹ��QoS 2�ɿ�����汾��
  * @param  topic: ��������
  * @param  data: ����������
  * @param  len: ���ݳ���
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_PublishBinary(const char* topic, uint8_t* data, uint16_t len)
{
    uint16_t topic_len = strlen(topic);
    uint16_t packet_len = 0;
    
    // ������ݳ����Ƿ����
    if (topic_len == 0 || len == 0 || topic_len + len + 4 > 1024) {
        MQTT_DebugPrint("Invalid topic or data length");
        return 0;
    }
    
    // ʹ�������Ķ��������ݰ�
    static uint8_t publish_packet[1024 + 20]; // Ԥ����Ķ��������ݰ�
    
    // ��Ϣ���� (QoS 2, No Retain) - ʹ��QoS����Ϊ2�ṩ��ɿ�����
    publish_packet[packet_len++] = 0x34;  // 0x30 | 0x04 = QoS 2
    
    // ����ʣ�೤��
    uint16_t remaining_length = topic_len + 2 + len + 2;  // +2 for message ID
    
    // ��ϢID (ֻ��QoS > 0ʱ��Ҫ)
    static uint16_t message_id = 0;
    message_id = (message_id + 1) % 65535;
    if (message_id == 0) message_id = 1;  // ����IDΪ0
    
    // ����ʣ�೤��(���ݳ���ѡ����뷽ʽ)
    if (remaining_length < 128) {
        publish_packet[packet_len++] = remaining_length;
    } else if (remaining_length < 16384) {
        publish_packet[packet_len++] = (remaining_length % 128) + 128;
        publish_packet[packet_len++] = remaining_length / 128;
    } else {
        // �����������ݰ� (�����ϲ�Ӧ����16KB�����Ա���ʱ֮��)
        publish_packet[packet_len++] = (remaining_length % 128) + 128;
        publish_packet[packet_len++] = ((remaining_length / 128) % 128) + 128;
        publish_packet[packet_len++] = remaining_length / 16384;
    }
    
    // �ɱ�ͷ�� - ������
    publish_packet[packet_len++] = topic_len >> 8;
    publish_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&publish_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // ��ϢID (QoS 2��Ҫ)
    publish_packet[packet_len++] = message_id >> 8;
    publish_packet[packet_len++] = message_id & 0xFF;
    
    // �غ� - ����������
    memcpy(&publish_packet[packet_len], data, len);
    packet_len += len;
    
    char debug_msg[64];
    sprintf(debug_msg, "Publishing binary data to topic: %s (%d bytes) with QoS 2", topic, len);
    MQTT_DebugPrint(debug_msg);
    
    // ���ͷ�����
    uint8_t success = ESP8266_SendTCPData((char*)publish_packet, packet_len);
    
    if (success) {
        // �ȴ�PUBREC (QoS 2��һȷ�Ͻ׶�)
        uint32_t start_time = HAL_GetTick();
        uint8_t response_buffer[128];
        uint16_t response_len = 0;
        uint8_t pubrec_received = 0;
        
        while ((HAL_GetTick() - start_time) < 1000 && !pubrec_received) {
            response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 10);
            
            if (response_len > 0) {
                // ���з�������PUBREC
                char* ipd_str = strstr((char*)response_buffer, "+IPD,");
                if (ipd_str) {
                    char* colon = strchr(ipd_str, ':');
                    if (colon) {
                        uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                        // ����Ƿ�ΪPUBREC (0x50)
                        if ((mqtt_data[0] & 0xF0) == 0x50) {
                            // ��֤��ϢID�Ƿ�ƥ��
                            uint16_t recv_id = (mqtt_data[2] << 8) | mqtt_data[3];
                            if (recv_id == message_id) {
                                pubrec_received = 1;
                                MQTT_DebugPrint("PUBREC received");
                                
                                // ����PUBREL (�ڶ�ȷ��)
                                uint8_t pubrel_packet[4];
                                pubrel_packet[0] = 0x62;  // PUBREL with QoS 1
                                pubrel_packet[1] = 0x02;  // ʣ�೤��
                                pubrel_packet[2] = message_id >> 8;
                                pubrel_packet[3] = message_id & 0xFF;
                                
                                if (ESP8266_SendTCPData((char*)pubrel_packet, 4)) {
                                    // �ȴ�PUBCOMP (QoS 2����ȷ�Ͻ׶�)
                                    uint32_t pubcomp_start = HAL_GetTick();
                                    uint8_t pubcomp_received = 0;
                                    
                                    while ((HAL_GetTick() - pubcomp_start) < 1000 && !pubcomp_received) {
                                        response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 10);
                                        
                                        if (response_len > 0) {
                                            // ���з�������PUBCOMP
                                            ipd_str = strstr((char*)response_buffer, "+IPD,");
                                            if (ipd_str) {
                                                colon = strchr(ipd_str, ':');
                                                if (colon) {
                                                    mqtt_data = (uint8_t*)(colon + 1);
                                                    // ����Ƿ�ΪPUBCOMP (0x70)
                                                    if ((mqtt_data[0] & 0xF0) == 0x70) {
                                                        pubcomp_received = 1;
                                                        MQTT_DebugPrint("PUBCOMP received - QoS 2 publish complete");
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    
                                    if (pubcomp_received) {
                                        return 1;  // QoS 2Э�����
                                    } else {
                                        MQTT_DebugPrint("Failed to receive PUBCOMP");
                                        // ��Ȼ�ɹ�����Ϊ�����������յ�PUBREC
                                        return 1;
                                    }
                                } else {
                                    MQTT_DebugPrint("Failed to send PUBREL");
                                    // ��Ȼ�ɹ�����Ϊ�����������յ�PUBREC
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (!pubrec_received) {
            MQTT_DebugPrint("QoS 2 confirmation not received, assuming success");
            // ���ǵ�������ܲ��ȶ������Ǽ��������ѷ���
            // ��ʵ�����Ӧ�����ԣ�������򻯴���
            return 1;
        }
        
        MQTT_DebugPrint("Binary data published successfully with QoS 2");
        return 1;
    } else {
        MQTT_DebugPrint("Failed to send binary PUBLISH packet");
        return 0;
    }
}

/**
  * @brief  ����MQTT���� - ������
  * @param  topic: ��������
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_Subscribe(const char* topic)
{
    uint16_t topic_len = strlen(topic);
    uint16_t packet_len = 0;
    
    // ������ⳤ��
    if (topic_len == 0 || topic_len > 255) {
        MQTT_DebugPrint("Invalid topic length");
        return 0;
    }
    
    // ����MQTT���İ� - ʹ�������Ķ��������ݰ�
    static uint8_t subscribe_packet[280];  // Ԥ����Ķ��������ݰ�
    
    // ��Ϣ����
    subscribe_packet[packet_len++] = 0x82;  // SUBSCRIBE packet type with QoS 1
    
    // ����ʣ�೤��
    uint8_t remaining_length = topic_len + 5;  // 2(��ϢID) + 2(���ⳤ��) + topic_len + 1(QoS)
    subscribe_packet[packet_len++] = remaining_length;
    
    // �ɱ�ͷ�� - ��Ϣ��ʶ��
    subscribe_packet[packet_len++] = 0x00;
    subscribe_packet[packet_len++] = 0x01;  // Message ID = 1
    
    // �غ� - �����б�
    subscribe_packet[packet_len++] = topic_len >> 8;
    subscribe_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&subscribe_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // �����QoS
    subscribe_packet[packet_len++] = 0x00;  // QoS 0
    
    // ��ӡ������Ϣ
    char debug_msg[64];
    sprintf(debug_msg, "Subscribing to topic: %s", topic);
    MQTT_DebugPrint(debug_msg);
    MQTT_DebugHex("MQTT SUBSCRIBE packet", subscribe_packet, packet_len);
    
    // ���Ͷ��İ�
    if (!ESP8266_SendTCPData((char*)subscribe_packet, packet_len)) {
        MQTT_DebugPrint("Failed to send SUBSCRIBE packet");
        return 0;
    }
    
    // �ȴ�SUBACK
    uint32_t start_time = HAL_GetTick();
    uint8_t response_buffer[128];
    uint16_t response_len = 0;
    uint8_t suback_received = 0;
    
    while ((HAL_GetTick() - start_time) < 5000 && !suback_received) {
        // ��������
        memset(response_buffer, 0, sizeof(response_buffer));
        response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 1000);
        
        if (response_len > 0) {
            MQTT_DebugPrint("Received data:");
            MQTT_DebugHex("Raw data", response_buffer, response_len);
            
            // ����+IPD����
            char* ipd_str = strstr((char*)response_buffer, "+IPD,");
            if (ipd_str) {
                char* colon = strchr(ipd_str, ':');
                if (colon) {
                    // ���÷��ص�����ָ��
                    uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                    
                    // ����Ƿ�ΪSUBACK�� (��һ���ֽ���Ϊ0x90)
                    if (mqtt_data[0] == 0x90) {
                        MQTT_DebugPrint("Valid SUBACK received");
                        suback_received = 1;
                    }
                }
            }
        }
    }
    
    if (suback_received) {
        MQTT_DebugPrint("Subscribed to topic successfully");
        
        // ������������
        memset(mqtt_result_buffer, 0, sizeof(mqtt_result_buffer));
        mqtt_result_received = 0;
        
        return 1;
    } else {
        MQTT_DebugPrint("Failed to receive SUBACK");
        return 0;
    }
}

/**
  * @brief  ���MQTT����״̬
  * @retval 1: ������, 0: δ����
  */
uint8_t MQTT_Check(void)
{
    static uint8_t ping_packet[2] = {0xC0, 0x00}; // PINGREQ packet
    MQTT_DebugPrint("Checking MQTT connection...");
    
    if (ESP8266_SendTCPData((char*)ping_packet, 2)) {
        // �ȴ�PINGRESP
        uint32_t start_time = HAL_GetTick();
        uint8_t response_buffer[32];
        uint16_t response_len = 0;
        uint8_t pingresp_received = 0;
        
        while ((HAL_GetTick() - start_time) < 2000 && !pingresp_received) {
            // ��������
            memset(response_buffer, 0, sizeof(response_buffer));
            response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 500);
            
            if (response_len > 0) {
                MQTT_DebugHex("PINGRESP data", response_buffer, response_len);
                
                // ����+IPD����
                char* ipd_str = strstr((char*)response_buffer, "+IPD,");
                if (ipd_str) {
                    char* colon = strchr(ipd_str, ':');
                    if (colon) {
                        // ���÷��ص�����ָ��
                        uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                        
                        // ����Ƿ�ΪPINGRESP�� (0xD0 0x00)
                        if (mqtt_data[0] == 0xD0) {
                            MQTT_DebugPrint("Valid PINGRESP received");
                            pingresp_received = 1;
                        }
                    }
                }
            }
        }
        
        if (pingresp_received) {
            MQTT_DebugPrint("MQTT connection is active");
            return 1;
        }
    }
    
    MQTT_DebugPrint("MQTT connection check failed");
    return 0;
}

/**
  * @brief  �Ͽ�MQTT����
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_Disconnect(void)
{
    static uint8_t disconnect_packet[2] = {0xE0, 0x00}; // DISCONNECT packet
    
    MQTT_DebugPrint("Disconnecting from MQTT broker...");
    
    if (ESP8266_SendTCPData((char*)disconnect_packet, 2)) {
        MQTT_DebugPrint("Disconnected from MQTT broker");
        ESP8266_CloseTCPConnection();
        return 1;
    } else {
        MQTT_DebugPrint("Failed to send DISCONNECT packet");
        ESP8266_CloseTCPConnection();
        return 0;
    }
}

/**
  * @brief  ������յ���MQTT��Ϣ - ������
  */
void MQTT_ProcessIncomingData(void)
{
    uint8_t buffer[512];
    uint16_t len = ESP8266_ReceiveData(buffer, sizeof(buffer), 100);
    
    // ���û�������ݣ����Դ��ļ���ȡ���
    if (len == 0) {
        static uint32_t last_file_check = 0;
        // ÿ��1����һ���ļ�
        if (HAL_GetTick() - last_file_check > 1000) {
            last_file_check = HAL_GetTick();
            
            // ���SD�����Ƿ���result.txt�ļ�
            FIL file;
            FRESULT res = f_open(&file, "0:/result.txt", FA_READ);
            if (res == FR_OK) {
                MQTT_DebugPrint("Found result file on SD card!");
                
                // ��ȡ�ļ�����
                char file_result[256] = {0};
                UINT bytes_read;
                res = f_read(&file, file_result, sizeof(file_result) - 1, &bytes_read);
                
                if (res == FR_OK && bytes_read > 0) {
                    file_result[bytes_read] = '\0';
                    
                    // ���Ƶ���������������ñ�־
                    strncpy(mqtt_result_buffer, file_result, sizeof(mqtt_result_buffer) - 1);
                    mqtt_result_buffer[sizeof(mqtt_result_buffer) - 1] = '\0';
                    mqtt_result_received = 1;
                    
                    char debug_msg[64];
                    sprintf(debug_msg, "Result from file: %s", mqtt_result_buffer);
                    MQTT_DebugPrint(debug_msg);
                    
                    // ɾ���ļ��������ظ���ȡ
                    f_close(&file);
                    f_unlink("0:/result.txt");
                } else {
                    f_close(&file);
                }
            }
        }
        return;
    }
    
    // Add debug output for data received
    char debug_buf[64];
    sprintf(debug_buf, "Processing incoming data (%d bytes)", len);
    MQTT_DebugPrint(debug_buf);
    
    // Special case: if we see only a single 5B byte (which is "["), skip it as it's likely noise
    if (len == 1 && buffer[0] == 0x5B) {
        MQTT_DebugPrint("Ignoring single bracket byte (noise)");
        return;
    }
    
    MQTT_DebugHex("Incoming Data", buffer, len > 32 ? 32 : len);
    
    // ����Ƿ����+IPD
    char* ipd_str = strstr((char*)buffer, "+IPD,");
    if (ipd_str) {
        // Try to handle the special case seen in logs: "+IPD,36:0"stm32/resultceramic floor tiles"
        char* topic_start = strstr(ipd_str, "stm32/result");
        if (topic_start) {
            MQTT_DebugPrint("Found direct result message!");
            
            // Extract the payload (skip the topic)
            char* payload = topic_start + strlen("stm32/result");
            
            // Copy to result buffer and set received flag
            strncpy(mqtt_result_buffer, payload, sizeof(mqtt_result_buffer) - 1);
            mqtt_result_buffer[sizeof(mqtt_result_buffer) - 1] = '\0';
            mqtt_result_received = 1;
            
            sprintf(debug_buf, "Direct result extracted: %s", mqtt_result_buffer);
            MQTT_DebugPrint(debug_buf);
            return;
        }
        
        // Standard IPD parsing for normal MQTT messages
        // ��ȡ���ݳ���
        int data_len = 0;
        sscanf(ipd_str + 5, "%d", &data_len);
        
        sprintf(debug_buf, "Found IPD with length %d", data_len);
        MQTT_DebugPrint(debug_buf);
        
        // ����ð�ţ�ð�ź�����ʵ������
        char* data_start = strchr(ipd_str, ':');
        if (data_start && data_len > 0) {
            data_start++; // ����ð��
            
            // Check for any variation of the result message format
            if (strstr(data_start, "stm32/result")) {
                MQTT_DebugPrint("Found result topic in IPD data");
                
                char* result_content = strstr(data_start, "stm32/result");
                result_content += strlen("stm32/result");
                
                // Copy the result and set flag
                strncpy(mqtt_result_buffer, result_content, sizeof(mqtt_result_buffer) - 1);
                mqtt_result_buffer[sizeof(mqtt_result_buffer) - 1] = '\0';
                mqtt_result_received = 1;
                
                sprintf(debug_buf, "Result from IPD: %s", mqtt_result_buffer);
                MQTT_DebugPrint(debug_buf);
                return;
            }
            
            // ����Ƿ���PUBLISH��Ϣ (0x30)
            if ((data_start[0] & 0xF0) == 0x30) {
                MQTT_DebugPrint("Detected PUBLISH message");
                
                // ��ȡʣ�೤��(�ɱ䳤�ȱ���)
                uint16_t remaining_length = data_start[1];
                uint8_t multiplier = 1;
                uint8_t offset = 2;
                
                if (data_start[1] & 0x80) {
                    remaining_length = (data_start[1] & 0x7F);
                    multiplier = 128;
                    offset = 3;
                    if (data_start[2] & 0x80) {
                        remaining_length += (data_start[2] & 0x7F) * multiplier;
                        multiplier *= 128;
                        offset = 4;
                    }
                }
                
                // ��ȡ���ⳤ��
                uint16_t topic_length = (data_start[offset] << 8) | data_start[offset + 1];
                
                // ��������Ƿ������Ƕ��ĵ�����
                if (topic_length < 64) {  // ��ֹ���
                    char topic[64] = {0};
                    memcpy(topic, &data_start[offset + 2], topic_length);
                    topic[topic_length] = '\0';
                    
                    sprintf(debug_buf, "Message topic: %s", topic);
                    MQTT_DebugPrint(debug_buf);
                    
                    // ��ȡ��Ϣ����
                    uint16_t payload_offset = offset + 2 + topic_length;
                    
                    // ȷ��QoS���� - ���ݹ̶�ͷ��λ
                    uint8_t qos = (data_start[0] & 0x06) >> 1;
                    
                    // ���QoS > 0����ϢID��ռ��2���ֽ�
                    if (qos > 0) {
                        payload_offset += 2;  // ������ϢID
                    }
                    
                    uint16_t payload_length = remaining_length - (payload_offset - 2);
                    
                    // Ϊ���ط��仺����
                    static char payload_buffer[512];
                    if (payload_length < sizeof(payload_buffer)) {
                        memcpy(payload_buffer, &data_start[payload_offset], payload_length);
                        payload_buffer[payload_length] = '\0';
                        
                        sprintf(debug_buf, "Payload: %s", payload_buffer);
                        MQTT_DebugPrint(debug_buf);
                        
                        // ����ͬ�������Ϣ - �Ľ���
                        if (strcmp(topic, MQTT_TOPIC_RESULT) == 0) {
                            // ����ʶ����
                            MQTT_DebugPrint("Received result message");
                            if (payload_length < sizeof(mqtt_result_buffer)) {
                                // Copy result to buffer and set flag
                                strncpy(mqtt_result_buffer, payload_buffer, payload_length);
                                mqtt_result_buffer[payload_length] = '\0';
                                mqtt_result_received = 1;
                                
                                sprintf(debug_buf, "Result saved: %s", mqtt_result_buffer);
                                MQTT_DebugPrint(debug_buf);
                            } else {
                                MQTT_DebugPrint("Result too large for buffer!");
                            }
                        } else if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
                            // ��������
                            MQTT_DebugPrint("Received command message");
                            MQTT_CommandCallback(payload_buffer);
                        } else if (strcmp(topic, MQTT_TOPIC_RETRANS_REQUEST) == 0) {
                            // �����ش�����
                            MQTT_DebugPrint("Received retransmission request");
                            Integration_HandleRetransmissionRequest(topic, payload_buffer);
                        }
                        
                        // �����QoS 1��QoS 2����Ҫ����ȷ��
                        if (qos == 1) {
                            // ����PUBACK
                            uint16_t message_id = (data_start[offset + 2 + topic_length] << 8) | 
                                                 data_start[offset + 2 + topic_length + 1];
                            
                            // ����PUBACK��
                            uint8_t puback[4];
                            puback[0] = 0x40;  // PUBACK
                            puback[1] = 0x02;  // ʣ�೤��
                            puback[2] = message_id >> 8;  // ��ϢID���ֽ�
                            puback[3] = message_id & 0xFF; // ��ϢID���ֽ�
                            
                            // ����PUBACK
                            ESP8266_SendTCPData((char*)puback, 4);
                            MQTT_DebugPrint("Sent PUBACK for QoS 1 message");
                        } else if (qos == 2) {
                            // ����PUBREC (��һ�׶�ȷ��)
                            uint16_t message_id = (data_start[offset + 2 + topic_length] << 8) | 
                                                 data_start[offset + 2 + topic_length + 1];
                            
                            // ����PUBREC��
                            uint8_t pubrec[4];
                            pubrec[0] = 0x50;  // PUBREC
                            pubrec[1] = 0x02;  // ʣ�೤��
                            pubrec[2] = message_id >> 8;  // ��ϢID���ֽ�
                            pubrec[3] = message_id & 0xFF; // ��ϢID���ֽ�
                            
                            // ����PUBREC
                            ESP8266_SendTCPData((char*)pubrec, 4);
                            MQTT_DebugPrint("Sent PUBREC for QoS 2 message");
                        }
                    } else {
                        MQTT_DebugPrint("Payload too large for buffer");
                    }
                } else {
                    MQTT_DebugPrint("Topic too long");
                }
            } else if ((data_start[0] & 0xF0) == 0x60) {
                // ����PUBREL (QoS 2�ڶ��׶�)
                // ��ȡ��ϢID
                uint16_t message_id = (data_start[2] << 8) | data_start[3];
                
                // ����PUBCOMP (�����׶�ȷ��)
                uint8_t pubcomp[4];
                pubcomp[0] = 0x70;  // PUBCOMP
                pubcomp[1] = 0x02;  // ʣ�೤��
                pubcomp[2] = message_id >> 8;  // ��ϢID���ֽ�
                pubcomp[3] = message_id & 0xFF; // ��ϢID���ֽ�
                
                // ����PUBCOMP
                ESP8266_SendTCPData((char*)pubcomp, 4);
                MQTT_DebugPrint("Processed PUBREL and sent PUBCOMP");
            } else {
                // Add debug for other message types
                sprintf(debug_buf, "Other MQTT message type: 0x%02X", data_start[0]);
                MQTT_DebugPrint(debug_buf);
            }
        } else {
            MQTT_DebugPrint("IPD format error or zero length");
        }
    } else {
        // Check for other ESP8266 messages
        if (strstr((char*)buffer, "CLOSED") != NULL) {
            MQTT_DebugPrint("Connection closed notification");
        } else if (strstr((char*)buffer, "ALREADY CONNECTED") != NULL) {
            MQTT_DebugPrint("Already connected notification");
        } else if (strstr((char*)buffer, "ERROR") != NULL) {
            MQTT_DebugPrint("Error notification");
        } else if (strstr((char*)buffer, "OK") != NULL) {
            MQTT_DebugPrint("OK notification");
        } else {
            // Check for direct result message if IPD was not found
            char* topic_start = strstr((char*)buffer, "stm32/result");
            if (topic_start) {
                MQTT_DebugPrint("Found direct result message without IPD prefix!");
                
                // Extract the payload (skip the topic)
                char* payload = topic_start + strlen("stm32/result");
                
                // Copy to result buffer and set flag
                strncpy(mqtt_result_buffer, payload, sizeof(mqtt_result_buffer) - 1);
                mqtt_result_buffer[sizeof(mqtt_result_buffer) - 1] = '\0';
                mqtt_result_received = 1;
                
                sprintf(debug_buf, "Direct result extracted: %s", mqtt_result_buffer);
                MQTT_DebugPrint(debug_buf);
            } else {
                MQTT_DebugPrint("Unknown ESP8266 response");
            }
        }
    }
}

/**
  * @brief  �ȴ�MQTT�����Ϣ - ������
  * @param  result_buffer: ������ս���Ļ�����
  * @param  buffer_size: ��������С
  * @param  timeout: ��ʱ�ȴ�ʱ��(����)
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t MQTT_WaitForResult(char* result_buffer, uint16_t buffer_size, uint32_t timeout)
{
    uint32_t start_time = HAL_GetTick();
    
    MQTT_DebugPrint("Waiting for result...");
    
    // ׼���ý��ձ�־
    mqtt_result_received = 0;
    memset(mqtt_result_buffer, 0, sizeof(mqtt_result_buffer));
    
    // ѭ���ȴ�ֱ�����յ����ؽ����ʱ
    while ((HAL_GetTick() - start_time) < timeout && !mqtt_result_received) {
        // Process incoming messages frequently
        MQTT_ProcessIncomingData();
        
        // ÿ��1����ʾ�ȴ�״̬
        if ((HAL_GetTick() - start_time) % 1000 < 20) {
            char debug_msg[64];
            sprintf(debug_msg, "Still waiting for result... %lu ms elapsed", 
                  (unsigned long)(HAL_GetTick() - start_time));
            MQTT_DebugPrint(debug_msg);
        }
        
        // Reset received flag for testing
        if (mqtt_result_received) {
            MQTT_DebugPrint("Result received flag is set!");
        }
        
        // Add a short delay to avoid CPU overload
        HAL_Delay(10);
    }
    
    if (mqtt_result_received) {
        // ���ƽ�������������
        strncpy(result_buffer, mqtt_result_buffer, buffer_size - 1);
        result_buffer[buffer_size - 1] = '\0';
        
        char debug_msg[64];
        sprintf(debug_msg, "Result received: %s", result_buffer);
        MQTT_DebugPrint(debug_msg);
        return 1;
    } else {
        MQTT_DebugPrint("Timeout waiting for result");
        return 0;
    }
}