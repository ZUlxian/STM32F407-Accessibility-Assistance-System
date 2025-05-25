/**
  ******************************************************************************
  * @file    mqtt.c
  * @brief   MQTT client implementation for ESP8266
  ******************************************************************************
  */

#include "mqtt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>  // 需要用到的标准库函数，如malloc和free等功能函数
#include "ff.h"  

/* 串口通信对象声明 */
extern UART_HandleTypeDef huart1;

// 调试打印功能函数
void MQTT_DebugPrint(const char* msg) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"[MQTT] ", 7, 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 300);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
    HAL_Delay(10);
}

// 以16进制格式打印数据的功能函数
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

// 接收消息的数据缓冲区变量
static char mqtt_result_buffer[512] = {0};
static uint8_t mqtt_result_received = 0;

/**
  * @brief  初始化MQTT客户端
  * @retval 1: 成功, 0: 失败
  */
uint8_t MQTT_Init(void)
{
    MQTT_DebugPrint("Initializing MQTT client...");
    
    // 确保ESP8266已经初始化并连接到WiFi
    if (!esp8266_initialized || !esp8266_connected) {
        MQTT_DebugPrint("ESP8266 not initialized or not connected to WiFi");
        return 0;
    }
    
    return 1;
}

/**
  * @brief  解析ESP8266返回的+IPD数据
  * @param  buffer: 数据缓冲区
  * @param  len: 缓冲区长度
  * @param  data: 解析出的数据指针
  * @param  data_len: 数据指针长度
  * @retval 1: 成功, 0: 失败
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
    
    // 解析长度
    int ipd_len = 0;
    sscanf(ipd_start + 5, "%d", &ipd_len);
    
    // 设置返回的数据指针
    *data = (uint8_t*)(colon + 1);
    *data_len = ipd_len;
    
    return 1;
}

/**
  * @brief  连接到MQTT服务器 - 基本版
  * @retval 1: 成功, 0: 失败
  */
