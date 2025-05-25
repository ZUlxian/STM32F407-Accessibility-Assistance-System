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

/* ȫ�ֱ��� */
uint8_t esp8266_initialized = 0;
uint8_t esp8266_connected = 0;

static char rx_buffer[2048];
static UART_HandleTypeDef *esp8266_uart;

/* �ⲿ���õ��Դ��� */
extern UART_HandleTypeDef huart1;
extern void ESP8266_HardReset(void);

/* �򻯰������� */
void DebugPrint(const char* msg) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"[ESP] ", 6, 100);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 300);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
    HAL_Delay(10); // ����һ���ӳ٣�ȷ�������������
}

/**
  * @brief  ��ս��ջ�����
  */
void ESP8266_ClearBuffer(void)
{
    uint8_t dummy;
    uint32_t timeout = HAL_GetTick() + 100; // 100ms��ʱ
    
    // ��ȡ���������д����������
    while (HAL_GetTick() < timeout) {
        if (HAL_UART_Receive(esp8266_uart, &dummy, 1, 1) != HAL_OK) {
            break;
        }
    }
    
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

/**
  * @brief  ����AT����ȴ���Ӧ
  * @param  command: Ҫ���͵�AT�����ַ���
  * @param  response: �������յ�����Ӧ�ַ�����NULL��ʾ�������Ӧ��
  * @param  timeout: �ȴ���Ӧ�ĳ�ʱʱ�䣨���룩
  * @retval 1: �ɹ��յ�������Ӧ, 0: ʧ�ܻ�ʱ
  */
uint8_t ESP8266_SendCommand(char* command, char* response, uint32_t timeout)
{
    uint32_t start_time;
    uint16_t rx_index = 0;
    uint8_t received_char;
    
    // ��ս��ջ�����
    ESP8266_ClearBuffer();
    
    // ����AT����
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)command, strlen(command), 500);
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)"\r\n", 2, 100);
    
    // �������Ҫ�����Ӧ
    if (response == NULL) {
        HAL_Delay(50); // ��ģ��һЩ����ʱ��
        return 1;
    }
    
    // �ȴ���Ӧ
    start_time = HAL_GetTick();
    while ((HAL_GetTick() - start_time) < timeout) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // ����Ƿ����������Ӧ
            if (strstr(rx_buffer, response) != NULL) {
                // �ɹ�
                return 1;
            }
        }
    }
    
    // ��ʾ��ʱ����ͽ��յ�������
    if (timeout > 1000) { // ֻ�г���ʱ�����������Ϣ
        DebugPrint("Command timeout. Received:");
        if (rx_index > 0) {
            DebugPrint(rx_buffer);
        } else {
            DebugPrint("(No data)");
        }
    }
    
    // ��ʱ��δ�յ�������Ӧ
    return 0;
}

/**
  * @brief  ��������
  * @param  data: Ҫ���͵�����
  * @param  len: ���ݳ���
  * @param  timeout: ��ʱʱ�䣨���룩
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_SendData(uint8_t* data, uint16_t len, uint32_t timeout)
{
    if (HAL_UART_Transmit(esp8266_uart, data, len, timeout) == HAL_OK) {
        return 1;
    }
    return 0;
}

/**
  * @brief  ��������
  * @param  buffer: ���ջ�����
  * @param  max_len: ��������󳤶�
  * @param  timeout: ��ʱʱ�䣨���룩
  * @retval ���յ������ݳ���
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
  * @brief  ��ʼ��ESP8266
  * @param  huart: ��ESP8266���ӵ�UART���
  * @retval 1: ��ʼ���ɹ�, 0: ��ʼ��ʧ��
  */
