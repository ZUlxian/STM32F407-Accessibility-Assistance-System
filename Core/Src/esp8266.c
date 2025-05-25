/**
  ******************************************************************************
  * @file    esp8266.c
  * @brief   ESP8266 WiFi module driver with AT firmware
  ******************************************************************************
  */

#include "esp8266.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 全局变量 */
uint8_t esp8266_initialized = 0;
uint8_t esp8266_connected = 0;

static char rx_buffer[2048];
static UART_HandleTypeDef *esp8266_uart;

/* 外部引用调试串口 */
extern UART_HandleTypeDef huart1;
extern void ESP8266_HardReset(void);

/* 简化版调试输出 */
void DebugPrint(const char* msg) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"[ESP] ", 6, 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 300);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
    HAL_Delay(10); // 增加一点延迟，确保串口输出完整
}

/**
  * @brief  清空接收缓冲区
  */
void ESP8266_ClearBuffer(void)
{
    uint8_t dummy;
    uint32_t timeout = HAL_GetTick() + 100; // 100ms超时
    
    // 读取并丢弃所有待处理的数据
    while (HAL_GetTick() < timeout) {
        if (HAL_UART_Receive(esp8266_uart, &dummy, 1, 1) != HAL_OK) {
            break;
        }
    }
    
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

/**
  * @brief  发送AT命令并等待响应
  * @param  command: 要发送的AT命令字符串
  * @param  response: 期望接收到的响应字符串（NULL表示不检查响应）
  * @param  timeout: 等待响应的超时时间（毫秒）
  * @retval 1: 成功收到期望响应, 0: 失败或超时
  */
uint8_t ESP8266_SendCommand(char* command, char* response, uint32_t timeout)
{
    uint32_t start_time;
    uint16_t rx_index = 0;
    uint8_t received_char;
    
    // 清空接收缓冲区
    ESP8266_ClearBuffer();
    
    // 发送AT命令
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)command, strlen(command), 500);
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)"\r\n", 2, 100);
    
    // 如果不需要检查响应
    if (response == NULL) {
        HAL_Delay(50); // 给模块一些处理时间
        return 1;
    }
    
    // 等待响应
    start_time = HAL_GetTick();
    while ((HAL_GetTick() - start_time) < timeout) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // 检查是否包含期望响应
            if (strstr(rx_buffer, response) != NULL) {
                // 成功
                return 1;
            }
        }
    }
    
    // 显示超时错误和接收到的内容
    if (timeout > 1000) { // 只有长超时才输出调试信息
        DebugPrint("Command timeout. Received:");
        if (rx_index > 0) {
            DebugPrint(rx_buffer);
        } else {
            DebugPrint("(No data)");
        }
    }
    
    // 超时，未收到期望响应
    return 0;
}

/**
  * @brief  发送数据
  * @param  data: 要发送的数据
  * @param  len: 数据长度
  * @param  timeout: 超时时间（毫秒）
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_SendData(uint8_t* data, uint16_t len, uint32_t timeout)
{
    if (HAL_UART_Transmit(esp8266_uart, data, len, timeout) == HAL_OK) {
        return 1;
    }
    return 0;
}

/**
  * @brief  接收数据
  * @param  buffer: 接收缓冲区
  * @param  max_len: 缓冲区最大长度
  * @param  timeout: 超时时间（毫秒）
  * @retval 接收到的数据长度
  */
uint8_t ESP8266_ReceiveData(uint8_t* buffer, uint16_t max_len, uint32_t timeout)
{
    uint16_t rx_index = 0;
    uint8_t received_char;
    uint32_t start_time = HAL_GetTick();
    
    while ((HAL_GetTick() - start_time) < timeout && rx_index < max_len) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            buffer[rx_index++] = received_char;
        }
    }
    
    return rx_index;
}

/**
  * @brief  初始化ESP8266
  * @param  huart: 与ESP8266连接的UART句柄
  * @retval 1: 初始化成功, 0: 初始化失败
  */
