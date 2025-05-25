/**
  ******************************************************************************
  * @file    Integration.c
  * @brief   基于MQTT的图像识别集成（实现版本）
  ******************************************************************************
  */

#include "Integration.h"

/* 串口通信对象声明 */
extern UART_HandleTypeDef huart1;

/* 全局变量 */
static uint8_t integration_initialized = 0;
static volatile uint8_t result_received = 0;
static char result_buffer[512] = {0};

// 重传请求相关的变量
volatile uint8_t retransmission_requested = 0;
char retransmission_chunks[512] = {0};

// 定义具体MQTT主题名称以作为全局变量供其他模块使用 - 可由集成配置文件更改
const char* MQTT_TOPIC_IMAGE_INFO = "stm32/image/info";
const char* MQTT_TOPIC_IMAGE_DATA = "stm32/image/data";
const char* MQTT_TOPIC_IMAGE_END = "stm32/image/end";
const char* MQTT_TOPIC_STATUS = "stm32/status";
const char* MQTT_TOPIC_IMAGE_STATUS = "stm32/image/status";
const char* MQTT_TOPIC_RETRANS_REQUEST = "stm32/image/retrans/req";
const char* MQTT_TOPIC_RETRANS_COMPLETE = "stm32/image/retrans/complete";

// 用于调试打印的辅助函数，不修改[AI]标签
void Debug_Print(const char* msg) {
    // 调试输出，方便在串口查看实时处理结果
    HAL_UART_Transmit(&huart1, (uint8_t*)"[MQTT] ", 7, 200);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 300);
    HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 100);
    HAL_Delay(20); // 用于确保通信稳定性，避免打印混乱
}

// 标准化的UART调试打印函数，增加前缀标记来源
void UART_Debug(const char* msg) {
    // 调试输出格式
    char buffer[256];
    sprintf(buffer, "DEBUG: %s\r\n", msg);
    HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), 300);
    HAL_Delay(20); // 用于确保通信稳定性
}

// ESP8266硬件重置函数
void ESP8266_HardReset(void) {
    Debug_Print("Performing ESP8266 hardware reset...");
    UART_Debug("Performing ESP8266 hardware reset");
    
    ESP8266_Restart();
    HAL_Delay(3000);
    
    Debug_Print("ESP8266 reset complete");
    UART_Debug("ESP8266 reset complete");
}

/**
  * @brief  Base64编码
  * @param  data: 要编码数据
  * @param  data_len: 要编码数据长度
  * @param  output: 输出缓冲区
  * @param  output_max: 最大输出缓冲区大小
  * @retval 编码后的数据长度，或0表示失败
  */
uint32_t Base64_Encode(uint8_t* data, uint32_t data_len, char* output, uint32_t output_max)
{
    uint32_t i, j;
    uint32_t output_len = 0;
    uint32_t required_len = 4 * ((data_len + 2) / 3) + 1; // 计算需要的缓冲区大小（包含结尾）
    char debug_buf[64];
    
    sprintf(debug_buf, "Base64 encoding %u bytes", (unsigned int)data_len);
    Debug_Print(debug_buf);
    
    // 检查输出缓冲区是否足够大
    if(output_max < required_len) {
        Debug_Print("Output buffer too small");
        return 0;
    }
    
    // 每3字节组成一组，4个Base64字符
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
                output[output_len++] = '='; // 填充字符
            }
        }
        
        // 显示编码进度 - 每3000字节输出一次
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
  * @brief  初始化MQTT集成
  * @retval 1: 成功, 0: 失败
  */