uint8_t ESP8266_Init(UART_HandleTypeDef *huart)
{
    uint8_t retry_count;
    char debug_msg[64];
    
    // �洢UART���
    esp8266_uart = huart;
    
    DebugPrint("Initializing ESP8266...");
    
    // ��ս��ջ�����
    ESP8266_ClearBuffer();
    
    // ��ģ���ϵ����ȶ�ʱ��
    HAL_Delay(1000);
    
    // ������ESP8266ͨ�� - ���ȳ���������ȷ������״̬
    ESP8266_SendCommand("AT+RST", NULL, 1000);
    DebugPrint("ESP8266 reset sent");
    
    // �ȴ�ģ���������
    HAL_Delay(3000);
    
    // ������ESP8266ͨ��
    for (retry_count = 0; retry_count < 5; retry_count++) {
        sprintf(debug_msg, "AT test attempt %d", retry_count+1);
        DebugPrint(debug_msg);
        
        if (ESP8266_SendCommand("AT", "OK", 1000)) {
            // ͨ�ųɹ�����������
            DebugPrint("ESP8266 communication established");
            break;
        }
        
        HAL_Delay(500);
    }
    
    // ���5�γ��Զ�ʧ�ܣ����ش���
    if (retry_count >= 5) {
        DebugPrint("ESP8266 communication failed");
        esp8266_initialized = 0;
        return 0;
    }
    
    // ���ù���ģʽΪStationģʽ���ͻ��ˣ�
    DebugPrint("Setting Station mode");
    if (!ESP8266_SetMode(1)) {
        DebugPrint("Failed to set Station mode");
        esp8266_initialized = 0;
        return 0;
    }
    
    // ��������ģʽΪ������ģʽ
    DebugPrint("Setting single connection mode");
    if (!ESP8266_SendCommand("AT+CIPMUX=0", "OK", 1000)) {
        DebugPrint("Failed to set single connection mode");
        esp8266_initialized = 0;
        return 0;
    }
    
    // �����Զ�����
    ESP8266_SendCommand("AT+CWAUTOCONN=0", "OK", 1000);
    
    // ���SSL֧�� - �޸ģ��Ƴ�AT+CIPSSLSIZE�����Ϊ�����ܵ��´���
    DebugPrint("Checking SSL support...");
    
    // ����SSL֤����֤
    if (!ESP8266_SendCommand("AT+CIPSSLCCONF=0", "OK", 1000)) {
        DebugPrint("Warning: Disabling SSL certificate verification failed");
    } else {
        DebugPrint("SSL certificate verification disabled");
    }
    
    // ��ʼ���ɹ�
    DebugPrint("ESP8266 initialization complete");
    esp8266_initialized = 1;
    return 1;
}

/**
  * @brief  ����ESP8266����ģʽ
  * @param  mode: 1=Stationģʽ, 2=APģʽ, 3=���߼���
  * @retval 1: �ɹ�, 0: ʧ��
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
  * @brief  ����ESP8266ģ��
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_Restart(void)
{
    DebugPrint("Restarting ESP8266");
    
    // ������������
    if (!ESP8266_SendCommand("AT+RST", NULL, 1000)) {
        DebugPrint("Failed to send restart command");
        return 0;
    }
    
    // �ȴ�ģ������
    DebugPrint("Waiting for ESP8266 to restart...");
    HAL_Delay(2000);
    
    // ���ͨ��
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
  * @brief  ��ȡESP8266�Ĺ̼��汾
  * @param  version: ���հ汾��Ϣ�Ļ�����
  * @param  version_max_len: ��������󳤶�
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_GetVersion(char* version, uint16_t version_max_len)
{
    if (!esp8266_initialized) {
        DebugPrint("ESP8266 not initialized");
        return 0;
    }
    
    // ��ջ�����
    ESP8266_ClearBuffer();
    
    // ����AT+GMR�����ȡ�汾��Ϣ
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)"AT+GMR\r\n", 8, 500);
    
    // �ȴ���Ӧ
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
    
    // �����Ӧ���Ƿ�����汾��Ϣ
    if (strstr(rx_buffer, "OK") != NULL) {
        strncpy(version, rx_buffer, version_max_len);
        version[version_max_len - 1] = '\0';
        return 1;
    }
    
    return 0;
}

/**
  * @brief  ���ӵ�WiFi����� - �Ľ��汾
  * @param  ssid: WiFi����
  * @param  password: WiFi����
  * @retval 1: ���ӳɹ�, 0: ����ʧ��
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
    
    // ��鵱ǰ����״̬���Ͽ����ܴ��ڵ�����
    DebugPrint("Disconnecting from any existing AP");
    ESP8266_SendCommand("AT+CWQAP", "OK", 1000);
    HAL_Delay(200);
    
    // ������������
    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    
    // ������������ - ���ӳ�ʱʱ��
    DebugPrint("Sending WiFi connection command");
    if (ESP8266_SendCommand(cmd, "WIFI GOT IP", 20000)) {  // ���ӵ�20�볬ʱ
        DebugPrint("WiFi connection successful");
        esp8266_connected = 1;
        return 1;
    }
    
    DebugPrint("WiFi connection failed");
    esp8266_connected = 0;
    return 0;
}

/**
  * @brief  �Ͽ�WiFi����
  * @retval 1: �ɹ�, 0: ʧ��
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
  * @brief  ���WiFi����״̬ - �Ľ��汾
  * @retval 1: ������, 0: δ����
  */