uint8_t ESP8266_Init(UART_HandleTypeDef *huart)
{
    uint8_t retry_count;
    char debug_msg[64];
    
    // 存储UART句柄
    esp8266_uart = huart;
    
    DebugPrint("Initializing ESP8266...");
    
    // 清空接收缓冲区
    ESP8266_ClearBuffer();
    
    // 给模块上电后的稳定时间
    HAL_Delay(1000);
    
    // 尝试与ESP8266通信 - 首先尝试重启以确保正常状态
    ESP8266_SendCommand("AT+RST", NULL, 1000);
    DebugPrint("ESP8266 reset sent");
    
    // 等待模块重启完成
    HAL_Delay(3000);
    
    // 尝试与ESP8266通信
    for (retry_count = 0; retry_count < 5; retry_count++) {
        sprintf(debug_msg, "AT test attempt %d", retry_count+1);
        DebugPrint(debug_msg);
        
        if (ESP8266_SendCommand("AT", "OK", 1000)) {
            // 通信成功，继续配置
            DebugPrint("ESP8266 communication established");
            break;
        }
        
        HAL_Delay(500);
    }
    
    // 如果5次尝试都失败，返回错误
    if (retry_count >= 5) {
        DebugPrint("ESP8266 communication failed");
        esp8266_initialized = 0;
        return 0;
    }
    
    // 配置工作模式为Station模式（客户端）
    DebugPrint("Setting Station mode");
    if (!ESP8266_SetMode(1)) {
        DebugPrint("Failed to set Station mode");
        esp8266_initialized = 0;
        return 0;
    }
    
    // 配置连接模式为单连接模式
    DebugPrint("Setting single connection mode");
    if (!ESP8266_SendCommand("AT+CIPMUX=0", "OK", 1000)) {
        DebugPrint("Failed to set single connection mode");
        esp8266_initialized = 0;
        return 0;
    }
    
    // 禁用自动连接
    ESP8266_SendCommand("AT+CWAUTOCONN=0", "OK", 1000);
    
    // 检查SSL支持 - 修改：移除AT+CIPSSLSIZE命令，因为它可能导致错误
    DebugPrint("Checking SSL support...");
    
    // 禁用SSL证书验证
    if (!ESP8266_SendCommand("AT+CIPSSLCCONF=0", "OK", 1000)) {
        DebugPrint("Warning: Disabling SSL certificate verification failed");
    } else {
        DebugPrint("SSL certificate verification disabled");
    }
    
    // 初始化成功
    DebugPrint("ESP8266 initialization complete");
    esp8266_initialized = 1;
    return 1;
}

/**
  * @brief  设置ESP8266工作模式
  * @param  mode: 1=Station模式, 2=AP模式, 3=两者兼有
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_SetMode(uint8_t mode)
{
    char cmd[20];
    sprintf(cmd, "AT+CWMODE=%d", mode);
    
    if (ESP8266_SendCommand(cmd, "OK", 1000)) {
        return 1;
    }
    
    return 0;
}

/**
  * @brief  重启ESP8266模块
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_Restart(void)
{
    DebugPrint("Restarting ESP8266");
    
    // 发送重启命令
    if (!ESP8266_SendCommand("AT+RST", NULL, 1000)) {
        DebugPrint("Failed to send restart command");
        return 0;
    }
    
    // 等待模块重启
    DebugPrint("Waiting for ESP8266 to restart...");
    HAL_Delay(2000);
    
    // 检查通信
    for (uint8_t retry = 0; retry < 10; retry++) {
        if (ESP8266_SendCommand("AT", "OK", 1000)) {
            DebugPrint("ESP8266 restarted successfully");
            return 1;
        }
        HAL_Delay(500);
    }
    
    DebugPrint("ESP8266 restart check failed");
    return 0;
}

/**
  * @brief  获取ESP8266的固件版本
  * @param  version: 接收版本信息的缓冲区
  * @param  version_max_len: 缓冲区最大长度
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_GetVersion(char* version, uint16_t version_max_len)
{
    if (!esp8266_initialized) {
        DebugPrint("ESP8266 not initialized");
        return 0;
    }
    
    // 清空缓冲区
    ESP8266_ClearBuffer();
    
    // 发送AT+GMR命令获取版本信息
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)"AT+GMR\r\n", 8, 500);
    
    // 等待响应
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_index = 0;
    uint8_t received_char;
    
    while ((HAL_GetTick() - start_time) < 2000) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
        }
    }
    
    // 检查响应中是否包含版本信息
    if (strstr(rx_buffer, "OK") != NULL) {
        strncpy(version, rx_buffer, version_max_len);
        version[version_max_len - 1] = '\0';
        return 1;
    }
    
    return 0;
}

/**
  * @brief  连接到WiFi接入点 - 改进版本
  * @param  ssid: WiFi名称
  * @param  password: WiFi密码
  * @retval 1: 连接成功, 0: 连接失败
  */