uint8_t Integration_Init(void)
{
    Debug_Print("Initializing MQTT integration...");
    
    // 检查WiFi连接状态
    if (!esp8266_connected) {
        Debug_Print("ESP8266 not connected to WiFi. Cannot initialize MQTT integration.");
        return 0;
    }
    
    // 初始化MQTT客户端
    if (!MQTT_Init()) {
        Debug_Print("Failed to initialize MQTT client");
        return 0;
    }
    
    // 连接到MQTT服务器
    if (!MQTT_Connect()) {
        Debug_Print("Failed to connect to MQTT broker");
        return 0;
    }
    
    // 订阅结果主题
    if (!MQTT_Subscribe(MQTT_TOPIC_RESULT)) {
        Debug_Print("Failed to subscribe to result topic");
        MQTT_Disconnect();
        return 0;
    }
    
    // 订阅命令主题
    if (!MQTT_Subscribe(MQTT_TOPIC_COMMAND)) {
        Debug_Print("Failed to subscribe to command topic");
        MQTT_Disconnect();
        return 0;
    }
    
    // 发送初次上线状态消息
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
  * @brief  发送图片到云端服务 - 分块版本，包含使用更可靠的传输机制
  * @param  filename: 图片文件名
  * @retval 1: 成功, 0: 失败
  */
uint8_t Integration_SendImage(const char* filename)
{
    FIL file;
    FRESULT res;
    UINT bytes_read;
    uint32_t file_size;
    char debug_buf[64];
    
    // 检查初始化状态
    if (!integration_initialized) {
        Debug_Print("MQTT integration not initialized");
        return 0;
    }
    
    // 发前先检查MQTT连接状态
    Debug_Print("Checking MQTT connection status...");
    if (!MQTT_Check()) {
        Debug_Print("MQTT connection lost, trying to reconnect");
        
        // 尝试重新连接
        if (!MQTT_Connect()) {
            Debug_Print("Failed to reconnect to MQTT broker");
            return 0;
        }
        Debug_Print("MQTT reconnected successfully");
        
        // 重新订阅需要的主题
        MQTT_Subscribe(MQTT_TOPIC_RESULT);
        MQTT_Subscribe(MQTT_TOPIC_COMMAND);
    }
    
    sprintf(debug_buf, "Processing image: %s", filename);
    Debug_Print(debug_buf);
    
    // 打开图片文件
    Debug_Print("Opening image file");
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        sprintf(debug_buf, "File open error: %d", res);
        Debug_Print(debug_buf);
        return 0;
    }
    
    // 获取文件大小
    file_size = f_size(&file);
    sprintf(debug_buf, "File size: %u bytes", (unsigned int)file_size);
    Debug_Print(debug_buf);
    
    if (file_size == 0 || file_size > 1024*512) { // 最大支持大小为512KB
        Debug_Print("File size invalid or too large");
        f_close(&file);
        return 0;
    }
    
    // 策略: 发送额外消息以激活连接
    Debug_Print("Warming up MQTT connection...");
    for (int i = 0; i < 3; i++) {
        char warmup_msg[32];
        sprintf(warmup_msg, "{\"warmup\":%d}", i);
        MQTT_Publish("stm32/warmup", warmup_msg);
        HAL_Delay(200);  // 200ms between warmup messages
    }
    
    // 发送文件信息 - 包含文件名和大小
    char info_message[128];
    sprintf(info_message, "{\"filename\":\"%s\",\"size\":%u,\"device\":\"stm32\"}", 
            filename, (unsigned int)file_size);
    
    Debug_Print("Sending file info");
    if (!MQTT_Publish(MQTT_TOPIC_IMAGE_INFO, info_message)) {
        Debug_Print("Failed to send file info");
        f_close(&file);
        return 0;
    }
    
    // 使用分块的大小
    const uint16_t chunk_size = 512;  // 分块为512字节
    
    // 使用二进制数据包处理生成数据
    static uint8_t chunk_buffer[512];
    
    uint32_t sent_bytes = 0;
    uint16_t chunk_index = 0;
    uint8_t success = 1;  // 成功标志
    
    // 数据用于记录已发送的各分块状态
    #define MAX_CHUNKS 200  // 能够处理最大分块
    uint8_t chunk_sent[MAX_CHUNKS] = {0};  // 0=未发送, 1=已发送
    uint8_t chunk_acked[MAX_CHUNKS] = {0}; // 0=未确认, 1=已确认
    uint8_t chunk_count = 0;
    
    Debug_Print("Starting image data transmission in chunks");
    
    // 等待一段时间给服务准备好
    HAL_Delay(200);
    
    // 发送分块数据分块
    while (sent_bytes < file_size && chunk_index < MAX_CHUNKS) {
        // 计算当前需要的字节数
        uint16_t bytes_to_read = (file_size - sent_bytes > chunk_size) ? 
                               chunk_size : (file_size - sent_bytes);
        
        // 从文件读取当前块
        memset(chunk_buffer, 0, chunk_size); // 清除缓冲区
        res = f_read(&file, chunk_buffer, bytes_to_read, &bytes_read);
        
        sprintf(debug_buf, "Reading chunk %u, size: %u", chunk_index, bytes_read);
        Debug_Print(debug_buf);
        
        if (res != FR_OK || bytes_read != bytes_to_read) {
            sprintf(debug_buf, "File read error: %d, read %u bytes", res, (unsigned int)bytes_read);
            Debug_Print(debug_buf);
            success = 0;
            break;
        }
        
        // 创建发布主题 - 包含块序号
        char chunk_topic[32];
        sprintf(chunk_topic, "%s/%u", MQTT_TOPIC_IMAGE_DATA, chunk_index);
        
        // 处理前20个块 (重要数据块)，确保这些块必须传输成功，否则整体图像可能无法解析
        if (chunk_index < 20) {
            // 关键块采用重试机制的处理方式
            uint8_t retry_count = 0;
            uint8_t sent_success = 0;
            
            sprintf(debug_buf, "Sending critical chunk %u (%u bytes) with retries", chunk_index, bytes_read);
            Debug_Print(debug_buf);
            
            while (!sent_success && retry_count < 3) {
                sent_success = MQTT_PublishBinary(chunk_topic, chunk_buffer, bytes_read);
                if (!sent_success) {
                    sprintf(debug_buf, "Retry %u for critical chunk %u", retry_count + 1, chunk_index);
                    Debug_Print(debug_buf);
                    HAL_Delay(200);  // 重试前稍等一会
                    retry_count++;
                }
            }
            
            if (!sent_success) {
                sprintf(debug_buf, "Failed to send critical chunk %u after retries", chunk_index);
                Debug_Print(debug_buf);
                success = 0;
                break;
            }
            
            // 重要块发送成功的特殊标记
            if (chunk_index == 5) {
                char status_msg[64];
                sprintf(status_msg, "{\"status\":\"critical_chunks_sent\"}");
                MQTT_Publish(MQTT_TOPIC_STATUS, status_msg);
            }
            
            // 对重要块使用更长的延迟时间
            HAL_Delay(150);  // 3倍的普通块延迟
        } else {
            // 发送普通数据块
            sprintf(debug_buf, "Sending chunk %u (%u bytes)", chunk_index, bytes_read);
            Debug_Print(debug_buf);
            
            if (!MQTT_PublishBinary(chunk_topic, chunk_buffer, bytes_read)) {
                sprintf(debug_buf, "Failed to send chunk %u", chunk_index);
                Debug_Print(debug_buf);
                // 但不立即退出，允许部分数据块发送失败，后续通过重传机制恢复
                chunk_sent[chunk_index] = 0; // 标记为未发送成功
            } else {
                chunk_sent[chunk_index] = 1; // 标记为发送成功
            }
            
            // 用于控制发送速度避免服务端消息堆积
            HAL_Delay(50);
        }
        
        // 更新进度
        sent_bytes += bytes_read;
        chunk_index++;
        chunk_count = chunk_index; // 记录总块数
        
        // 每隔30个块发送一次进度通知
        if (chunk_index % 30 == 0) {
            uint8_t progress = (sent_bytes * 100) / file_size;
            char status_msg[64];
            sprintf(status_msg, "{\"status\":\"transmitting\",\"progress\":%u}", progress);
            MQTT_Publish(MQTT_TOPIC_STATUS, status_msg);
        }
        
        // 每个块的发送进度
        sprintf(debug_buf, "Progress: %u/%u bytes (%u%%)", 
                (unsigned int)sent_bytes, 
                (unsigned int)file_size, 
                (unsigned int)(sent_bytes * 100 / file_size));
        Debug_Print(debug_buf);
    }
    
    // 关闭文件
    f_close(&file);
    Debug_Print("File closed");
    
    // 发送块状态信息给服务端，以让服务端开始检查是否丢失块
    char chunk_status[256];
    sprintf(chunk_status, "{\"total_chunks\":%u,\"last_chunk_index\":%u}", 
            chunk_count, chunk_count-1);
    Debug_Print("Sending chunk status information");
    MQTT_Publish(MQTT_TOPIC_IMAGE_STATUS, chunk_status);
    HAL_Delay(500); // 等服务端处理这些信息
    
    // 等待接收端请求重传缺失的块
    Debug_Print("Waiting for retransmission requests...");
    uint32_t retransmission_start = HAL_GetTick();
    uint8_t retransmission_done = 0;
    
    // 等待最多10秒检查重传请求
    while ((HAL_GetTick() - retransmission_start) < 10000 && !retransmission_done) {
        // 处理接收到的消息
        MQTT_ProcessIncomingData();
        
        // 检查是否收到重传请求
        if (retransmission_requested) {
            retransmission_requested = 0; // 重置标志
            
            // 解析重传请求，准备重发所请求的块序号
            Debug_Print("Received retransmission request");
            Debug_Print(retransmission_chunks);
            
            // 打开文件重传
            res = f_open(&file, filename, FA_READ);
            if (res != FR_OK) {
                Debug_Print("Failed to open file for retransmission");
                break;
            }
            
            // 处理重传请求
            char* chunk_str = strtok(retransmission_chunks, ",");
            while (chunk_str != NULL) {
                // 获取块序号
                uint16_t chunk_idx = atoi(chunk_str);
                
                // 检查序号是否有效
                if (chunk_idx < chunk_count) {
                    // 获取文件中的位置
                    uint32_t offset = chunk_idx * chunk_size;
                    
                    // 跳到文件中相应位置
                    res = f_lseek(&file, offset);
                    if (res != FR_OK) {
                        sprintf(debug_buf, "Failed to seek to position for chunk %u", chunk_idx);
                        Debug_Print(debug_buf);
                        chunk_str = strtok(NULL, ",");
                        continue;
                    }
                    
                    // 读取数据
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
                    
                    // 重新发送块
                    char chunk_topic[32];
                    sprintf(chunk_topic, "%s/%u", MQTT_TOPIC_IMAGE_DATA, chunk_idx);
                    sprintf(debug_buf, "Retransmitting chunk %u", chunk_idx);
                    Debug_Print(debug_buf);
                    
                    // 多次尝试重传
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
                
                // 获取下一个块序号
                chunk_str = strtok(NULL, ",");
                
                // 每个重传块之间用于确保通信稳定性
                HAL_Delay(100);
            }
            
            // 关闭文件
            f_close(&file);
            
            // 发送重传完成消息
            MQTT_Publish(MQTT_TOPIC_RETRANS_COMPLETE, "1");
            Debug_Print("Retransmission complete notification sent");
            
            // 等服务端处理这些重传的块
            HAL_Delay(1000);
        }
        
        // 通信稳定性
        HAL_Delay(50);
    }
    
    // 发送完成标记
    if (success) {
        Debug_Print("Sending completion marker");
        
        // 用于确保通信稳定性
        HAL_Delay(200);
        
        // 多次发送完成标记以确保接收
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
            
            // 最后状态更新
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
 * @brief 处理从服务端收到的重传请求
 * @param topic: 主题
 * @param message: 消息内容
 */
void Integration_HandleRetransmissionRequest(const char* topic, const char* message)
{
    // 检查是否为重传请求主题
    if (strcmp(topic, MQTT_TOPIC_RETRANS_REQUEST) == 0) {
        Debug_Print("Received retransmission request");
        
        // 保存请求的块信息
        strncpy(retransmission_chunks, message, sizeof(retransmission_chunks) - 1);
        retransmission_chunks[sizeof(retransmission_chunks) - 1] = '\0';
        
        // 设置重传请求标志
        retransmission_requested = 1;
    }
}

/**
  * @brief  等待识别结果 - 基本版
  * @param  result_buffer: 保存接收结果的缓冲区
  * @param  buffer_size: 缓冲区大小
  * @param  timeout: 超时等待时间(毫秒)
  * @retval 1: 成功, 0: 失败
  */
uint8_t Integration_WaitForResult(char* output_buffer, uint16_t buffer_size, uint32_t timeout)
{
    // 发布请求结果的消息
    Debug_Print("Publishing request for result...");
    MQTT_Publish("stm32/request_result", "{\"request\":\"current_result\"}");
    
    // 使用MQTT模块的等待结果函数 - 这是修复的关键
    Debug_Print("Waiting for recognition result...");
    if (MQTT_WaitForResult(output_buffer, buffer_size, timeout)) {
        Debug_Print("Result received:");
        Debug_Print(output_buffer);
        return 1;
    }
    
    Debug_Print("Timeout waiting for result");
    
    // 尝试最后的连接恢复
    UART_Debug("Still no result after timeout, attempting reconnection");
    MQTT_Disconnect();
    HAL_Delay(500);
    if (MQTT_Connect()) {
        MQTT_Subscribe(MQTT_TOPIC_RESULT);
        MQTT_Subscribe(MQTT_TOPIC_COMMAND);
        
        UART_Debug("Reconnected, waiting a bit more");
        // 重新等待结果
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
  * @brief  发送设备状态信息
  * @param  status_message: 状态消息
  * @retval 1: 成功, 0: 失败
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
  * @brief  处理接收到的命令
  */
void Integration_ProcessCommands(void)
{
    // 检查MQTT订阅消息
    MQTT_ProcessIncomingData();
    
}