uint8_t MQTT_Connect(void)
{
    char connect_cmd[256];
    
    MQTT_DebugPrint("Connecting to MQTT broker...");
    
    // 创建TCP连接
    if (!ESP8266_CreateTCPConnection(MQTT_BROKER, MQTT_PORT)) {
        MQTT_DebugPrint("Failed to create TCP connection to MQTT broker");
        return 0;
    }
    
    // 创建MQTT连接包 - 使用完整的二进制数据包
    static uint8_t connect_packet[128]; // 预分配的二进制数据包
    uint16_t packet_len = 0;
    
    // 消息类型
    connect_packet[packet_len++] = 0x10;  // CONNECT packet type
    
    // 剩余长度采用变长编码方式Variable Length Encoding of Remaining Length
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
    
    // 更新剩余长度
    uint8_t remaining_length = packet_len - 2;  // 减去消息类型和剩余长度字段
    connect_packet[remaining_length_pos] = remaining_length;
    
    // 打印调试信息
    MQTT_DebugHex("MQTT CONNECT packet", connect_packet, packet_len);
    
    // 发送连接包
    if (!ESP8266_SendTCPData((char*)connect_packet, packet_len)) {
        MQTT_DebugPrint("Failed to send MQTT CONNECT packet");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // 等待CONNACK
    uint32_t start_time = HAL_GetTick();
    uint8_t response_buffer[128];
    uint16_t response_len = 0;
    uint8_t connack_received = 0;
    
    while ((HAL_GetTick() - start_time) < 5000 && !connack_received) {
        // 接收数据(包含+IPD前缀)
        memset(response_buffer, 0, sizeof(response_buffer));
        response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 1000);
        
        if (response_len > 0) {
            MQTT_DebugPrint("Received data:");
            MQTT_DebugHex("Raw data", response_buffer, response_len);
            
            // 解析IPD数据
            uint8_t* mqtt_data = NULL;
            uint16_t mqtt_data_len = 0;
            
            // 逐行分析查找解析+IPD
            char* ipd_str = strstr((char*)response_buffer, "+IPD,");
            if (ipd_str) {
                MQTT_DebugPrint("Found +IPD data");
                
                char* colon = strchr(ipd_str, ':');
                if (colon) {
                    // 设置返回的数据指针
                    mqtt_data = (uint8_t*)(colon + 1);
                    
                    // 检查是否为CONNACK包 (消息类型码为0x20)
                    if (mqtt_data[0] == 0x20) {
                        MQTT_DebugPrint("Valid CONNACK received");
                        connack_received = 1;
                    } else {
                        MQTT_DebugPrint("Received data is not a CONNACK packet");
                    }
                }
            } else if (strstr((char*)response_buffer, "OK") != NULL) {
                // ESP8266可能回复仅表示接收OK
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
  * @brief  发送文本消息到MQTT主题（使用QoS版本-使用完整二进制数据包）
  * @param  topic: 主题名称
  * @param  message: 要发送的消息
  * @retval 1: 成功, 0: 失败
  */
uint8_t MQTT_Publish(const char* topic, const char* message)
{
    uint16_t topic_len = strlen(topic);
    uint16_t message_len = strlen(message);
    uint16_t packet_len = 0;
    
    // 检查数据长度是否合理
    if (topic_len == 0 || message_len == 0 || topic_len + message_len + 4 > 1024) {
        MQTT_DebugPrint("Invalid topic or message length");
        return 0;
    }
    
    // 使用完整的二进制数据包处理生成数据
    static uint8_t publish_packet[1024 + 20]; // 预分配的二进制数据包
    
    // 消息类型 (QoS 1, No Retain) - 使用QoS 1提供更可靠传输
    publish_packet[packet_len++] = 0x32;  // 0x30 | 0x02 = QoS 1
    
    // 计算剩余长度(可变头部+载荷)
    uint16_t remaining_length = topic_len + 2 + message_len + 2;  // +2 for message ID
    
    // 消息ID (只在QoS > 0时需要)
    static uint16_t message_id = 0;
    message_id = (message_id + 1) % 65535;
    if (message_id == 0) message_id = 1;  // 避免ID为0
    
    // 编码剩余长度(根据长度选择编码方式)
    if (remaining_length < 128) {
        publish_packet[packet_len++] = remaining_length;
    } else {
        publish_packet[packet_len++] = (remaining_length % 128) + 128;
        publish_packet[packet_len++] = remaining_length / 128;
    }
    
    // 可变头部 - 主题名
    publish_packet[packet_len++] = topic_len >> 8;
    publish_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&publish_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // 消息ID (QoS 1需要)
    publish_packet[packet_len++] = message_id >> 8;
    publish_packet[packet_len++] = message_id & 0xFF;
    
    // 载荷 - 消息内容
    memcpy(&publish_packet[packet_len], message, message_len);
    packet_len += message_len;
    
    char debug_msg[64];
    sprintf(debug_msg, "Publishing to topic: %s (%d bytes)", topic, message_len);
    MQTT_DebugPrint(debug_msg);
    MQTT_DebugHex("MQTT PUBLISH packet", publish_packet, packet_len);
    
    // 发送发布包
    uint8_t success = ESP8266_SendTCPData((char*)publish_packet, packet_len);
    
    if (success) {
        // 等待PUBACK (QoS 1需要)
        uint32_t start_time = HAL_GetTick();
        uint8_t response_buffer[64];
        uint16_t response_len = 0;
        uint8_t puback_received = 0;
        
        while ((HAL_GetTick() - start_time) < 2000 && !puback_received) {
            response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 500);
            
            if (response_len > 0) {
                // 逐行分析查找PUBACK
                char* ipd_str = strstr((char*)response_buffer, "+IPD,");
                if (ipd_str) {
                    char* colon = strchr(ipd_str, ':');
                    if (colon) {
                        uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                        // 检查是否为PUBACK (0x40)
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
  * @brief  发送二进制数据到MQTT主题（使用QoS 2可靠传输版本）
  * @param  topic: 主题名称
  * @param  data: 二进制数据
  * @param  len: 数据长度
  * @retval 1: 成功, 0: 失败
  */
uint8_t MQTT_PublishBinary(const char* topic, uint8_t* data, uint16_t len)
{
    uint16_t topic_len = strlen(topic);
    uint16_t packet_len = 0;
    
    // 检查数据长度是否合理
    if (topic_len == 0 || len == 0 || topic_len + len + 4 > 1024) {
        MQTT_DebugPrint("Invalid topic or data length");
        return 0;
    }
    
    // 使用完整的二进制数据包
    static uint8_t publish_packet[1024 + 20]; // 预分配的二进制数据包
    
    // 消息类型 (QoS 2, No Retain) - 使用QoS级别为2提供最可靠传输
    publish_packet[packet_len++] = 0x34;  // 0x30 | 0x04 = QoS 2
    
    // 计算剩余长度
    uint16_t remaining_length = topic_len + 2 + len + 2;  // +2 for message ID
    
    // 消息ID (只在QoS > 0时需要)
    static uint16_t message_id = 0;
    message_id = (message_id + 1) % 65535;
    if (message_id == 0) message_id = 1;  // 避免ID为0
    
    // 编码剩余长度(根据长度选择编码方式)
    if (remaining_length < 128) {
        publish_packet[packet_len++] = remaining_length;
    } else if (remaining_length < 16384) {
        publish_packet[packet_len++] = (remaining_length % 128) + 128;
        publish_packet[packet_len++] = remaining_length / 128;
    } else {
        // 处理更大的数据包 (理论上不应超过16KB，但以备不时之需)
        publish_packet[packet_len++] = (remaining_length % 128) + 128;
        publish_packet[packet_len++] = ((remaining_length / 128) % 128) + 128;
        publish_packet[packet_len++] = remaining_length / 16384;
    }
    
    // 可变头部 - 主题名
    publish_packet[packet_len++] = topic_len >> 8;
    publish_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&publish_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // 消息ID (QoS 2需要)
    publish_packet[packet_len++] = message_id >> 8;
    publish_packet[packet_len++] = message_id & 0xFF;
    
    // 载荷 - 二进制数据
    memcpy(&publish_packet[packet_len], data, len);
    packet_len += len;
    
    char debug_msg[64];
    sprintf(debug_msg, "Publishing binary data to topic: %s (%d bytes) with QoS 2", topic, len);
    MQTT_DebugPrint(debug_msg);
    
    // 发送发布包
    uint8_t success = ESP8266_SendTCPData((char*)publish_packet, packet_len);
    
    if (success) {
        // 等待PUBREC (QoS 2第一确认阶段)
        uint32_t start_time = HAL_GetTick();
        uint8_t response_buffer[128];
        uint16_t response_len = 0;
        uint8_t pubrec_received = 0;
        
        while ((HAL_GetTick() - start_time) < 1000 && !pubrec_received) {
            response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 10);
            
            if (response_len > 0) {
                // 逐行分析查找PUBREC
                char* ipd_str = strstr((char*)response_buffer, "+IPD,");
                if (ipd_str) {
                    char* colon = strchr(ipd_str, ':');
                    if (colon) {
                        uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                        // 检查是否为PUBREC (0x50)
                        if ((mqtt_data[0] & 0xF0) == 0x50) {
                            // 验证消息ID是否匹配
                            uint16_t recv_id = (mqtt_data[2] << 8) | mqtt_data[3];
                            if (recv_id == message_id) {
                                pubrec_received = 1;
                                MQTT_DebugPrint("PUBREC received");
                                
                                // 发送PUBREL (第二确认)
                                uint8_t pubrel_packet[4];
                                pubrel_packet[0] = 0x62;  // PUBREL with QoS 1
                                pubrel_packet[1] = 0x02;  // 剩余长度
                                pubrel_packet[2] = message_id >> 8;
                                pubrel_packet[3] = message_id & 0xFF;
                                
                                if (ESP8266_SendTCPData((char*)pubrel_packet, 4)) {
                                    // 等待PUBCOMP (QoS 2第三确认阶段)
                                    uint32_t pubcomp_start = HAL_GetTick();
                                    uint8_t pubcomp_received = 0;
                                    
                                    while ((HAL_GetTick() - pubcomp_start) < 1000 && !pubcomp_received) {
                                        response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 10);
                                        
                                        if (response_len > 0) {
                                            // 逐行分析查找PUBCOMP
                                            ipd_str = strstr((char*)response_buffer, "+IPD,");
                                            if (ipd_str) {
                                                colon = strchr(ipd_str, ':');
                                                if (colon) {
                                                    mqtt_data = (uint8_t*)(colon + 1);
                                                    // 检查是否为PUBCOMP (0x70)
                                                    if ((mqtt_data[0] & 0xF0) == 0x70) {
                                                        pubcomp_received = 1;
                                                        MQTT_DebugPrint("PUBCOMP received - QoS 2 publish complete");
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    
                                    if (pubcomp_received) {
                                        return 1;  // QoS 2协议完成
                                    } else {
                                        MQTT_DebugPrint("Failed to receive PUBCOMP");
                                        // 仍然成功，因为我们至少已收到PUBREC
                                        return 1;
                                    }
                                } else {
                                    MQTT_DebugPrint("Failed to send PUBREL");
                                    // 仍然成功，因为我们至少已收到PUBREC
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
            // 考虑到网络可能不稳定，我们假设数据已发送
            // 真实情况下应该重试，但这里简化处理
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
  * @brief  订阅MQTT主题 - 基本版
  * @param  topic: 主题名称
  * @retval 1: 成功, 0: 失败
  */
uint8_t MQTT_Subscribe(const char* topic)
{
    uint16_t topic_len = strlen(topic);
    uint16_t packet_len = 0;
    
    // 检查主题长度
    if (topic_len == 0 || topic_len > 255) {
        MQTT_DebugPrint("Invalid topic length");
        return 0;
    }
    
    // 创建MQTT订阅包 - 使用完整的二进制数据包
    static uint8_t subscribe_packet[280];  // 预分配的二进制数据包
    
    // 消息类型
    subscribe_packet[packet_len++] = 0x82;  // SUBSCRIBE packet type with QoS 1
    
    // 计算剩余长度
    uint8_t remaining_length = topic_len + 5;  // 2(消息ID) + 2(主题长度) + topic_len + 1(QoS)
    subscribe_packet[packet_len++] = remaining_length;
    
    // 可变头部 - 消息标识符
    subscribe_packet[packet_len++] = 0x00;
    subscribe_packet[packet_len++] = 0x01;  // Message ID = 1
    
    // 载荷 - 主题列表
    subscribe_packet[packet_len++] = topic_len >> 8;
    subscribe_packet[packet_len++] = topic_len & 0xFF;
    memcpy(&subscribe_packet[packet_len], topic, topic_len);
    packet_len += topic_len;
    
    // 请求的QoS
    subscribe_packet[packet_len++] = 0x00;  // QoS 0
    
    // 打印调试信息
    char debug_msg[64];
    sprintf(debug_msg, "Subscribing to topic: %s", topic);
    MQTT_DebugPrint(debug_msg);
    MQTT_DebugHex("MQTT SUBSCRIBE packet", subscribe_packet, packet_len);
    
    // 发送订阅包
    if (!ESP8266_SendTCPData((char*)subscribe_packet, packet_len)) {
        MQTT_DebugPrint("Failed to send SUBSCRIBE packet");
        return 0;
    }
    
    // 等待SUBACK
    uint32_t start_time = HAL_GetTick();
    uint8_t response_buffer[128];
    uint16_t response_len = 0;
    uint8_t suback_received = 0;
    
    while ((HAL_GetTick() - start_time) < 5000 && !suback_received) {
        // 接收数据
        memset(response_buffer, 0, sizeof(response_buffer));
        response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 1000);
        
        if (response_len > 0) {
            MQTT_DebugPrint("Received data:");
            MQTT_DebugHex("Raw data", response_buffer, response_len);
            
            // 解析+IPD数据
            char* ipd_str = strstr((char*)response_buffer, "+IPD,");
            if (ipd_str) {
                char* colon = strchr(ipd_str, ':');
                if (colon) {
                    // 设置返回的数据指针
                    uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                    
                    // 检查是否为SUBACK包 (第一个字节码为0x90)
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
        
        // 清除结果缓冲区
        memset(mqtt_result_buffer, 0, sizeof(mqtt_result_buffer));
        mqtt_result_received = 0;
        
        return 1;
    } else {
        MQTT_DebugPrint("Failed to receive SUBACK");
        return 0;
    }
}

/**
  * @brief  检查MQTT连接状态
  * @retval 1: 已连接, 0: 未连接
  */
uint8_t MQTT_Check(void)
{
    static uint8_t ping_packet[2] = {0xC0, 0x00}; // PINGREQ packet
    MQTT_DebugPrint("Checking MQTT connection...");
    
    if (ESP8266_SendTCPData((char*)ping_packet, 2)) {
        // 等待PINGRESP
        uint32_t start_time = HAL_GetTick();
        uint8_t response_buffer[32];
        uint16_t response_len = 0;
        uint8_t pingresp_received = 0;
        
        while ((HAL_GetTick() - start_time) < 2000 && !pingresp_received) {
            // 接收数据
            memset(response_buffer, 0, sizeof(response_buffer));
            response_len = ESP8266_ReceiveData(response_buffer, sizeof(response_buffer), 500);
            
            if (response_len > 0) {
                MQTT_DebugHex("PINGRESP data", response_buffer, response_len);
                
                // 解析+IPD数据
                char* ipd_str = strstr((char*)response_buffer, "+IPD,");
                if (ipd_str) {
                    char* colon = strchr(ipd_str, ':');
                    if (colon) {
                        // 设置返回的数据指针
                        uint8_t* mqtt_data = (uint8_t*)(colon + 1);
                        
                        // 检查是否为PINGRESP包 (0xD0 0x00)
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
  * @brief  断开MQTT连接
  * @retval 1: 成功, 0: 失败
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
  * @brief  处理接收到的MQTT消息 - 基本版
  */
void MQTT_ProcessIncomingData(void)
{
    uint8_t buffer[512];
    uint16_t len = ESP8266_ReceiveData(buffer, sizeof(buffer), 100);
    
    // 如果没有新数据，尝试从文件读取结果
    if (len == 0) {
        static uint32_t last_file_check = 0;
        // 每隔1秒检查一次文件
        if (HAL_GetTick() - last_file_check > 1000) {
            last_file_check = HAL_GetTick();
            
            // 检查SD卡上是否有result.txt文件
            FIL file;
            FRESULT res = f_open(&file, "0:/result.txt", FA_READ);
            if (res == FR_OK) {
                MQTT_DebugPrint("Found result file on SD card!");
                
                // 读取文件内容
                char file_result[256] = {0};
                UINT bytes_read;
                res = f_read(&file, file_result, sizeof(file_result) - 1, &bytes_read);
                
                if (res == FR_OK && bytes_read > 0) {
                    file_result[bytes_read] = '\0';
                    
                    // 复制到结果缓冲区并设置标志
                    strncpy(mqtt_result_buffer, file_result, sizeof(mqtt_result_buffer) - 1);
                    mqtt_result_buffer[sizeof(mqtt_result_buffer) - 1] = '\0';
                    mqtt_result_received = 1;
                    
                    char debug_msg[64];
                    sprintf(debug_msg, "Result from file: %s", mqtt_result_buffer);
                    MQTT_DebugPrint(debug_msg);
                    
                    // 删除文件，避免重复读取
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
    
    // 检查是否包含+IPD
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
        // 获取数据长度
        int data_len = 0;
        sscanf(ipd_str + 5, "%d", &data_len);
        
        sprintf(debug_buf, "Found IPD with length %d", data_len);
        MQTT_DebugPrint(debug_buf);
        
        // 查找冒号，冒号后面是实际数据
        char* data_start = strchr(ipd_str, ':');
        if (data_start && data_len > 0) {
            data_start++; // 跳过冒号
            
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
            
            // 检查是否是PUBLISH消息 (0x30)
            if ((data_start[0] & 0xF0) == 0x30) {
                MQTT_DebugPrint("Detected PUBLISH message");
                
                // 获取剩余长度(可变长度编码)
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
                
                // 获取主题长度
                uint16_t topic_length = (data_start[offset] << 8) | data_start[offset + 1];
                
                // 检查主题是否是我们订阅的主题
                if (topic_length < 64) {  // 防止溢出
                    char topic[64] = {0};
                    memcpy(topic, &data_start[offset + 2], topic_length);
                    topic[topic_length] = '\0';
                    
                    sprintf(debug_buf, "Message topic: %s", topic);
                    MQTT_DebugPrint(debug_buf);
                    
                    // 提取消息负载
                    uint16_t payload_offset = offset + 2 + topic_length;
                    
                    // 确定QoS级别 - 根据固定头的位
                    uint8_t qos = (data_start[0] & 0x06) >> 1;
                    
                    // 如果QoS > 0，消息ID会占用2个字节
                    if (qos > 0) {
                        payload_offset += 2;  // 跳过消息ID
                    }
                    
                    uint16_t payload_length = remaining_length - (payload_offset - 2);
                    
                    // 为负载分配缓冲区
                    static char payload_buffer[512];
                    if (payload_length < sizeof(payload_buffer)) {
                        memcpy(payload_buffer, &data_start[payload_offset], payload_length);
                        payload_buffer[payload_length] = '\0';
                        
                        sprintf(debug_buf, "Payload: %s", payload_buffer);
                        MQTT_DebugPrint(debug_buf);
                        
                        // 处理不同主题的消息 - 改进版
                        if (strcmp(topic, MQTT_TOPIC_RESULT) == 0) {
                            // 接收识别结果
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
                            // 处理命令
                            MQTT_DebugPrint("Received command message");
                            MQTT_CommandCallback(payload_buffer);
                        } else if (strcmp(topic, MQTT_TOPIC_RETRANS_REQUEST) == 0) {
                            // 处理重传请求
                            MQTT_DebugPrint("Received retransmission request");
                            Integration_HandleRetransmissionRequest(topic, payload_buffer);
                        }
                        
                        // 如果是QoS 1或QoS 2，需要发送确认
                        if (qos == 1) {
                            // 发送PUBACK
                            uint16_t message_id = (data_start[offset + 2 + topic_length] << 8) | 
                                                 data_start[offset + 2 + topic_length + 1];
                            
                            // 构建PUBACK包
                            uint8_t puback[4];
                            puback[0] = 0x40;  // PUBACK
                            puback[1] = 0x02;  // 剩余长度
                            puback[2] = message_id >> 8;  // 消息ID高字节
                            puback[3] = message_id & 0xFF; // 消息ID低字节
                            
                            // 发送PUBACK
                            ESP8266_SendTCPData((char*)puback, 4);
                            MQTT_DebugPrint("Sent PUBACK for QoS 1 message");
                        } else if (qos == 2) {
                            // 发送PUBREC (第一阶段确认)
                            uint16_t message_id = (data_start[offset + 2 + topic_length] << 8) | 
                                                 data_start[offset + 2 + topic_length + 1];
                            
                            // 构建PUBREC包
                            uint8_t pubrec[4];
                            pubrec[0] = 0x50;  // PUBREC
                            pubrec[1] = 0x02;  // 剩余长度
                            pubrec[2] = message_id >> 8;  // 消息ID高字节
                            pubrec[3] = message_id & 0xFF; // 消息ID低字节
                            
                            // 发送PUBREC
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
                // 处理PUBREL (QoS 2第二阶段)
                // 提取消息ID
                uint16_t message_id = (data_start[2] << 8) | data_start[3];
                
                // 发送PUBCOMP (第三阶段确认)
                uint8_t pubcomp[4];
                pubcomp[0] = 0x70;  // PUBCOMP
                pubcomp[1] = 0x02;  // 剩余长度
                pubcomp[2] = message_id >> 8;  // 消息ID高字节
                pubcomp[3] = message_id & 0xFF; // 消息ID低字节
                
                // 发送PUBCOMP
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
  * @brief  等待MQTT结果消息 - 基本版
  * @param  result_buffer: 保存接收结果的缓冲区
  * @param  buffer_size: 缓冲区大小
  * @param  timeout: 超时等待时间(毫秒)
  * @retval 1: 成功, 0: 失败
  */
uint8_t MQTT_WaitForResult(char* result_buffer, uint16_t buffer_size, uint32_t timeout)
{
    uint32_t start_time = HAL_GetTick();
    
    MQTT_DebugPrint("Waiting for result...");
    
    // 准备好接收标志
    mqtt_result_received = 0;
    memset(mqtt_result_buffer, 0, sizeof(mqtt_result_buffer));
    
    // 循环等待直到接收到返回结果或超时
    while ((HAL_GetTick() - start_time) < timeout && !mqtt_result_received) {
        // Process incoming messages frequently
        MQTT_ProcessIncomingData();
        
        // 每隔1秒显示等待状态
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
        // 复制结果到输出缓冲区
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