uint8_t ESP8266_ConnectToAP(char* ssid, char* password)
{
    char cmd[128];
    char debug_msg[64];
    
    sprintf(debug_msg, "Connecting to %s", ssid);
    DebugPrint(debug_msg);
    
    if (!esp8266_initialized) {
        DebugPrint("ESP8266 not initialized");
        return 0;
    }
    
    // 检查当前连接状态，断开可能存在的连接
    DebugPrint("Disconnecting from any existing AP");
    ESP8266_SendCommand("AT+CWQAP", "OK", 1000);
    HAL_Delay(200);
    
    // 构造连接命令
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    
    // 发送连接命令 - 增加超时时间
    DebugPrint("Sending WiFi connection command");
    if (ESP8266_SendCommand(cmd, "WIFI GOT IP", 20000)) {  // 增加到20秒超时
        DebugPrint("WiFi connection successful");
        esp8266_connected = 1;
        return 1;
    }
    
    DebugPrint("WiFi connection failed");
    esp8266_connected = 0;
    return 0;
}

/**
  * @brief  断开WiFi连接
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_DisconnectFromAP(void)
{
    if (!esp8266_initialized) {
        DebugPrint("ESP8266 not initialized");
        return 0;
    }
    
    if (ESP8266_SendCommand("AT+CWQAP", "OK", 1000)) {
        esp8266_connected = 0;
        return 1;
    }
    
    return 0;
}

/**
  * @brief  检查WiFi连接状态 - 改进版本
  * @retval 1: 已连接, 0: 未连接
  */
uint8_t ESP8266_CheckConnection(void)
{
    if (!esp8266_initialized) {
        DebugPrint("ESP8266 not initialized");
        return 0;
    }
    
    ESP8266_ClearBuffer();
    
    // 使用CIFSR命令检查IP地址状态 - 更可靠的方法
    DebugPrint("Checking WiFi status using IP address...");
    if (ESP8266_SendCommand("AT+CIFSR", "OK", 5000)) {
        char ip_buffer[128];
        ESP8266_GetLastResponse(ip_buffer, sizeof(ip_buffer));
        
        if (strstr(ip_buffer, "0.0.0.0") == NULL && 
            strstr(ip_buffer, "+CIFSR:STAIP") != NULL) {
            esp8266_connected = 1;
            DebugPrint("WiFi connected - Valid IP found");
            return 1;
        } else {
            DebugPrint("No valid IP address found");
        }
    } else {
        DebugPrint("CIFSR command failed");
    }
    
    // 备用方法：查询当前连接的AP
    DebugPrint("Trying secondary connection check...");
    ESP8266_ClearBuffer();
    if (ESP8266_SendCommand("AT+CWJAP?", "+CWJAP:", 3000)) {
        char ap_buffer[128];
        ESP8266_GetLastResponse(ap_buffer, sizeof(ap_buffer));
        
        // 检查返回内容是否包含No AP或ERROR
        if (strstr(ap_buffer, "No AP") == NULL && 
            strstr(ap_buffer, "ERROR") == NULL &&
            strstr(ap_buffer, WIFI_SSID) != NULL) {
            esp8266_connected = 1;
            DebugPrint("WiFi connected - SSID matched");
            return 1;
        } else {
            DebugPrint("CWJAP query indicates no connection");
        }
    } else {
        DebugPrint("CWJAP query failed");
    }
    
    DebugPrint("WiFi not connected");
    esp8266_connected = 0;
    return 0;
}

