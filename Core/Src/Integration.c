/**
  ******************************************************************************
  * @file    Integration.c
  * @brief   ����MQTT��ͼ��ʶ�𼯳ɣ�ʵ�ְ汾��
  ******************************************************************************
  */

#include "Integration.h"

/* ����ͨ�Ŷ������� */
extern UART_HandleTypeDef huart1;

/* ȫ�ֱ��� */
static uint8_t integration_initialized = 0;
static volatile uint8_t result_received = 0;
static char result_buffer[512] = {0};

// �ش�������صı���
volatile uint8_t retransmission_requested = 0;
char retransmission_chunks[512] = {0};

// �������MQTT������������Ϊȫ�ֱ���������ģ��ʹ�� - ���ɼ��������ļ�����
const char* MQTT_TOPIC_IMAGE_INFO = "stm32/image/info";
const char* MQTT_TOPIC_IMAGE_DATA = "stm32/image/data";
const char* MQTT_TOPIC_IMAGE_END = "stm32/image/end";
const char* MQTT_TOPIC_STATUS = "stm32/status";
const char* MQTT_TOPIC_IMAGE_STATUS = "stm32/image/status";
const char* MQTT_TOPIC_RETRANS_REQUEST = "stm32/image/retrans/req";
const char* MQTT_TOPIC_RETRANS_COMPLETE = "stm32/image/retrans/complete";

// ���ڵ��Դ�ӡ�ĸ������������޸�[AI]��ǩ
void Debug_Print(const char* msg) {
    // ��������������ڴ��ڲ鿴ʵʱ������
    HAL_UART_Transmit(&huart1, (uint8_t*)"[MQTT] ", 7, 200);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 300);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
    HAL_Delay(20); // ����ȷ��ͨ���ȶ��ԣ������ӡ����
}

// ��׼����UART���Դ�ӡ����������ǰ׺�����Դ
void UART_Debug(const char* msg) {
    // ���������ʽ
    char buffer[256];
    sprintf(buffer, "DEBUG: %s\r\n", msg);
    HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), 300);
    HAL_Delay(20); // ����ȷ��ͨ���ȶ���
}

// ESP8266Ӳ�����ú���
void ESP8266_HardReset(void) {
    Debug_Print("Performing ESP8266 hardware reset...");
    UART_Debug("Performing ESP8266 hardware reset");
    
    ESP8266_Restart();
    HAL_Delay(3000);
    
    Debug_Print("ESP8266 reset complete");
    UART_Debug("ESP8266 reset complete");
}

/**
  * @brief  Base64����
  * @param  data: Ҫ��������
  * @param  data_len: Ҫ�������ݳ���
  * @param  output: ���������
  * @param  output_max: ��������������С
  * @retval ���������ݳ��ȣ���0��ʾʧ��
  */
uint32_t Base64_Encode(uint8_t* data, uint32_t data_len, char* output, uint32_t output_max)
{
    uint32_t i, j;
    uint32_t output_len = 0;
    uint32_t required_len = 4 * ((data_len + 2) / 3) + 1; // ������Ҫ�Ļ�������С��������β��
    char debug_buf[64];
    
    sprintf(debug_buf, "Base64 encoding %u bytes", (unsigned int)data_len);
    Debug_Print(debug_buf);
    
    // �������������Ƿ��㹻��
    if(output_max < required_len) {
        Debug_Print("Output buffer too small");
        return 0;
    }
    
    // ÿ3�ֽ����һ�飬4��Base64�ַ�
    for (i = 0; i < data_len; i += 3) {
        uint32_t triple = (data[i] << 16);
        
        if (i + 1 < data_len)
            triple |= (data[i + 1] << 8);
        if (i + 2 < data_len)
            triple |= data[i + 2];
        
        for (j = 0; j < 4; j++) {
            if (i * 8 + j * 6 < data_len * 8) {
                output[output_len++] = base64_table[(triple >> (18 - j * 6)) & 0x3F];
            } else {
                output[output_len++] = '='; // ����ַ�
            }
        }
        
        // ��ʾ������� - ÿ3000�ֽ����һ��
        if (i % 3000 == 0 && i > 0) {
            sprintf(debug_buf, "Encoded %u/%u bytes", (unsigned int)i, (unsigned int)data_len);
            Debug_Print(debug_buf);
        }
    }
    
    output[output_len] = '\0';
    sprintf(debug_buf, "Base64 encoding complete, size: %u bytes", (unsigned int)output_len);
    Debug_Print(debug_buf);
    return output_len;
}