uint8_t ESP8266_CheckConnection(void)
{
    if (!esp8266_initialized) {
        DebugPrint("ESP8266 not initialized");
        return 0;
    }
    
    ESP8266_ClearBuffer();
    
    // ʹ��CIFSR������IP��ַ״̬ - ���ɿ��ķ���
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
    
    // ���÷�������ѯ��ǰ���ӵ�AP
    DebugPrint("Trying secondary connection check...");
    ESP8266_ClearBuffer();
    if (ESP8266_SendCommand("AT+CWJAP?", "+CWJAP:", 3000)) {
        char ap_buffer[128];
        ESP8266_GetLastResponse(ap_buffer, sizeof(ap_buffer));
        
        // ��鷵�������Ƿ����No AP��ERROR
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
  * @brief  ��ȡESP8266��IP��ַ
  * @param  ip_buffer: ����IP��ַ�Ļ�����
  * @param  buffer_size: ��������С
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_GetIP(char* ip_buffer, uint16_t buffer_size)
{
    if (!esp8266_initialized || !esp8266_connected) {
        return 0;
    }
    
    ESP8266_ClearBuffer();
    
    // ���Ͳ�ѯIP����
    if (ESP8266_SendCommand("AT+CIFSR", "OK", 2000)) {
        // ����STAIP����
        char* staip = strstr(rx_buffer, "+CIFSR:STAIP,\"");
        if (staip) {
            staip += 14; // ����"+CIFSR:STAIP,\""
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
  * @brief  ��ȡWiFi�ź�ǿ��
  * @retval �ź�ǿ�ȣ�dBm����0��ʧ�ܣ�
  */
int8_t ESP8266_GetRSSI(void)
{
    if (!esp8266_initialized || !esp8266_connected) {
        return 0;
    }
    
    char rssi_buffer[128] = {0};
    int8_t rssi_value = 0;
    
    // ʹ��AT+CWJAP?�����ѯ��ǰ���ӵ�AP��Ϣ
    if (ESP8266_SendCommand("AT+CWJAP?", "OK", 3000)) {
        // ��ȡ��Ӧ
        ESP8266_GetLastResponse(rssi_buffer, sizeof(rssi_buffer));
        
        // ����RSSIֵ - ��ʽͨ���� +CWJAP:<ssid>,"xx:xx:xx:xx:xx:xx",channel,rssi
        char* rssi_start = strstr(rssi_buffer, ",-");
        if (rssi_start) {
            rssi_start += 1; // ��������
            rssi_value = atoi(rssi_start); // ���Զ�������
            
            // ȷ��ֵ�ں���Χ��
            if (rssi_value < -100 || rssi_value > 0) {
                rssi_value = -45; // ʹ��Ĭ��ֵ
            }
        } else {
            rssi_value = -45; // �������ʧ�ܣ�ʹ��Ĭ��ֵ
        }
    } else {
        // ����ʧ�ܣ�����Ĭ��ֵ
        rssi_value = -45;
    }
    
    return rssi_value;
}

/**
  * @brief  ����SSL���� - �Ľ��汾
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_ConfigureSSL(void)
{
    // Debug���
    DebugPrint("Configuring SSL settings...");
    
    // ����SSL֤����֤ - �ⲿ������֤������������
    DebugPrint("Disabling SSL certificate verification...");
    if (!ESP8266_SendCommand("AT+CIPSSLCCONF=0", "OK", 1000)) {
        DebugPrint("WARNING: Failed to disable SSL verification");
        // �������ԣ�������ʧ��
    } else {
        DebugPrint("SSL certificate verification disabled");
    }
    
    // ȷ��ʹ�÷�͸��ģʽ - ���SSL���Ӻ���Ҫ
    if (!ESP8266_SendCommand("AT+CIPMODE=0", "OK", 1000)) {
        DebugPrint("WARNING: Failed to set normal mode");
        // �������ԣ�������ʧ��
    } else {
        DebugPrint("Normal transfer mode set");
    }
    
    // ���÷��ͳ�ʱʱ��ϳ�����ӦSSL����
    if (!ESP8266_SendCommand("AT+CIPSTO=30", "OK", 1000)) {
        DebugPrint("WARNING: Failed to set socket timeout");
        // ����������Ϊʧ��
    } else {
        DebugPrint("Socket timeout set to 30 seconds");
    }
    
    DebugPrint("SSL configuration complete");
    return 1;
}

/**
  * @brief  ����TCP����
  * @param  server: ��������ַ
  * @param  port: �������˿�
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_CreateTCPConnection(char* server, uint16_t port)
{
    char cmd[128];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // �ȹرտ��ܴ��ڵ�����
    ESP8266_SendCommand("AT+CIPCLOSE", "OK", 1000);
    HAL_Delay(100);
    
    // ������������
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", server, port);
    
    // ������������
    if (ESP8266_SendCommand(cmd, "OK", 5000)) {
        return 1;
    }
    
    return 0;
}

/**
  * @brief  ������ȫTCP����(HTTPS) - �Ľ��汾
  * @param  server: ��������ַ
  * @param  port: �������˿�
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_CreateSecureTCPConnection(char* server, uint16_t port)
{
    char cmd[128];
    char debug_msg[128];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // �ر��κ���������
    DebugPrint("Closing any existing connection...");
    ESP8266_SendCommand("AT+CIPCLOSE", "OK", 1000);
    HAL_Delay(200);
    
    // ����SSL - ʹ�øĽ��ĺ���
    ESP8266_ConfigureSSL();
    
    // ������������
    sprintf(cmd, "AT+CIPSTART=\"SSL\",\"%s\",%d", server, port);
    
    // ������������
    sprintf(debug_msg, "Creating secure connection to %s:%d", server, port);
    DebugPrint(debug_msg);
    DebugPrint(cmd);
    
    // ��������ȴ��ϳ�ʱ��
    uint8_t success = 0;
    for(uint8_t retry = 0; retry < 3; retry++) {
        DebugPrint("Sending CIPSTART command (attempt)");
        success = ESP8266_SendCommand(cmd, "OK", 15000);  // ���ӳ�ʱʱ�䵽15��
        
        if (success) {
            DebugPrint("Secure connection established successfully");
            return 1;
        }
        
        // ����Ƿ�������
        if (strstr(rx_buffer, "ALREADY CONNECTED") != NULL) {
            DebugPrint("Connection already exists");
            return 1;  // �����ӣ����سɹ�
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
  * @brief  �ر�TCP����
  * @retval 1: �ɹ�, 0: ʧ��
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
  * @brief  ����TCP����
  * @param  data: Ҫ���͵�����
  * @param  len: ���ݳ���
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_SendTCPData(char* data, uint16_t len)
{
    char cmd[32];
    char debug_msg[64];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // ���췢������
    sprintf(cmd, "AT+CIPSEND=%d", len);
    sprintf(debug_msg, "Sending %d bytes of data", len);
    DebugPrint(debug_msg);
    
    // ��������ȴ�">"��ʾ��
    if (ESP8266_SendCommand(cmd, ">", 5000)) {
        // ��������
        if (ESP8266_SendData((uint8_t*)data, len, 10000)) {
            // �ȴ����ͳɹ�Ӧ��
            uint32_t start_time = HAL_GetTick();
            uint16_t rx_index = 0;
            uint8_t received_char;
            
            while ((HAL_GetTick() - start_time) < 10000) {
                if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
                    if (rx_index < sizeof(rx_buffer) - 1) {
                        rx_buffer[rx_index++] = received_char;
                        rx_buffer[rx_index] = '\0';
                    }
                    
                    // ����Ƿ��յ�"SEND OK"
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
  * @brief  ����HTTP GET����
  * @param  server: ��������ַ
  * @param  path: ����·��
  * @param  result: ������Ӧ�Ļ�����
  * @param  result_max_len: ��������󳤶�
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_HTTPGet(char* server, char* path, char* result, uint16_t result_max_len)
{
    char request[512];
    
    if (!esp8266_initialized || !esp8266_connected) {
        return 0;
    }
    
    // ����TCP����
    if (!ESP8266_CreateTCPConnection(server, 80)) {
        return 0;
    }
    
    // ����HTTP����
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, server);
    
    // ����HTTP����
    if (!ESP8266_SendTCPData(request, strlen(request))) {
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // �ȴ���������Ӧ
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
            
            // ����Ƿ������ϣ�ESP8266�ر����ӵı�־��
            if (strstr(rx_buffer, "CLOSED") != NULL) {
                response_complete = 1;
            }
        }
    }
    
    // �ر�����
    ESP8266_CloseTCPConnection();
    
    // ����Ƿ��յ���Ч��Ӧ
    if (response_complete) {
        // ��ȡHTTP��Ӧ��
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // ����"\r\n\r\n"
            
            // ������Ӧ�嵽���������
            strncpy(result, body_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            
            return 1;
        }
    }
    
    return 0;
}

/**
  * @brief  ����HTTP POST���� - �Ľ���
  * @param  server: ��������ַ
  * @param  path: ����·��
  * @param  data: POST����
  * @param  result: ������Ӧ�Ļ�����
  * @param  result_max_len: ��������󳤶�
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_HTTPPost(char* server, char* path, char* data, char* result, uint16_t result_max_len)
{
    char request[1024];
    char debug_msg[64];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // ����TCP����
    DebugPrint("Creating TCP connection");
    if (!ESP8266_CreateTCPConnection(server, 80)) {
        DebugPrint("Failed to create TCP connection");
        return 0;
    }
    
    // ����HTTP����
    DebugPrint("Building HTTP request");
    sprintf(request, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", 
            path, server, strlen(data), data);
    
    // ����HTTP����
    DebugPrint("Sending HTTP request");
    if (!ESP8266_SendTCPData(request, strlen(request))) {
        DebugPrint("Failed to send HTTP request");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // �ȴ���������Ӧ
    DebugPrint("Waiting for HTTP response");
    ESP8266_ClearBuffer();
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_index = 0;
    uint8_t received_char;
    uint8_t response_complete = 0;
    
    while ((HAL_GetTick() - start_time) < 20000 && !response_complete) {  // ���ӵ�20�볬ʱ
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // ����Ƿ������ϣ�ESP8266�ر����ӵı�־��
            if (strstr(rx_buffer, "CLOSED") != NULL) {
                response_complete = 1;
                DebugPrint("Connection closed by server");
            }
        }
        
        // ÿ��3������ȴ�״̬
        if ((HAL_GetTick() - start_time) % 3000 < 100) {
            sprintf(debug_msg, "Still waiting... %u ms elapsed", (unsigned int)(HAL_GetTick() - start_time));
            DebugPrint(debug_msg);
        }
    }
    
    // �ر�����
    ESP8266_CloseTCPConnection();
    DebugPrint("TCP connection closed");
    
    // ����Ƿ��յ���Ч��Ӧ
    if (response_complete) {
        DebugPrint("Response received");
        
        // ��ȡHTTP��Ӧ��
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // ����"\r\n\r\n"
            DebugPrint("HTTP body found");
            
            // ������Ӧ�嵽���������
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
  * @brief  ����HTTPS GET����
  * @param  server: ��������ַ
  * @param  path: ����·��
  * @param  result: ������Ӧ�Ļ�����
  * @param  result_max_len: ���ջ�������󳤶�
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_HTTPSGet(char* server, char* path, char* result, uint16_t result_max_len)
{
    char request[512];
    char debug_msg[64];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // ������ȫ����
    DebugPrint("Creating secure connection");
    if (!ESP8266_CreateSecureTCPConnection(server, 443)) {
        DebugPrint("Failed to create secure connection");
        return 0;
    }
    
    // ����HTTP����
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, server);
    
    // ����HTTP����
    DebugPrint("Sending HTTPS GET request");
    if (!ESP8266_SendTCPData(request, strlen(request))) {
        DebugPrint("Failed to send HTTPS request");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // �ȴ���������Ӧ
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
            
            // ����Ƿ������ϣ�ESP8266�ر����ӵı�־��
            if (strstr(rx_buffer, "CLOSED") != NULL) {
                response_complete = 1;
                DebugPrint("Connection closed by server");
            }
        }
        
        // ÿ��3������ȴ�״̬
        if ((HAL_GetTick() - start_time) % 3000 < 100) {
            sprintf(debug_msg, "Still waiting... %u ms elapsed", (unsigned int)(HAL_GetTick() - start_time));
            DebugPrint(debug_msg);
        }
    }
    
    // �ر�����
    ESP8266_CloseTCPConnection();
    
    // ����Ƿ��յ���Ч��Ӧ
    if (response_complete) {
        DebugPrint("HTTPS response received");
        
        // ��ȡHTTP��Ӧ��
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // ����"\r\n\r\n"
            DebugPrint("HTTP body found");
            
            // ������Ӧ�嵽���������
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
  * @brief  ����HTTPS POST���� - �Ľ��汾
  * @param  server: ��������ַ
  * @param  path: ����·��
  * @param  data: POST����
  * @param  result: ������Ӧ�Ļ�����
  * @param  result_max_len: ��������󳤶�
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t ESP8266_HTTPSPost(char* server, char* path, char* data, char* result, uint16_t result_max_len)
{
    char request[1024];
    char debug_msg[128];
    
    if (!esp8266_initialized || !esp8266_connected) {
        DebugPrint("ESP8266 not initialized or connected");
        return 0;
    }
    
    // ===== ׼������ =====
    DebugPrint("===== HTTPS POST START =====");
    sprintf(debug_msg, "Server: %s", server);
    DebugPrint(debug_msg);
    sprintf(debug_msg, "Path: %s", path);
    DebugPrint(debug_msg);
    sprintf(debug_msg, "Data length: %u bytes", strlen(data));
    DebugPrint(debug_msg);
    
    // �ؼ��޸�: ȷ��ESP8266״̬����
    DebugPrint("Testing ESP8266 communication");
    if (!ESP8266_SendCommand("AT", "OK", 2000)) {
        DebugPrint("ESP8266 not responding - resetting");
        ESP8266_HardReset();  // ����Ӳ����λ
        HAL_Delay(3000);
        
        if (!ESP8266_SendCommand("AT", "OK", 2000)) {
            DebugPrint("ESP8266 still not responding after reset!");
            return 0;
        }
        DebugPrint("ESP8266 reset successful");
    }
    
    // �ر��κ���������
    DebugPrint("Closing any existing connections...");
    ESP8266_SendCommand("AT+CIPCLOSE", "OK", 2000);
    HAL_Delay(500);
    
    // ===== �ؼ��޸�: ������ȷ��SSL���� =====
    DebugPrint("Configuring SSL settings...");
    // ��ȷ���رն�����ģʽ��ʹ�õ�����
    ESP8266_SendCommand("AT+CIPMUX=0", "OK", 2000);
    
    // �ر�SSL֤����֤  
    ESP8266_SendCommand("AT+CIPSSLCCONF=0", "OK", 2000);
    
    // ���÷�͸��ģʽ
    ESP8266_SendCommand("AT+CIPMODE=0", "OK", 2000);
    
    // ���ø����SSL��������С
    ESP8266_SendCommand("AT+CIPSSLSIZE=8192", "OK", 2000);
    
    // ���ø����ĳ�ʱʱ��
    ESP8266_SendCommand("AT+CIPSTO=120", "OK", 2000);
    
    // ===== ������ȫ���� =====
    DebugPrint("Creating secure connection...");
    char cmd[128];
    sprintf(cmd, "AT+CIPSTART=\"SSL\",\"%s\",%d", server, 443);
    DebugPrint(cmd);
    
    // ��γ�������
    uint8_t connected = 0;
    for (uint8_t retry = 0; retry < 3; retry++) {
        sprintf(debug_msg, "Connection attempt %d/3", retry+1);
        DebugPrint(debug_msg);
        
        if (ESP8266_SendCommand(cmd, "OK", 15000)) {  // ���ӵ�15�볬ʱ
            connected = 1;
            DebugPrint("SSL connection established");
            break;
        }
        
        // ����Ƿ�������
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
    
    // ===== ����������HTTP���� =====
    DebugPrint("Building HTTPS request...");
    
    // �ؼ��޸�: ����HTTP����ͷ
    sprintf(request, "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s", 
            path, server, strlen(data), data);
    
    // ��ȡ��������
    char send_cmd[32];
    sprintf(send_cmd, "AT+CIPSEND=%d", strlen(request));
    DebugPrint("Sending data length command:");
    DebugPrint(send_cmd);
    
    // �������ݳ�������ȴ�'>'��ʾ��
    if (!ESP8266_SendCommand(send_cmd, ">", 10000)) {
        DebugPrint("Failed to get '>' prompt for data sending");
        ESP8266_CloseTCPConnection();
        return 0;
    }
    
    // ��������
    DebugPrint("Sending HTTP data...");
    HAL_UART_Transmit(esp8266_uart, (uint8_t*)request, strlen(request), 10000);
    
    // ===== ������Ӧ =====
    DebugPrint("Waiting for response...");
    ESP8266_ClearBuffer();
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_index = 0;
    uint8_t received_char;
    uint8_t response_complete = 0;
    
    // ʹ�ø����ĳ�ʱʱ��(120��)
    uint32_t timeout = 120000;
    
    while ((HAL_GetTick() - start_time) < timeout && !response_complete) {
        if (HAL_UART_Receive(esp8266_uart, &received_char, 1, 10) == HAL_OK) {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = received_char;
                rx_buffer[rx_index] = '\0';
            }
            
            // ����Ƿ������� - ���Ӷ��ֽ�������
            if (strstr(rx_buffer, "CLOSED") != NULL ||
                strstr(rx_buffer, "SEND OK") != NULL) {
                // ���������Ҫ�����ӳ٣����Եȴ�һС��ʱ��ȷ����������
                HAL_Delay(500);
                response_complete = 1;
            }
        }
        
        // ����������ս���
        if ((HAL_GetTick() - start_time) % 5000 < 100) {
            sprintf(debug_msg, "Still waiting... %lu ms elapsed, buffer size: %d", 
                   (unsigned long)(HAL_GetTick() - start_time), rx_index);
            DebugPrint(debug_msg);
        }
    }
    
    // �ر�����
    DebugPrint("Closing connection...");
    ESP8266_CloseTCPConnection();
    
    // ===== ������Ӧ =====
    if (rx_index > 0) {
        DebugPrint("Response received");
        
        // ����: ��ӡ��Ӧǰ100���ַ�
        char preview[101] = {0};
        strncpy(preview, rx_buffer, rx_index > 100 ? 100 : rx_index);
        sprintf(debug_msg, "Response preview: %s", preview);
        DebugPrint(debug_msg);
        
        // ���ȳ��Ա�׼HTTP��ȡ����
        char* body_start = strstr(rx_buffer, "\r\n\r\n");
        if (body_start) {
            body_start += 4; // ����"\r\n\r\n"
            DebugPrint("HTTP body found using standard separator");
            strncpy(result, body_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            DebugPrint("===== HTTPS POST COMPLETE =====");
            return 1;
        }
        
        // ���Բ���JSON��ʼ���
        char* json_start = strstr(rx_buffer, "{\"");
        if (json_start) {
            DebugPrint("JSON object start found");
            strncpy(result, json_start, result_max_len - 1);
            result[result_max_len - 1] = '\0';
            DebugPrint("===== HTTPS POST COMPLETE =====");
            return 1;
        }
        
        // ������+IPD�������ҵ�����
        char* ipd_start = strstr(rx_buffer, "+IPD,");
        if (ipd_start) {
            char* content_start = strchr(ipd_start, ':');
            if (content_start) {
                content_start++; // ����":"
                DebugPrint("Extracted data from +IPD");
                strncpy(result, content_start, result_max_len - 1);
                result[result_max_len - 1] = '\0';
                DebugPrint("===== HTTPS POST COMPLETE =====");
                return 1;
            }
        }
        
        // ���ı�ѡ����: �������н��յ�������
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
  * @brief  ��ȡ���һ��AT�������Ӧ����
  * @param  buffer: ���ڴ洢��Ӧ�Ļ�����
  * @param  buffer_size: ��������С
  * @retval ���Ƶ��ֽ���
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