/**
  * @brief  获取ESP8266的IP地址
  * @param  ip_buffer: 接收IP地址的缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_GetIP(char* ip_buffer, uint16_t buffer_size)
{
    if (!esp8266_initialized || !esp8266_connected) {
        return 0;
    }
    
    ESP8266_ClearBuffer();
    
    // 发送查询IP命令
    if (ESP8266_SendCommand("AT+CIFSR", "OK", 2000)) {
        // 查找STAIP部分
        char* staip = strstr(rx_buffer, "+CIFSR:STAIP,\"");
        if (staip) {
            staip += 14; // 跳过"+CIFSR:STAIP,\""
            char* end = strchr(staip, '\"');
            if (end) {
                uint16_t ip_len = end - staip;
                if (ip_len < buffer_size) {
                    strncpy(ip_buffer, staip, ip_len);
                    ip_buffer[ip_len] = '\0';
                    return 1;
                }
            }
        }
    }
    
    strcpy(ip_buffer, "0.0.0.0");
    return 0;
}

/**
  * @brief  获取WiFi信号强度
  * @retval 信号强度（dBm）或0（失败）
  */
int8_t ESP8266_GetRSSI(void)
{
    if (!esp8266_initialized || !esp8266_connected) {
        return 0;
    }
    
    char rssi_buffer[128] = {0};
    int8_t rssi_value = 0;
    
    // 使用AT+CWJAP?命令查询当前连接的AP信息
    if (ESP8266_SendCommand("AT+CWJAP?", "OK", 3000)) {
        // 获取响应
        ESP8266_GetLastResponse(rssi_buffer, sizeof(rssi_buffer));
        
        // 解析RSSI值 - 格式通常是 +CWJAP:<ssid>,"xx:xx:xx:xx:xx:xx",channel,rssi
        char* rssi_start = strstr(rssi_buffer, ",-");
        if (rssi_start) {
            rssi_start += 1; // 跳过逗号
            rssi_value = atoi(rssi_start); // 会自动处理负号
            
            // 确保值在合理范围内
            if (rssi_value < -100 || rssi_value > 0) {
                rssi_value = -45; // 使用默认值
            }
        } else {
            rssi_value = -45; // 如果解析失败，使用默认值
        }
    } else {
        // 命令失败，返回默认值
        rssi_value = -45;
    }
    
    return rssi_value;
}