/**
  * @brief  ��ʼ��MQTT����
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t Integration_Init(void)
{
    Debug_Print("Initializing MQTT integration...");
    
    // ���WiFi����״̬
    if (!esp8266_connected) {
        Debug_Print("ESP8266 not connected to WiFi. Cannot initialize MQTT integration.");
        return 0;
    }
    
    // ��ʼ��MQTT�ͻ���
    if (!MQTT_Init()) {
        Debug_Print("Failed to initialize MQTT client");
        return 0;
    }
    
    // ���ӵ�MQTT������
    if (!MQTT_Connect()) {
        Debug_Print("Failed to connect to MQTT broker");
        return 0;
    }
    
    // ���Ľ������
    if (!MQTT_Subscribe(MQTT_TOPIC_RESULT)) {
        Debug_Print("Failed to subscribe to result topic");
        MQTT_Disconnect();
        return 0;
    }
    
    // ������������
    if (!MQTT_Subscribe(MQTT_TOPIC_COMMAND)) {
        Debug_Print("Failed to subscribe to command topic");
        MQTT_Disconnect();
        return 0;
    }
    
    // ���ͳ�������״̬��Ϣ
    char status_msg[64];
    sprintf(status_msg, "{\"status\":\"online\",\"device\":\"stm32\",\"time\":%u}", (unsigned int)HAL_GetTick());
    if (!MQTT_Publish(MQTT_TOPIC_STATUS, status_msg)) {
        Debug_Print("Failed to publish status message");
        MQTT_Disconnect();
        return 0;
    }
    
    integration_initialized = 1;
    Debug_Print("MQTT integration initialized successfully");
    
    return 1;
}

/**
  * @brief  ����ͼƬ���ƶ˷��� - �ֿ�汾������ʹ�ø��ɿ��Ĵ������
  * @param  filename: ͼƬ�ļ���
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t Integration_SendImage(const char* filename)
{
    FIL file;
    FRESULT res;
    UINT bytes_read;
    uint32_t file_size;
    char debug_buf[64];
    
    // ����ʼ��״̬
    if (!integration_initialized) {
        Debug_Print("MQTT integration not initialized");
        return 0;
    }
    
    // ��ǰ�ȼ��MQTT����״̬
    Debug_Print("Checking MQTT connection status...");
    if (!MQTT_Check()) {
        Debug_Print("MQTT connection lost, trying to reconnect");
        
        // ������������
        if (!MQTT_Connect()) {
            Debug_Print("Failed to reconnect to MQTT broker");
            return 0;
        }
        Debug_Print("MQTT reconnected successfully");
        
        // ���¶�����Ҫ������
        MQTT_Subscribe(MQTT_TOPIC_RESULT);
        MQTT_Subscribe(MQTT_TOPIC_COMMAND);
    }
    
    sprintf(debug_buf, "Processing image: %s", filename);
    Debug_Print(debug_buf);
    
    // ��ͼƬ�ļ�
    Debug_Print("Opening image file");
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        sprintf(debug_buf, "File open error: %d", res);
        Debug_Print(debug_buf);
        return 0;
    }
    
    // ��ȡ�ļ���С
    file_size = f_size(&file);
    sprintf(debug_buf, "File size: %u bytes", (unsigned int)file_size);
    Debug_Print(debug_buf);
    
    if (file_size == 0 || file_size > 1024*512) { // ���֧�ִ�СΪ512KB
        Debug_Print("File size invalid or too large");
        f_close(&file);
        return 0;
    }
    
    // ����: ���Ͷ�����Ϣ�Լ�������
    Debug_Print("Warming up MQTT connection...");
    for (int i = 0; i < 3; i++) {
        char warmup_msg[32];
        sprintf(warmup_msg, "{\"warmup\":%d}", i);
        MQTT_Publish("stm32/warmup", warmup_msg);
        HAL_Delay(200);  // 200ms between warmup messages
    }
    
    // �����ļ���Ϣ - �����ļ����ʹ�С
    char info_message[128];
    sprintf(info_message, "{\"filename\":\"%s\",\"size\":%u,\"device\":\"stm32\"}", 
            filename, (unsigned int)file_size);
    
    Debug_Print("Sending file info");
    if (!MQTT_Publish(MQTT_TOPIC_IMAGE_INFO, info_message)) {
        Debug_Print("Failed to send file info");
        f_close(&file);
        return 0;
    }
    
    // ʹ�÷ֿ�Ĵ�С
    const uint16_t chunk_size = 512;  // �ֿ�Ϊ512�ֽ�
    
    // ʹ�ö��������ݰ�������������
    static uint8_t chunk_buffer[512];
    
    uint32_t sent_bytes = 0;
    uint16_t chunk_index = 0;
    uint8_t success = 1;  // �ɹ���־
    
    // �������ڼ�¼�ѷ��͵ĸ��ֿ�״̬
    #define MAX_CHUNKS 200  // �ܹ��������ֿ�
    uint8_t chunk_sent[MAX_CHUNKS] = {0};  // 0=δ����, 1=�ѷ���
    uint8_t chunk_acked[MAX_CHUNKS] = {0}; // 0=δȷ��, 1=��ȷ��
    uint8_t chunk_count = 0;
    
    Debug_Print("Starting image data transmission in chunks");
    
    // �ȴ�һ��ʱ�������׼����
    HAL_Delay(200);
    
    // ���ͷֿ����ݷֿ�
    while (sent_bytes < file_size && chunk_index < MAX_CHUNKS) {
        // ���㵱ǰ��Ҫ���ֽ���
        uint16_t bytes_to_read = (file_size - sent_bytes > chunk_size) ? 
                               chunk_size : (file_size - sent_bytes);
        
        // ���ļ���ȡ��ǰ��
        memset(chunk_buffer, 0, chunk_size); // ���������
        res = f_read(&file, chunk_buffer, bytes_to_read, &bytes_read);
        
        sprintf(debug_buf, "Reading chunk %u, size: %u", chunk_index, bytes_read);
        Debug_Print(debug_buf);
        
        if (res != FR_OK || bytes_read != bytes_to_read) {
            sprintf(debug_buf, "File read error: %d, read %u bytes", res, (unsigned int)bytes_read);
            Debug_Print(debug_buf);
            success = 0;
            break;
        }
        
        // ������������ - ���������
        char chunk_topic[32];
        sprintf(chunk_topic, "%s/%u", MQTT_TOPIC_IMAGE_DATA, chunk_index);
        
        // ����ǰ20���� (��Ҫ���ݿ�)��ȷ����Щ����봫��ɹ�����������ͼ������޷�����
        if (chunk_index < 20) {
            // �ؼ���������Ի��ƵĴ���ʽ
            uint8_t retry_count = 0;
            uint8_t sent_success = 0;
            
            sprintf(debug_buf, "Sending critical chunk %u (%u bytes) with retries", chunk_index, bytes_read);
            Debug_Print(debug_buf);
            
            while (!sent_success && retry_count < 3) {
                sent_success = MQTT_PublishBinary(chunk_topic, chunk_buffer, bytes_read);
                if (!sent_success) {
                    sprintf(debug_buf, "Retry %u for critical chunk %u", retry_count + 1, chunk_index);
                    Debug_Print(debug_buf);
                    HAL_Delay(200);  // ����ǰ�Ե�һ��
                    retry_count++;
                }
            }
            
            if (!sent_success) {
                sprintf(debug_buf, "Failed to send critical chunk %u after retries", chunk_index);
                Debug_Print(debug_buf);
                success = 0;
                break;
            }
            
            // ��Ҫ�鷢�ͳɹ���������
            if (chunk_index == 5) {
                char status_msg[64];
                sprintf(status_msg, "{\"status\":\"critical_chunks_sent\"}");
                MQTT_Publish(MQTT_TOPIC_STATUS, status_msg);
            }
            
            // ����Ҫ��ʹ�ø������ӳ�ʱ��
            HAL_Delay(150);  // 3������ͨ���ӳ�
        } else {
            // ������ͨ���ݿ�
            sprintf(debug_buf, "Sending chunk %u (%u bytes)", chunk_index, bytes_read);
            Debug_Print(debug_buf);
            
            if (!MQTT_PublishBinary(chunk_topic, chunk_buffer, bytes_read)) {
                sprintf(debug_buf, "Failed to send chunk %u", chunk_index);
                Debug_Print(debug_buf);
                // ���������˳������������ݿ鷢��ʧ�ܣ�����ͨ���ش����ƻָ�
                chunk_sent[chunk_index] = 0; // ���Ϊδ���ͳɹ�
            } else {
                chunk_sent[chunk_index] = 1; // ���Ϊ���ͳɹ�
            }
            
            // ���ڿ��Ʒ����ٶȱ���������Ϣ�ѻ�
            HAL_Delay(50);
        }
        
        // ���½���
        sent_bytes += bytes_read;
        chunk_index++;
        chunk_count = chunk_index; // ��¼�ܿ���
        
        // ÿ��30���鷢��һ�ν���֪ͨ
        if (chunk_index % 30 == 0) {
            uint8_t progress = (sent_bytes * 100) / file_size;
            char status_msg[64];
            sprintf(status_msg, "{\"status\":\"transmitting\",\"progress\":%u}", progress);
            MQTT_Publish(MQTT_TOPIC_STATUS, status_msg);
        }
        
        // ÿ����ķ��ͽ���
        sprintf(debug_buf, "Progress: %u/%u bytes (%u%%)", 
                (unsigned int)sent_bytes, 
                (unsigned int)file_size, 
                (unsigned int)(sent_bytes * 100 / file_size));
        Debug_Print(debug_buf);
    }
    
    // �ر��ļ�
    f_close(&file);
    Debug_Print("File closed");
    
    // ���Ϳ�״̬��Ϣ������ˣ����÷���˿�ʼ����Ƿ�ʧ��
    char chunk_status[256];
    sprintf(chunk_status, "{\"total_chunks\":%u,\"last_chunk_index\":%u}", 
            chunk_count, chunk_count-1);
    Debug_Print("Sending chunk status information");
    MQTT_Publish(MQTT_TOPIC_IMAGE_STATUS, chunk_status);
    HAL_Delay(500); // �ȷ���˴�����Щ��Ϣ
    
    // �ȴ����ն������ش�ȱʧ�Ŀ�
    Debug_Print("Waiting for retransmission requests...");
    uint32_t retransmission_start = HAL_GetTick();
    uint8_t retransmission_done = 0;
    
    // �ȴ����10�����ش�����
    while ((HAL_GetTick() - retransmission_start) < 10000 && !retransmission_done) {
        // ������յ�����Ϣ
        MQTT_ProcessIncomingData();
        
        // ����Ƿ��յ��ش�����
        if (retransmission_requested) {
            retransmission_requested = 0; // ���ñ�־
            
            // �����ش�����׼���ط�������Ŀ����
            Debug_Print("Received retransmission request");
            Debug_Print(retransmission_chunks);
            
            // ���ļ��ش�
            res = f_open(&file, filename, FA_READ);
            if (res != FR_OK) {
                Debug_Print("Failed to open file for retransmission");
                break;
            }
            
            // �����ش�����
            char* chunk_str = strtok(retransmission_chunks, ",");
            while (chunk_str != NULL) {
                // ��ȡ�����
                uint16_t chunk_idx = atoi(chunk_str);
                
                // �������Ƿ���Ч
                if (chunk_idx < chunk_count) {
                    // ��ȡ�ļ��е�λ��
                    uint32_t offset = chunk_idx * chunk_size;
                    
                    // �����ļ�����Ӧλ��
                    res = f_lseek(&file, offset);
                    if (res != FR_OK) {
                        sprintf(debug_buf, "Failed to seek to position for chunk %u", chunk_idx);
                        Debug_Print(debug_buf);
                        chunk_str = strtok(NULL, ",");
                        continue;
                    }
                    
                    // ��ȡ����
                    uint16_t bytes_to_read = (file_size - offset > chunk_size) ? 
                                        chunk_size : (file_size - offset);
                    memset(chunk_buffer, 0, chunk_size);
                    res = f_read(&file, chunk_buffer, bytes_to_read, &bytes_read);
                    
                    if (res != FR_OK || bytes_read != bytes_to_read) {
                        sprintf(debug_buf, "Failed to read chunk %u for retransmission", chunk_idx);
                        Debug_Print(debug_buf);
                        chunk_str = strtok(NULL, ",");
                        continue;
                    }
                    
                    // ���·��Ϳ�
                    char chunk_topic[32];
                    sprintf(chunk_topic, "%s/%u", MQTT_TOPIC_IMAGE_DATA, chunk_idx);
                    sprintf(debug_buf, "Retransmitting chunk %u", chunk_idx);
                    Debug_Print(debug_buf);
                    
                    // ��γ����ش�
                    uint8_t retries = 3;
                    uint8_t resend_success = 0;
                    
                    while (retries > 0 && !resend_success) {
                        resend_success = MQTT_PublishBinary(chunk_topic, chunk_buffer, bytes_read);
                        if (!resend_success) {
                            retries--;
                            HAL_Delay(100);
                        }
                    }
                    
                    if (resend_success) {
                        sprintf(debug_buf, "Chunk %u retransmitted successfully", chunk_idx);
                    } else {
                        sprintf(debug_buf, "Failed to retransmit chunk %u", chunk_idx);
                    }
                    Debug_Print(debug_buf);
                }
                
                // ��ȡ��һ�������
                chunk_str = strtok(NULL, ",");
                
                // ÿ���ش���֮������ȷ��ͨ���ȶ���
                HAL_Delay(100);
            }
            
            // �ر��ļ�
            f_close(&file);
            
            // �����ش������Ϣ
            MQTT_Publish(MQTT_TOPIC_RETRANS_COMPLETE, "1");
            Debug_Print("Retransmission complete notification sent");
            
            // �ȷ���˴�����Щ�ش��Ŀ�
            HAL_Delay(1000);
        }
        
        // ͨ���ȶ���
        HAL_Delay(50);
    }
    
    // ������ɱ��
    if (success) {
        Debug_Print("Sending completion marker");
        
        // ����ȷ��ͨ���ȶ���
        HAL_Delay(200);
        
        // ��η�����ɱ����ȷ������
        uint8_t end_marker_sent = 0;
        for (uint8_t retry = 0; retry < 3 && !end_marker_sent; retry++) {
            end_marker_sent = MQTT_Publish(MQTT_TOPIC_IMAGE_END, "1");
            if (!end_marker_sent) {
                Debug_Print("Retrying to send completion marker");
                HAL_Delay(200);
            }
        }
        
        if (!end_marker_sent) {
            Debug_Print("Failed to send completion marker");
            success = 0;
        } else {
            Debug_Print("Completion marker sent");
            
            // ���״̬����
            char status_msg[64];
            sprintf(status_msg, "{\"status\":\"transmitting\",\"progress\":99}");
            MQTT_Publish(MQTT_TOPIC_STATUS, status_msg);
        }
    }
    
    if (success) {
        Debug_Print("Image transmitted successfully");
        sprintf(debug_buf, "Total chunks: %u, Total bytes: %u", 
                (unsigned int)chunk_count, (unsigned int)sent_bytes);
        Debug_Print(debug_buf);
    } else {
        Debug_Print("Image transmission failed");
    }
    
    return success;
}

/**
 * @brief ����ӷ�����յ����ش�����
 * @param topic: ����
 * @param message: ��Ϣ����
 */