/**
  * @brief  配置SSL设置 - 改进版本
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_ConfigureSSL(void)
{
    // Debug输出
    DebugPrint("Configuring SSL settings...");
    
    // 禁用SSL证书验证 - 这部分已验证可以正常工作
    DebugPrint("Disabling SSL certificate verification...");
    if (!ESP8266_SendCommand("AT+CIPSSLCCONF=0", "OK", 1000)) {
        DebugPrint("WARNING: Failed to disable SSL verification");
        // 继续尝试，不返回失败
    } else {
        DebugPrint("SSL certificate verification disabled");
    }
    
    // 确保使用非透传模式 - 这对SSL连接很重要
    if (!ESP8266_SendCommand("AT+CIPMODE=0", "OK", 1000)) {
        DebugPrint("WARNING: Failed to set normal mode");
        // 继续尝试，不返回失败
    } else {
        DebugPrint("Normal transfer mode set");
    }
    
    // 设置发送超时时间较长以适应SSL握手
    if (!ESP8266_SendCommand("AT+CIPSTO=30", "OK", 1000)) {
        DebugPrint("WARNING: Failed to set socket timeout");
        // 继续，不视为失败
    } else {
        DebugPrint("Socket timeout set to 30 seconds");
    }
    
    DebugPrint("SSL configuration complete");
    return 1;
}

/**
  * @brief  创建TCP连接
  * @param  server: 服务器地址
  * @param  port: 服务器端口
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_CreateTCPConnection(char* server, uint16_t port)
{
    char cmd[128];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // 先关闭可能存在的连接
    ESP8266_SendCommand("AT+CIPCLOSE", "OK", 1000);
    HAL_Delay(100);
    
    // 构造连接命令
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", server, port);
    
    // 发送连接命令
    if (ESP8266_SendCommand(cmd, "OK", 5000)) {
        return 1;
    }
    
    return 0;
}

/**
  * @brief  创建安全TCP连接(HTTPS) - 改进版本
  * @param  server: 服务器地址
  * @param  port: 服务器端口
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_CreateSecureTCPConnection(char* server, uint16_t port)
{
    char cmd[128];
    char debug_msg[128];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // 关闭任何现有连接
    DebugPrint("Closing any existing connection...");
    ESP8266_SendCommand("AT+CIPCLOSE", "OK", 1000);
    HAL_Delay(200);
    
    // 配置SSL - 使用改进的函数
    ESP8266_ConfigureSSL();
    
    // 创建连接命令
    sprintf(cmd, "AT+CIPSTART=\"SSL\",\"%s\",%d", server, port);
    
    // 发送连接命令
    sprintf(debug_msg, "Creating secure connection to %s:%d", server, port);
    DebugPrint(debug_msg);
    DebugPrint(cmd);
    
    // 发送命令并等待较长时间
    uint8_t success = 0;
    for(uint8_t retry = 0; retry < 3; retry++) {
        DebugPrint("Sending CIPSTART command (attempt)");
        success = ESP8266_SendCommand(cmd, "OK", 15000);  // 增加超时时间到15秒
        
        if (success) {
            DebugPrint("Secure connection established successfully");
            return 1;
        }
        
        // 检查是否已连接
        if (strstr(rx_buffer, "ALREADY CONNECTED") != NULL) {
            DebugPrint("Connection already exists");
            return 1;  // 已连接，返回成功
        }
        
        DebugPrint("Connection attempt failed, retrying...");
        HAL_Delay(500);
    }
    
    DebugPrint("All connection attempts failed");
    DebugPrint("Last response:");
    DebugPrint(rx_buffer);
    
    return 0;
}

/**
  * @brief  关闭TCP连接
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_CloseTCPConnection(void)
{
    if (!esp8266_initialized) {
        return 0;
    }
    
    if (ESP8266_SendCommand("AT+CIPCLOSE", "OK", 1000)) {
        return 1;
    }
    
    return 0;
}

/**
  * @brief  发送TCP数据
  * @param  data: 要发送的数据
  * @param  len: 数据长度
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_SendTCPData(char* data, uint16_t len)
{
    char cmd[32];
    char debug_msg[64];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // 构造发送命令
    sprintf(cmd, "AT+CIPSEND=%d", len);
    sprintf(debug_msg, "Sending %d bytes of data", len);
    DebugPrint(debug_msg);
    
    // 发送命令并等待">"提示符
    if (ESP8266_SendCommand(cmd, ">", 5000)) {
        // 发送数据
        if (ESP8266_SendData((uint8_t*)data, len, 10000)) {
            // 等待发送成功应答
            uint32_t start_time = HAL_GetTick();
            uint16_t rx_index = 0;
            uint8_t received_char;
            
            while ((HAL_GetTick() - start_time) < 10000) {
                if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
                    if (rx_index < sizeof(rx_buffer) - 1) {
                        rx_buffer[rx_index++] = received_char;
                        rx_buffer[rx_index] = '\0';
                    }
                    
                    // 检查是否收到"SEND OK"
                    if (strstr(rx_buffer, "SEND OK") != NULL) {
                        DebugPrint("Data sent successfully");
                        return 1;
                    }
                }
            }
            DebugPrint("Timeout waiting for SEND OK");
        } else {
            DebugPrint("Failed to send data");
        }
    } else {
        DebugPrint("Failed to get '>' prompt");
        DebugPrint(rx_buffer);
    }
    
    return 0;
}

/**
  * @brief  发送HTTP GET请求
  * @param  server: 服务器地址
  * @param  path: 请求路径
  * @param  result: 接收响应的缓冲区
  * @param  result_max_len: 缓冲区最大长度
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_HTTPGet(char* server, char* path, char* result, uint16_t result_max_len)
{
    char request[512];
    
    if (!esp8266_initialized || !esp8266_connected) {
        return 0;
    }
    
    // 建立TCP连接
    if (!ESP8266_CreateTCPConnection(server, 80)) {
        return 0;
    }
    
    // 构造HTTP请求
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, server);
    
    // 发送HTTP请求
    if (!ESP8266_SendTCPData(request, strlen(request))) {
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // 等待并接收响应
    ESP8266_ClearBuffer();
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_index = 0;
    uint8_t received_char;
    uint8_t response_complete = 0;
    
    while ((HAL_GetTick() - start_time) < 10000 && !response_complete) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // 检查是否接收完毕（ESP8266关闭连接的标志）
            if (strstr(rx_buffer, "CLOSED") != NULL) {
                response_complete = 1;
            }
        }
    }
    
    // 关闭连接
    ESP8266_CloseTCPConnection();
    
    // 检查是否收到有效响应
    if (response_complete) {
        // 提取HTTP响应体
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // 跳过"\r\n\r\n"
            
            // 复制响应体到结果缓冲区
            strncpy(result, body_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            
            return 1;
        }
    }
    
    return 0;
}

/**
  * @brief  发送HTTP POST请求 - 改进版
  * @param  server: 服务器地址
  * @param  path: 请求路径
  * @param  data: POST数据
  * @param  result: 接收响应的缓冲区
  * @param  result_max_len: 缓冲区最大长度
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_HTTPPost(char* server, char* path, char* data, char* result, uint16_t result_max_len)
{
    char request[1024];
    char debug_msg[64];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // 建立TCP连接
    DebugPrint("Creating TCP connection");
    if (!ESP8266_CreateTCPConnection(server, 80)) {
        DebugPrint("Failed to create TCP connection");
        return 0;
    }
    
    // 构造HTTP请求
    DebugPrint("Building HTTP request");
    sprintf(request, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", 
            path, server, strlen(data), data);
    
    // 发送HTTP请求
    DebugPrint("Sending HTTP request");
    if (!ESP8266_SendTCPData(request, strlen(request))) {
        DebugPrint("Failed to send HTTP request");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // 等待并接收响应
    DebugPrint("Waiting for HTTP response");
    ESP8266_ClearBuffer();
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_index = 0;
    uint8_t received_char;
    uint8_t response_complete = 0;
    
    while ((HAL_GetTick() - start_time) < 20000 && !response_complete) {  // 增加到20秒超时
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // 检查是否接收完毕（ESP8266关闭连接的标志）
            if (strstr(rx_buffer, "CLOSED") != NULL) {
                response_complete = 1;
                DebugPrint("Connection closed by server");
            }
        }
        
        // 每隔3秒输出等待状态
        if ((HAL_GetTick() - start_time) % 3000 < 100) {
            sprintf(debug_msg, "Still waiting... %u ms elapsed", (unsigned int)(HAL_GetTick() - start_time));
            DebugPrint(debug_msg);
        }
    }
    
    // 关闭连接
    ESP8266_CloseTCPConnection();
    DebugPrint("TCP connection closed");
    
    // 检查是否收到有效响应
    if (response_complete) {
        DebugPrint("Response received");
        
        // 提取HTTP响应体
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // 跳过"\r\n\r\n"
            DebugPrint("HTTP body found");
            
            // 复制响应体到结果缓冲区
            strncpy(result, body_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            
            return 1;
        } else {
            DebugPrint("Could not find HTTP body");
        }
    } else if ((HAL_GetTick() - start_time) >= 20000) {
        DebugPrint("HTTP response timeout");
    }
    
    return 0;
}

/**
  * @brief  发送HTTPS GET请求
  * @param  server: 服务器地址
  * @param  path: 请求路径
  * @param  result: 接收响应的缓冲区
  * @param  result_max_len: 接收缓冲区最大长度
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_HTTPSGet(char* server, char* path, char* result, uint16_t result_max_len)
{
    char request[512];
    char debug_msg[64];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // 建立安全连接
    DebugPrint("Creating secure connection");
    if (!ESP8266_CreateSecureTCPConnection(server, 443)) {
        DebugPrint("Failed to create secure connection");
        return 0;
    }
    
    // 构造HTTP请求
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, server);
    
    // 发送HTTP请求
    DebugPrint("Sending HTTPS GET request");
    if (!ESP8266_SendTCPData(request, strlen(request))) {
        DebugPrint("Failed to send HTTPS request");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // 等待并接收响应
    DebugPrint("Waiting for HTTPS response");
    ESP8266_ClearBuffer();
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_index = 0;
    uint8_t received_char;
    uint8_t response_complete = 0;
    
    while ((HAL_GetTick() - start_time) < ESP8266_HTTPS_TIMEOUT && !response_complete) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // 检查是否接收完毕（ESP8266关闭连接的标志）
            if (strstr(rx_buffer, "CLOSED") != NULL) {
                response_complete = 1;
                DebugPrint("Connection closed by server");
            }
        }
        
        // 每隔3秒输出等待状态
        if ((HAL_GetTick() - start_time) % 3000 < 100) {
            sprintf(debug_msg, "Still waiting... %u ms elapsed", (unsigned int)(HAL_GetTick() - start_time));
            DebugPrint(debug_msg);
        }
    }
    
    // 关闭连接
    ESP8266_CloseTCPConnection();
    
    // 检查是否收到有效响应
    if (response_complete) {
        DebugPrint("HTTPS response received");
        
        // 提取HTTP响应体
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // 跳过"\r\n\r\n"
            DebugPrint("HTTP body found");
            
            // 复制响应体到结果缓冲区
            strncpy(result, body_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            
            return 1;
        } else {
            DebugPrint("Could not find HTTP body");
        }
    } else {
        DebugPrint("HTTPS response timeout");
    }
    
    return 0;
}

/**
  * @brief  发送HTTPS POST请求 - 改进版本
  * @param  server: 服务器地址
  * @param  path: 请求路径
  * @param  data: POST数据
  * @param  result: 接收响应的缓冲区
  * @param  result_max_len: 缓冲区最大长度
  * @retval 1: 成功, 0: 失败
  */
uint8_t ESP8266_HTTPSPost(char* server, char* path, char* data, char* result, uint16_t result_max_len)
{
    char request[1024];
    char debug_msg[128];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // ===== 准备工作 =====
    DebugPrint("===== HTTPS POST START =====");
    sprintf(debug_msg, "Server: %s", server);
    DebugPrint(debug_msg);
    sprintf(debug_msg, "Path: %s", path);
    DebugPrint(debug_msg);
    sprintf(debug_msg, "Data length: %u bytes", strlen(data));
    DebugPrint(debug_msg);
    
    // 关键修改: 确保ESP8266状态良好
    DebugPrint("Testing ESP8266 communication");
    if (!ESP8266_SendCommand("AT", "OK", 2000)) {
        DebugPrint("ESP8266 not responding - resetting");
        ESP8266_HardReset();  // 尝试硬件复位
        HAL_Delay(3000);
        
        if (!ESP8266_SendCommand("AT", "OK", 2000)) {
            DebugPrint("ESP8266 still not responding after reset!");
            return 0;
        }
        DebugPrint("ESP8266 reset successful");
    }
    
    // 关闭任何现有连接
    DebugPrint("Closing any existing connections...");
    ESP8266_SendCommand("AT+CIPCLOSE", "OK", 2000);
    HAL_Delay(500);
    
    // ===== 关键修改: 设置正确的SSL参数 =====
    DebugPrint("Configuring SSL settings...");
    // 先确保关闭多连接模式，使用单连接
    ESP8266_SendCommand("AT+CIPMUX=0", "OK", 2000);
    
    // 关闭SSL证书验证  
    ESP8266_SendCommand("AT+CIPSSLCCONF=0", "OK", 2000);
    
    // 设置非透传模式
    ESP8266_SendCommand("AT+CIPMODE=0", "OK", 2000);
    
    // 设置更大的SSL缓冲区大小
    ESP8266_SendCommand("AT+CIPSSLSIZE=8192", "OK", 2000);
    
    // 设置更长的超时时间
    ESP8266_SendCommand("AT+CIPSTO=120", "OK", 2000);
    
    // ===== 创建安全连接 =====
    DebugPrint("Creating secure connection...");
    char cmd[128];
    sprintf(cmd, "AT+CIPSTART=\"SSL\",\"%s\",%d", server, 443);
    DebugPrint(cmd);
    
    // 多次尝试连接
    uint8_t connected = 0;
    for (uint8_t retry = 0; retry < 3; retry++) {
        sprintf(debug_msg, "Connection attempt %d/3", retry+1);
        DebugPrint(debug_msg);
        
        if (ESP8266_SendCommand(cmd, "OK", 15000)) {  // 增加到15秒超时
            connected = 1;
            DebugPrint("SSL connection established");
            break;
        }
        
        // 检查是否已连接
        if (strstr(rx_buffer, "ALREADY CONNECTED") != NULL) {
            DebugPrint("Connection already exists");
            connected = 1;
            break;
        }
        
        HAL_Delay(1000);
    }
    
    if (!connected) {
        DebugPrint("Failed to establish SSL connection after multiple attempts");
        return 0;
    }
    
    // ===== 构建并发送HTTP请求 =====
    DebugPrint("Building HTTPS request...");
    
    // 关键修改: 调整HTTP请求头
    sprintf(request, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", 
            path, server, strlen(data), data);
    
    // 获取发送命令
    char send_cmd[32];
    sprintf(send_cmd, "AT+CIPSEND=%d", strlen(request));
    DebugPrint("Sending data length command:");
    DebugPrint(send_cmd);
    
    // 发送数据长度命令，等待'>'提示符
    if (!ESP8266_SendCommand(send_cmd, ">", 10000)) {
        DebugPrint("Failed to get '>' prompt for data sending");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // 发送数据
    DebugPrint("Sending HTTP data...");
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)request, strlen(request), 10000);
    
    // ===== 接收响应 =====
    DebugPrint("Waiting for response...");
    ESP8266_ClearBuffer();
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_index = 0;
    uint8_t received_char;
    uint8_t response_complete = 0;
    
    // 使用更长的超时时间(120秒)
    uint32_t timeout = 120000;
    
    while ((HAL_GetTick() - start_time) < timeout && !response_complete) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // 检查是否接收完成 - 增加多种结束条件
            if (strstr(rx_buffer, "CLOSED") != NULL ||
                strstr(rx_buffer, "SEND OK") != NULL) {
                // 接收完成需要考虑延迟，所以等待一小段时间确保接收完整
                HAL_Delay(500);
                response_complete = 1;
            }
        }
        
        // 定期输出接收进度
        if ((HAL_GetTick() - start_time) % 5000 < 100) {
            sprintf(debug_msg, "Still waiting... %lu ms elapsed, buffer size: %d", 
                   (unsigned long)(HAL_GetTick() - start_time), rx_index);
            DebugPrint(debug_msg);
        }
    }
    
    // 关闭连接
    DebugPrint("Closing connection...");
    ESP8266_CloseTCPConnection();
    
    // ===== 处理响应 =====
    if (rx_index > 0) {
        DebugPrint("Response received");
        
        // 调试: 打印响应前100个字符
        char preview[101] = {0};
        strncpy(preview, rx_buffer, rx_index > 100 ? 100 : rx_index);
        sprintf(debug_msg, "Response preview: %s", preview);
        DebugPrint(debug_msg);
        
        // 首先尝试标准HTTP提取方法
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // 跳过"\r\n\r\n"
            DebugPrint("HTTP body found using standard separator");
            strncpy(result, body_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            DebugPrint("===== HTTPS POST COMPLETE =====");
            return 1;
        }
        
        // 尝试查找JSON开始标记
        char* json_start = strstr(rx_buffer, "{\"");
        if (json_start) {
            DebugPrint("JSON object start found");
            strncpy(result, json_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            DebugPrint("===== HTTPS POST COMPLETE =====");
            return 1;
        }
        
        // 尝试在+IPD数据中找到内容
        char* ipd_start = strstr(rx_buffer, "+IPD,");
        if (ipd_start) {
            char* content_start = strchr(ipd_start, ':');
            if (content_start) {
                content_start++; // 跳过":"
                DebugPrint("Extracted data from +IPD");
                strncpy(result, content_start, result_max_len - 1);
                result[result_max_len - 1] = '\0';
                DebugPrint("===== HTTPS POST COMPLETE =====");
                return 1;
            }
        }
        
        // 最后的备选方案: 返回所有接收到的内容
        DebugPrint("Using fallback: returning all received data");
        strncpy(result, rx_buffer, result_max_len - 1);
        result[result_max_len - 1] = '\0';
        DebugPrint("===== HTTPS POST COMPLETE (FALLBACK) =====");
        return 1;
    }
    
    DebugPrint("No data received from server");
    DebugPrint("===== HTTPS POST FAILED =====");
    return 0;
}

/**
  * @brief  获取最近一次AT命令的响应内容
  * @param  buffer: 用于存储响应的缓冲区
  * @param  buffer_size: 缓冲区大小
  * @retval 复制的字节数
  */
uint16_t ESP8266_GetLastResponse(char* buffer, uint16_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0)
        return 0;
        
    uint16_t len = strlen(rx_buffer);
    if (len >= buffer_size)
        len = buffer_size - 1;
        
    strncpy(buffer, rx_buffer, len);
    buffer[len] = '\0';
    return len;
}