void Integration_HandleRetransmissionRequest(const char* topic, const char* message)
{
    // ����Ƿ�Ϊ�ش���������
    if (strcmp(topic, MQTT_TOPIC_RETRANS_REQUEST) == 0) {
        Debug_Print("Received retransmission request");
        
        // ��������Ŀ���Ϣ
        strncpy(retransmission_chunks, message, sizeof(retransmission_chunks) - 1);
        retransmission_chunks[sizeof(retransmission_chunks) - 1] = '\0';
        
        // �����ش������־
        retransmission_requested = 1;
    }
}

/**
  * @brief  �ȴ�ʶ���� - ������
  * @param  result_buffer: ������ս���Ļ�����
  * @param  buffer_size: ��������С
  * @param  timeout: ��ʱ�ȴ�ʱ��(����)
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t Integration_WaitForResult(char* output_buffer, uint16_t buffer_size, uint32_t timeout)
{
    // ��������������Ϣ
    Debug_Print("Publishing request for result...");
    MQTT_Publish("stm32/request_result", "{\"request\":\"current_result\"}");
    
    // ʹ��MQTTģ��ĵȴ�������� - �����޸��Ĺؼ�
    Debug_Print("Waiting for recognition result...");
    if (MQTT_WaitForResult(output_buffer, buffer_size, timeout)) {
        Debug_Print("Result received:");
        Debug_Print(output_buffer);
        return 1;
    }
    
    Debug_Print("Timeout waiting for result");
    
    // �����������ӻָ�
    UART_Debug("Still no result after timeout, attempting reconnection");
    MQTT_Disconnect();
    HAL_Delay(500);
    if (MQTT_Connect()) {
        MQTT_Subscribe(MQTT_TOPIC_RESULT);
        MQTT_Subscribe(MQTT_TOPIC_COMMAND);
        
        UART_Debug("Reconnected, waiting a bit more");
        // ���µȴ����
        if (MQTT_WaitForResult(output_buffer, buffer_size, 5000)) {
            Debug_Print("Result received after reconnection:");
            Debug_Print(output_buffer);
            return 1;
        }
    }
    
    UART_Debug("Still no result after retry");
    return 0;
}

/**
  * @brief  �����豸״̬��Ϣ
  * @param  status_message: ״̬��Ϣ
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t Integration_PublishStatus(const char* status_message)
{
    if (!integration_initialized) {
        Debug_Print("MQTT integration not initialized");
        return 0;
    }
    
    char status_data[256];
    sprintf(status_data, "{\"status\":\"%s\",\"device\":\"stm32\",\"time\":%u}", 
            status_message, (unsigned int)HAL_GetTick());
    
    if (MQTT_Publish(MQTT_TOPIC_STATUS, status_data)) {
        Debug_Print("Status published");
        return 1;
    } else {
        Debug_Print("Failed to publish status");
        return 0;
    }
}

/**
  * @brief  ������յ�������
  */
void Integration_ProcessCommands(void)
{
    // ���MQTT������Ϣ
    MQTT_ProcessIncomingData();
    
}