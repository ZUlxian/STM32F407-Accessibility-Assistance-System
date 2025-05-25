/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 基于STM32的盲人无障碍辅助系统
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "st7789.h"
#include "ov7670.h"
#include "sccb.h"
#include "sys_delay.h"
#include "dcmi.h"
#include "ov7670test.h"
#include "sdio_sd.h"
#include "image_save.h"
#include "buzzer.h"
#include "fatfs.h"
#include "ff.h"
#include "esp8266.h"
#include "hc_sr04.h"
#include "haptic.h"
#include "Integration.h" 
#include "mqtt.h"        
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Private defines -----------------------------------------------------------*/
#define IMAGE_FILENAME "0:/IMAGE.JPG" 

// 显示高度
#define DISPLAY_HEIGHT 240

// 调试信息显示的起始行
#define DEBUG_START_Y 160

// 按钮操作相关
#define BUTTON_LONGPRESS_TIME 2000  // 长按时间阈值(毫秒)
#define BUTTON_DEBOUNCE_TIME  50    // 按钮消抖时间(毫秒)

// 距离阈值
#define DISTANCE_CLOSE    15.0f  // 非常近的距离阈值(厘米)
#define DISTANCE_MEDIUM   50.0f  // 中等距离阈值(厘米)
#define DISTANCE_MAX     100.0f  // 最大有效距离阈值(厘米)

/* Private typedefs -----------------------------------------------------------*/
typedef enum {
  SYSTEM_STATE_PARAM_DISPLAY,  // 参数显示模式
  SYSTEM_STATE_CAMERA_MODE     // 相机模式
} SystemState;

/* Global variables ---------------------------------------------------------*/
// 解码变量
extern uint32_t datanum;
extern uint8_t ov_rev_ok;
extern DMA_HandleTypeDef hdma_dcmi;
extern uint16_t camera_buffer[PIC_WIDTH*PIC_HEIGHT];

// UART变量
UART_HandleTypeDef huart1;

// WiFi变量
volatile uint8_t wifiConnected = 0;
volatile char ipAddress[20] = "Updating...";
volatile int8_t rssiValue = 0;
volatile uint32_t lastWifiRssiUpdateTick = 0;
volatile uint32_t lastWifiIpUpdateTick = 0;

// MQTT变量
volatile uint8_t mqtt_connected = 0;
volatile uint8_t command_received = 0;
char command_buffer[128] = {0};

// 其他全局变量
uint32_t frame_counter = 0;
uint8_t init_retry_count = 0;
uint8_t sd_init_status = 0;
uint8_t fatfs_init_status = 0;
uint8_t capture_requested = 0;    // 拍照请求标志
uint8_t photo_saved_success = 0;  // 拍照成功标志
uint32_t success_display_time = 0; // 拍照成功提示显示时间
uint32_t photo_save_start_time = 0; // 拍照开始时间
uint32_t save_progress_time = 0;   // 保存进度更新时间
uint32_t current_file_index = 1;   // 当前文件索引
uint8_t save_mode = SAVE_MODE_FATFS;
uint32_t sd_status_check_time = 0; // SD卡状态检查时间
uint8_t mqtt_image_transfer_requested = 0; // MQTT图像传输请求标志
uint8_t mqtt_image_transfer_in_progress = 0; // MQTT图像传输进行中标志
DMA_HandleTypeDef hdma_spi2_tx;
DMA_HandleTypeDef hdma_spi3_tx;
I2S_HandleTypeDef hi2s3;

// 系统状态变量
volatile SystemState currentSystemState = SYSTEM_STATE_PARAM_DISPLAY;
volatile uint8_t systemJustStarted = 1;              // 刚启动标志
volatile uint32_t autoModeChangeProtectionTimer = 0; // 防止自动模式切换

// 模式切换按钮管理变量(PA15)
volatile uint8_t buttonPressed = 0;
volatile uint32_t buttonPressTick = 0;
volatile uint8_t buttonLongPressDetected = 0;
volatile uint8_t buttonStateChanged = 0;
volatile uint32_t lastButtonChangeTime = 0; // 用于按钮消抖

// 拍照按钮管理变量(PA0)
volatile uint8_t cameraButtonPressed = 0;
volatile uint32_t cameraButtonPressTick = 0;

// 超声波传感器变量
volatile float frontDistance = 0.0f;
volatile float sideDistance = 0.0f;
volatile uint32_t lastDistanceUpdateTick = 0;

// 调试缓冲区
char debug_buffer[128];

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void Setup_SDIO_Interrupts(void);

// WiFi相关函数
void InitializeWiFi(void);
void UpdateWiFiInfoDisplay(void);
void RestoreWiFiCommunication(void);

// MQTT相关函数
void InitializeMQTT(void);
void CheckMQTTStatus(void);
void ProcessMQTTCommands(void);
uint8_t SendImageViaMQTT(const char* filename);

// 显示和界面函数
void ShowBootAnimation(void);
void DisplayParameterScreen(void);
void UpdateDistanceDisplay(void);
void DisplayDebugInfo(const char* info);
void ProcessButton(void);
void Display_Message(uint16_t y, const char* msg, uint16_t color);
void Fill_Rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void DisplayCameraImage(uint16_t *camera_buf, uint16_t width, uint16_t height);
void UpdateProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress, uint16_t color);

// 模式相关函数
void ParameterDisplayModeUpdate(void);
void CameraModeUpdate(void);

// 按钮相关函数
void Button_Init(void);
uint8_t Is_Button_Pressed(void);
uint8_t Is_Camera_Button_Pressed(void);

// 其他辅助函数
void Check_DMA_Status(void);
void Display_DCMI_Status(void);
void Display_Pixel_Preview(void);
void Display_Save_Status(void);
void EnsureSDCardDirectory(void);

/* Function implementations --------------------------------------------------*/


void MX_I2S3_Init(void)
{
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_44K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
}
/**
  * @brief 从相机模式切换回参数显示模式时调用此函数恢复WiFi功能
  */
void RestoreWiFiCommunication(void) {
    uint8_t retry;
    
    // 显示状态
    ST7789_FillRectangle(10, 270, 220, 10, BLACK);
    ST7789_WriteString(10, 270, "Checking ESP8266...", YELLOW, BLACK, 1);
    
    // 重启ESP8266
    ESP8266_Restart();
    HAL_Delay(500);
    
    // 初始化ESP8266
    if (!ESP8266_Init(&huart1)) {
        ST7789_FillRectangle(10, 270, 220, 10, BLACK);
        ST7789_WriteString(10, 270, "ESP8266 not responding!", RED, BLACK, 1);
        
        // 尝试重启ESP8266的通信
        // 硬件可能需要重新上电
        wifiConnected = 0;
        return;
    }
    
    // 重置WiFi连接状态
    wifiConnected = 0;
    
    // 显示连接状态
    ST7789_FillRectangle(10, 270, 220, 10, BLACK);
    ST7789_WriteString(10, 270, "Checking WiFi connection...", YELLOW, BLACK, 1);
    
    // 尝试检查连接状态
    for(retry = 0; retry < 3; retry++) {
        wifiConnected = ESP8266_CheckConnection();
        if(wifiConnected) {
            ST7789_FillRectangle(10, 270, 220, 10, BLACK);
            ST7789_WriteString(10, 270, "WiFi already connected", GREEN, BLACK, 1);
            break;
        }
        HAL_Delay(300);
    }
    
    // 如果检查连接失败，尝试重新连接
    if(!wifiConnected) {
        // 显示重连状态
        ST7789_FillRectangle(10, 270, 220, 10, BLACK);
        ST7789_WriteString(10, 270, "Reconnecting WiFi...", YELLOW, BLACK, 1);
        
        for(retry = 0; retry < 3; retry++) {
            wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
            if(wifiConnected) {
                ST7789_FillRectangle(10, 270, 220, 10, BLACK);
                ST7789_WriteString(10, 270, "WiFi reconnected", GREEN, BLACK, 1);
                break;
            }
            HAL_Delay(1000);
        }
    }
    
    // 如果成功连接，获取WiFi信息
    if(wifiConnected) {
        // 重置计时器以立即更新WiFi信息
        lastWifiRssiUpdateTick = 0;
        lastWifiIpUpdateTick = 0;
        
        // 获取信号强度和IP地址
        rssiValue = 0;
        for(retry = 0; retry < 3; retry++) {
            rssiValue = ESP8266_GetRSSI();
            if(rssiValue != 0) break;
            HAL_Delay(300);
        }
        
        strcpy((char*)ipAddress, "Updating...");
        for(retry = 0; retry < 3; retry++) {
            if(ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress))) {
                break;
            }
            HAL_Delay(300);
        }
        
        // 更新WiFi信息显示
        UpdateWiFiInfoDisplay();
        
        // 初始化MQTT连接
        ST7789_FillRectangle(10, 270, 220, 10, BLACK);
        ST7789_WriteString(10, 270, "Initializing MQTT...", YELLOW, BLACK, 1);
        InitializeMQTT();
        
        // 测试触觉反馈是否正常
        Beep(2);
        HAL_Delay(200);
        Vibrate(VIBRATOR_1, 200);
    } else {
        // 连接失败处理
        strcpy((char*)ipAddress, "WiFi Failed");
        rssiValue = 0;
        mqtt_connected = 0;
        
        ST7789_FillRectangle(10, 270, 220, 10, BLACK);
        ST7789_WriteString(10, 270, "WiFi reconnect failed", RED, BLACK, 1);
        
        // 显示检查硬件提示
        Beep(1);
        HAL_Delay(200);
        Beep(1);
    }
}

/**
  * @brief  更新屏幕上的WiFi信息
  */
void UpdateWiFiInfoDisplay(void) {
  if (!wifiConnected || currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;
  
  // 更新信号强度
  ST7789_FillRectangle(10, 175, 220, 20, BLACK);
  char msgBuffer[128];
  sprintf(msgBuffer, "Signal: %d dBm", rssiValue);
  ST7789_WriteString(10, 175, msgBuffer, YELLOW, BLACK, 1);
  
  // 更新IP地址
  ST7789_FillRectangle(10, 195, 220, 20, BLACK);
  ST7789_WriteString(10, 195, "IP: ", CYAN, BLACK, 1);
  ST7789_WriteString(40, 195, (const char*)ipAddress, GREEN, BLACK, 1);
  
  // 添加MQTT状态显示
  ST7789_FillRectangle(10, 215, 220, 20, BLACK);
  sprintf(msgBuffer, "MQTT: %s", mqtt_connected ? "Connected" : "Disconnected");
  ST7789_WriteString(10, 215, msgBuffer, mqtt_connected ? GREEN : RED, BLACK, 1);
}

/**
  * @brief  初始化WiFi
  */
void InitializeWiFi(void) {
  ST7789_WriteString(10, 130, "Init WiFi...", CYAN, BLACK, 1);
  
  // 初始化ESP8266
  uint8_t init_status = ESP8266_Init(&huart1);
  
  if (init_status) {
    ST7789_WriteString(200, 130, "OK", GREEN, BLACK, 1);
    
    // 尝试连接WiFi
    ST7789_WriteString(10, 150, "Connecting to WiFi...", CYAN, BLACK, 1);
    
    // 尝试连接WiFi (增加等待和重试)
    HAL_Delay(1000);
    wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
    
    if (wifiConnected) {
      ST7789_WriteString(200, 150, "OK", GREEN, BLACK, 1);
      ST7789_WriteString(10, 170, "Network: ", CYAN, BLACK, 1);
      ST7789_WriteString(60, 170, WIFI_SSID, WHITE, BLACK, 1);
      
      // 获取IP地址 - 增加重试次数
      uint8_t ip_retry = 0;
      while (ip_retry < 3) {
        if (ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress))) {
          ST7789_WriteString(10, 190, "IP: ", CYAN, BLACK, 1);
          ST7789_WriteString(40, 190, (const char*)ipAddress, WHITE, BLACK, 1);
          break;
        }
        HAL_Delay(500);
        ip_retry++;
      }
      
      // 获取信号强度 - 增加重试次数
      uint8_t rssi_retry = 0;
      while (rssi_retry < 3) {
        rssiValue = ESP8266_GetRSSI();
        if (rssiValue != 0) {
          char msgBuffer[128];
          sprintf(msgBuffer, "Signal: %d dBm", rssiValue);
          ST7789_WriteString(10, 210, msgBuffer, CYAN, BLACK, 1);
          break;
        }
        HAL_Delay(500);
        rssi_retry++;
      }
      
      // 初始化MQTT连接
      ST7789_WriteString(10, 230, "Init MQTT...", CYAN, BLACK, 1);
      InitializeMQTT();
      
      Beep(3);
    } else {
      ST7789_WriteString(200, 150, "FAIL", RED, BLACK, 1);
      
      // 重连尝试
      ST7789_WriteString(10, 170, "Retrying...", YELLOW, BLACK, 1);
      
      // 增加等待时间和重试次数
      for (uint8_t retry = 0; retry < 3; retry++) {
        ST7789_WriteString(100, 170, "attempt ", YELLOW, BLACK, 1);
        char retry_str[8];
        sprintf(retry_str, "%d/3", retry+1);
        ST7789_WriteString(150, 170, retry_str, YELLOW, BLACK, 1);
        
        HAL_Delay(1000);
        wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
        
        if (wifiConnected) {
          ST7789_WriteString(10, 190, "Connected!", GREEN, BLACK, 1);
          Beep(3);
          
          // 获取IP和信号强度
          ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress));
          rssiValue = ESP8266_GetRSSI();
          
          // 初始化MQTT连接
          ST7789_WriteString(10, 230, "Init MQTT...", CYAN, BLACK, 1);
          InitializeMQTT();
          
          break;
        }
      }
      
      if (!wifiConnected) {
        ST7789_WriteString(10, 190, "Failed - Hardware issue?", RED, BLACK, 1);
        BeepContinuous(500);
        
        // 显示ESP8266硬件检查提示
        ST7789_WriteString(10, 210, "Check ESP8266 power/wiring", RED, BLACK, 1);
      }
    }
  } else {
    // ESP8266通信失败
    ST7789_WriteString(200, 130, "FAIL", RED, BLACK, 1);
    ST7789_WriteString(10, 150, "ESP8266 not responding", RED, BLACK, 1);
    ST7789_WriteString(10, 170, "Check power & wiring", RED, BLACK, 1);
    
    // 尝试强制重启
    ST7789_WriteString(10, 190, "Trying restart...", YELLOW, BLACK, 1);
    ESP8266_Restart();
    
    HAL_Delay(2000); // 给ESP8266重启的时间
    
    if (ESP8266_Init(&huart1)) {
      ST7789_WriteString(10, 210, "Communication restored", GREEN, BLACK, 1);
      BeepContinuous(300);
    } else {
      ST7789_WriteString(10, 210, "Hardware problem detected", RED, BLACK, 1);
      BeepContinuous(500);
    }
  }
}

/**
  * @brief  初始化MQTT连接
  */
void InitializeMQTT(void)
{
    // 检查ESP8266是否已初始化并连接到WiFi
    if (!wifiConnected) {
        DisplayDebugInfo("WiFi not connected, can't init MQTT");
        mqtt_connected = 0;
        return;
    }
    
    DisplayDebugInfo("Connecting to MQTT broker...");
    
    // 添加串口调试输出
    sprintf(debug_buffer, "MAIN: Initializing MQTT\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
    HAL_Delay(20);
    
    // 初始化MQTT集成
    if (Integration_Init()) {
        mqtt_connected = 1;
        DisplayDebugInfo("MQTT broker connected");
        ST7789_WriteString(200, 230, "OK", GREEN, BLACK, 1);
        
        // 添加串口调试输出
        sprintf(debug_buffer, "MAIN: MQTT connected successfully!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
        HAL_Delay(20);
        
        // 发布系统状态信息
        Integration_PublishStatus("system_ready");
    } else {
        mqtt_connected = 0;
        DisplayDebugInfo("MQTT connection failed");
        ST7789_WriteString(200, 230, "FAIL", RED, BLACK, 1);
        
        // 添加串口调试输出
        sprintf(debug_buffer, "MAIN: MQTT connection failed\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
        HAL_Delay(20);
        
        // 重试连接
        HAL_Delay(2000);
        DisplayDebugInfo("Retrying MQTT...");
        if (Integration_Init()) {
            mqtt_connected = 1;
            ST7789_WriteString(200, 230, "OK", GREEN, BLACK, 1);
            
            // 添加串口调试输出
            sprintf(debug_buffer, "MAIN: MQTT connected on retry!\r\n");
            HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
            HAL_Delay(20);
            
            // 发布系统状态信息
            Integration_PublishStatus("system_ready");
        } else {
            ST7789_WriteString(200, 230, "FAIL", RED, BLACK, 1);
            
            // 添加串口调试输出
            sprintf(debug_buffer, "MAIN: MQTT connection failed after retry\r\n");
            HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
            HAL_Delay(20);
        }
    }
}

/**
  * @brief  检查MQTT状态并处理命令
  */
void CheckMQTTStatus(void)
{
    if (mqtt_connected) {
        // 检查MQTT连接是否仍然活跃
        if (!MQTT_Check()) {
            // 连接断开，尝试重连
            DisplayDebugInfo("MQTT disconnected. Reconnecting...");
            mqtt_connected = 0;
            
            // 重新初始化MQTT
            if (Integration_Init()) {
                mqtt_connected = 1;
                DisplayDebugInfo("MQTT reconnected");
            } else {
                DisplayDebugInfo("MQTT reconnect failed");
            }
        } else {
            // 处理可能的传入消息
            Integration_ProcessCommands();
        }
    } else if (wifiConnected) {
        // WiFi已连接但MQTT未连接，尝试重连MQTT
        static uint32_t last_mqtt_reconnect = 0;
        if (HAL_GetTick() - last_mqtt_reconnect > 30000) { // 每30秒尝试一次
            DisplayDebugInfo("Attempting MQTT connection...");
            if (Integration_Init()) {
                mqtt_connected = 1;
                DisplayDebugInfo("MQTT connected");
            }
            last_mqtt_reconnect = HAL_GetTick();
        }
    }
}

/**
  * @brief  处理MQTT命令
  */
void ProcessMQTTCommands(void)
{
    if (command_received) {
        command_received = 0;
        
        // 这里可以添加处理特定命令的代码
        // 例如：解析command_buffer并执行相应的操作
        
        // 临时示例：响应接收到的命令
        char response[128];
        sprintf(response, "{\"status\":\"command_received\",\"command\":\"%s\"}", command_buffer);
        MQTT_Publish(MQTT_TOPIC_STATUS, response);
        
        // 清空命令缓冲区
        memset(command_buffer, 0, sizeof(command_buffer));
    }
}

/**
  * @brief  通过MQTT发送图像文件
  * @param  filename: 图像文件名
  * @retval 1: 成功, 0: 失败
  */
uint8_t SendImageViaMQTT(const char* filename)
{
    // 检查MQTT连接状态
    if (!mqtt_connected) {
        DisplayDebugInfo("MQTT not connected. Reconnecting...");
        mqtt_connected = 0;
        
        // 尝试重新连接MQTT
        if (!Integration_Init()) {
            DisplayDebugInfo("MQTT reconnect failed");
            return 0;
        }
        DisplayDebugInfo("MQTT reconnected");
        mqtt_connected = 1;
    }
    
    // 显示进度信息
    ST7789_FillScreen(BLACK);
    ST7789_WriteString(10, 10, "MQTT Image Transfer", WHITE, BLACK, 2);
    ST7789_WriteString(10, 40, "Sending image to server...", YELLOW, BLACK, 1);
    
    // 发送图像
    DisplayDebugInfo("Sending image via MQTT...");
    
    // 用进度条显示进度
    UpdateProgressBar(20, 80, 200, 20, 0, CYAN);
    
    // 设置传输标志
    mqtt_image_transfer_in_progress = 1;
    
    // 实际发送图像
    uint8_t result = Integration_SendImage(filename);
    
    if (result) {
        // 更新进度条
        UpdateProgressBar(20, 80, 200, 20, 100, GREEN);
        ST7789_WriteString(10, 120, "Image sent successfully!", GREEN, BLACK, 1);
        
        // 等待分析结果
        ST7789_WriteString(10, 150, "Waiting for response...", YELLOW, BLACK, 1);
        
        char result_buffer[256] = {0};
        uint8_t result_received = Integration_WaitForResult(result_buffer, sizeof(result_buffer), 10000);
        
        if (result_received) {
            ST7789_WriteString(10, 180, "Response received:", GREEN, BLACK, 1);
            ST7789_WriteString(10, 210, result_buffer, WHITE, BLACK, 1);
        } else {
            ST7789_WriteString(10, 180, "No response from server", YELLOW, BLACK, 1);
        }
        
        // 发送成功音效
        Beep(3);
        Vibrate(VIBRATOR_1, 200);
    } else {
        // 更新进度条为失败状态
        UpdateProgressBar(20, 80, 200, 20, 100, RED);
        ST7789_WriteString(10, 120, "Failed to send image!", RED, BLACK, 1);
        
        // 错误音效
        Beep(1);
        HAL_Delay(200);
        Beep(1);
    }
    
    // 清除传输标志
    mqtt_image_transfer_in_progress = 0;
    mqtt_image_transfer_requested = 0;
    
    // 等待按键返回
    ST7789_WriteString(10, 240, "Press any button to return", YELLOW, BLACK, 1);
    
    uint32_t start_wait = HAL_GetTick();
    uint8_t button_pressed = 0;
    while (!button_pressed && HAL_GetTick() - start_wait < 10000) {
        if (Is_Button_Pressed() || Is_Camera_Button_Pressed()) {
            button_pressed = 1;
        }
        HAL_Delay(50);
    }
    
    // 返回相机模式并重新显示相机图像
    if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
        DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
        ST7789_WriteString(10, 10, "Camera Mode", WHITE, BLACK, 2);
        ST7789_WriteString(10, PIC_HEIGHT + 10, "Press PA0 to capture & send", YELLOW, BLACK, 1);
    }
    
    return result;
}

/**
  * @brief  USART1初始化
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  
  // 启用USART1时钟
  __HAL_RCC_USART1_CLK_ENABLE();
  
  // 启用GPIOA时钟
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // 配置USART1引脚
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;  // PA9=TX, PA10=RX
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // 初始化UART
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  显示消息
  */
void Display_Message(uint16_t y, const char* msg, uint16_t color) {
    ST7789_WriteString(0, y, msg, color, BLACK, 1);
}

/**
  * @brief  填充矩形区域
  */
void Fill_Rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    uint16_t i, j;
    for(i = 0; i < height; i++) {
        for(j = 0; j < width; j++) {
            ST7789_DrawPixel(x + j, y + i, color);
        }
    }
}

/**
  * @brief  显示摄像头图像
  */
void DisplayCameraImage(uint16_t *camera_buf, uint16_t width, uint16_t height) {
    uint16_t x, y;
    
    // 逐像素绘制图像
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            ST7789_DrawPixel(x, y, camera_buf[y * width + x]);
        }
    }
}

/**
  * @brief  更新进度条
  */
void UpdateProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress, uint16_t color) {
    // 清除原有进度条
    Fill_Rectangle(x, y, width, height, BLACK);
    
    // 计算当前进度
    uint16_t current_width = (width * progress) / 100;
    
    // 绘制进度条
    Fill_Rectangle(x, y, current_width, height, color);
    
    // 显示百分比
    sprintf(debug_buffer, "%d%%", progress);
    ST7789_WriteString(x + width + 5, y, debug_buffer, WHITE, BLACK, 1);
}

/**
  * @brief  显示开机动画并执行初始化
  * @return 初始化状态 (0: 成功, 非0: 失败)
  */
uint8_t ShowBootAnimationAndInit(void) {
  uint8_t init_status = 0;
  char debug_buffer[64];
  
  // 显示启动屏幕
  ST7789_FillScreen(BLACK);
  ST7789_WriteString(10, 80, "Camera & MQTT System", WHITE, BLACK, 2);
  ST7789_WriteString(60, 110, "System v1.0", CYAN, BLACK, 2);
  
  // 绘制进度条框架
  uint16_t barWidth = 200;
  uint16_t barHeight = 20;
  uint16_t barX = (240 - barWidth) / 2;
  uint16_t barY = 160;
  
  ST7789_DrawRectangle(barX, barY, barWidth, barHeight, WHITE);
  
  // 状态显示区域
  uint16_t statusY = barY + barHeight + 10;
  uint16_t detailY = statusY + 20;
  
  // 初始化变量
  uint8_t progress = 0;
  
  // 0-10%: 系统基础初始化
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "System Initializing...", CYAN, BLACK, 1);
  
  // 这部分已在main函数开始完成: HAL_Init, SystemClock_Config, delay_init
  for (progress = 0; progress <= 10; progress += 2) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(50);
  }
  
  // 10-25%: 初始化GPIO和按钮
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing I/O...", CYAN, BLACK, 1);
  
  MX_GPIO_Init();
  Button_Init();
  Haptic_Init(); // 初始化触觉反馈
  
  Beep(1); // 短提示
  
  for (; progress <= 25; progress += 3) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(30);
  }
  
  // 25-40%: 初始化SD卡
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing SD Card...", CYAN, BLACK, 1);
  
  sd_init_status = BSP_SD_Init();
  sprintf(debug_buffer, "SD Card: %s", sd_init_status == MSD_OK ? "OK" : "FAIL");
  ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, detailY, debug_buffer, sd_init_status == MSD_OK ? GREEN : RED, BLACK, 1);
  
  if (sd_init_status == MSD_OK) {
    // 设置中断优先级
    Setup_SDIO_Interrupts();
    
    // 获取SD卡信息
    HAL_SD_CardInfoTypeDef cardInfo;
    BSP_SD_GetCardInfo(&cardInfo);
    
    // 挂载文件系统
    fatfs_init_status = FATFS_Init();
    if (fatfs_init_status == 0) {
      ImageSave_Init();
      ImageSave_SetMode(SAVE_MODE_FATFS);
      // 确保存在必要的目录
      EnsureSDCardDirectory();
    } else if (fatfs_init_status == 101) {
      // 需要格式化
      ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
      ST7789_WriteString(barX, detailY, "Formatting SD Card...", YELLOW, BLACK, 1);
      fatfs_init_status = FATFS_Format();
      if (fatfs_init_status == 0) {
        ImageSave_Init();
        ImageSave_SetMode(SAVE_MODE_FATFS);
        ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
        ST7789_WriteString(barX, detailY, "Format successful", GREEN, BLACK, 1);
        // 创建必要的目录
        EnsureSDCardDirectory();
      }
    }
  } else {
    init_status |= 0x01; // SD卡错误标志
  }
  
  for (; progress <= 40; progress += 3) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(30);
  }
  
  // 40-60%: 初始化UART和WiFi
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing WiFi...", CYAN, BLACK, 1);
  
  MX_USART1_UART_Init();
  
  for (; progress <= 50; progress += 2) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(20);
  }
  
  // 初始化ESP8266
  uint8_t esp8266_status = ESP8266_Init(&huart1);
  if (esp8266_status) {
    ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY, "ESP8266 OK", GREEN, BLACK, 1);
  } else {
    ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY, "ESP8266 Failed", RED, BLACK, 1);
    init_status |= 0x02; // WiFi错误标志
  }
  
  // 连接WiFi
  ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, detailY, "Connecting to network...", WHITE, BLACK, 1);
  
  wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
  if (!wifiConnected) {
    // 重试一次
    wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
  }
  
  if (wifiConnected) {
    ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress));
    rssiValue = ESP8266_GetRSSI();
    
    ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY, "WiFi Connected", GREEN, BLACK, 1);
    
    // 初始化MQTT连接
    ST7789_FillRectangle(barX, detailY + 20, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY + 20, "Initializing MQTT...", YELLOW, BLACK, 1);
    InitializeMQTT();
    
    if (mqtt_connected) {
        ST7789_FillRectangle(barX, detailY + 20, barWidth, 20, BLACK);
        ST7789_WriteString(barX, detailY + 20, "MQTT Connected", GREEN, BLACK, 1);
    } else {
        ST7789_FillRectangle(barX, detailY + 20, barWidth, 20, BLACK);
        ST7789_WriteString(barX, detailY + 20, "MQTT Failed", RED, BLACK, 1);
    }
  } else {
    ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY, "WiFi Failed", RED, BLACK, 1);
    init_status |= 0x02; // WiFi错误标志
  }
  
  for (; progress <= 60; progress += 2) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(20);
  }
  
  // 60-80%: 初始化传感器
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing Sensors...", CYAN, BLACK, 1);
  
  // 初始化超声波传感器
  HC_SR04_Init();
  frontDistance = HC_SR04_ReadDistance(HC_SR04_FRONT);
  sideDistance = HC_SR04_ReadDistance(HC_SR04_SIDE);
  
  sprintf(debug_buffer, "Ultrasonic: %s", (frontDistance > 0 || sideDistance > 0) ? "OK" : "Check");
  ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, detailY, debug_buffer, (frontDistance > 0 || sideDistance > 0) ? GREEN : YELLOW, BLACK, 1);
  
  for (; progress <= 80; progress += 2) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(20);
  }
  
  // 80-100%: 初始化相机
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing Camera...", CYAN, BLACK, 1);
  
  uint8_t camera_init_result = OV7670_Init();
  sprintf(debug_buffer, "Camera: %s", camera_init_result == 0 ? "OK" : "FAIL");
  ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, detailY, debug_buffer, camera_init_result == 0 ? GREEN : RED, BLACK, 1);
  
  if (camera_init_result != 0) {
    // 简单的相机重试
    camera_init_result = OV7670_Init();
    if (camera_init_result != 0) {
      init_status |= 0x04; // 相机错误标志
    }
  }
  
  for (; progress <= 100; progress += 4) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(30);
  }
  
  // 总结初始化状态
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  
  if (init_status == 0) {
    ST7789_WriteString(barX, statusY, "System Ready!", GREEN, BLACK, 1);
    Beep(3); // 成功提示音
  } else {
    sprintf(debug_buffer, "Ready with warnings (%02X)", init_status);
    ST7789_WriteString(barX, statusY, debug_buffer, YELLOW, BLACK, 1);
    Beep(2); // 警告提示音
  }
  
  HAL_Delay(1000);
  return init_status;
}

/**
  * @brief  显示参数屏幕
  */
void DisplayParameterScreen(void) {
    if (currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;
    
    ST7789_FillScreen(BLACK);
    ST7789_WriteString(10, 10, "Camera & MQTT System", WHITE, BLACK, 2);
    ST7789_WriteString(10, 50, "WiFi: ", WHITE, BLACK, 2);
    ST7789_WriteString(90, 50, wifiConnected ? "Connected" : "Not Connected", 
                     wifiConnected ? GREEN : RED, BLACK, 2);
    
    // 固定界面元素
    ST7789_WriteString(10, 90, "Front:", WHITE, BLACK, 2);
    ST7789_WriteString(10, 120, "Side:", WHITE, BLACK, 2);
    
    // SD卡状态
    ST7789_WriteString(10, 150, "SD Card: Ready", GREEN, BLACK, 1);
    
    // WiFi信息区域
    if (wifiConnected) {
        ST7789_WriteString(10, 175, "Signal: Updating...", YELLOW, BLACK, 1);
        ST7789_WriteString(10, 195, "IP: Updating...", CYAN, BLACK, 1);
        
        // 添加MQTT状态信息
        if (mqtt_connected) {
            ST7789_WriteString(10, 215, "MQTT: Connected", GREEN, BLACK, 1);
        } else {
            ST7789_WriteString(10, 215, "MQTT: Disconnected", RED, BLACK, 1);
        }
    }
    
    // 相机状态
    ST7789_WriteString(10, 235, "Camera: Ready", GREEN, BLACK, 1);
    
    // 更新使用说明，指示需要长按
    ST7789_WriteString(10, 255, "Long press mode btn (PA15) for camera", WHITE, BLACK, 1);
    
    // 调试信息区域
    ST7789_WriteString(10, 280, "System Ready", GREEN, BLACK, 1);
}

/**
  * @brief  更新屏幕上的距离显示
  */
void UpdateDistanceDisplay(void) {
  if (currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;
                            
  // 更新前方距离
  ST7789_FillRectangle(100, 90, 130, 20, BLACK);
  if (frontDistance > 0 && frontDistance <= DISTANCE_MAX) {
    sprintf(debug_buffer, "%.1f cm", frontDistance);
    uint16_t color;
    if (frontDistance < DISTANCE_CLOSE) {
      color = RED;
    } else if (frontDistance < DISTANCE_MEDIUM) {
      color = YELLOW;
    } else {
      color = GREEN;
    }
    ST7789_WriteString(100, 90, debug_buffer, color, BLACK, 2);
  } else {
    ST7789_WriteString(100, 90, "---", LIGHTGREY, BLACK, 2);
  }
  
  // 更新侧面距离
  ST7789_FillRectangle(100, 120, 130, 20, BLACK);
  if (sideDistance > 0 && sideDistance <= DISTANCE_MAX) {
    sprintf(debug_buffer, "%.1f cm", sideDistance);
    uint16_t color;
    if (sideDistance < DISTANCE_CLOSE) {
      color = RED;
    } else if (sideDistance < DISTANCE_MEDIUM) {
      color = YELLOW;
    } else {
      color = GREEN;
    }
    ST7789_WriteString(100, 120, debug_buffer, color, BLACK, 2);
  } else {
    ST7789_WriteString(100, 120, "---", LIGHTGREY, BLACK, 2);
  }
}

/**
  * @brief  显示调试信息
  */
void DisplayDebugInfo(const char* info) {
  ST7789_FillRectangle(10, 270, 220, 10, BLACK);
  ST7789_WriteString(10, 270, info, YELLOW, BLACK, 1);
  
  // 串口调试输出
  sprintf(debug_buffer, "DEBUG: %s\r\n", info);
  HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
  HAL_Delay(20); // 增加短延迟确保传输完成
}

/**
  * @brief  按钮初始化
  */
void Button_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 启用GPIOA时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // 配置PA15为输入模式(模式切换按钮)
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // 配置PA0为输入模式(拍照按钮)
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  检测PA15按钮是否按下，考虑按键抖动
  */
uint8_t Is_Button_Pressed(void) {
    static uint8_t last_state = 1;    // 保存上一次按键状态
    static uint32_t debounce_time = 0; // 抖动时间
    
    uint8_t current_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15);
    
    // 按键状态发生变化且已达到抖动间隔
    if (current_state != last_state && HAL_GetTick() - debounce_time > 100) {
        debounce_time = HAL_GetTick();
        last_state = current_state;
        
        // 如果检测到按键被按下，返回1
        if (current_state == GPIO_PIN_RESET) {
            return 1;
        }
    }
    
    return 0;
}

/**
  * @brief  检测PA0按钮是否按下，考虑按键抖动
  */
uint8_t Is_Camera_Button_Pressed(void) {
    static uint8_t last_state = 1;    // 保存上一次按键状态
    static uint32_t debounce_time = 0; // 抖动时间
    
    uint8_t current_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    
    // 按键状态发生变化且已达到抖动间隔
    if (current_state != last_state && HAL_GetTick() - debounce_time > 100) {
        debounce_time = HAL_GetTick();
        last_state = current_state;
        
        // 如果检测到按键被按下，返回1
        if (current_state == GPIO_PIN_RESET) {
            return 1;
        }
    }
    
    return 0;
}

/**
  * @brief  按钮处理函数的完整实现
  */
void ProcessButton(void) {
    static uint8_t lastModeButtonReading = GPIO_PIN_SET;    // 上次读取PA15的状态
    static uint8_t modeButtonState = GPIO_PIN_SET;          // 当前消抖后的PA15状态
    static uint32_t lastModeButtonDebounceTime = 0;         // PA15的消抖时间
    
    static uint8_t lastCameraButtonReading = GPIO_PIN_SET;  // 上次读取PA0的状态
    static uint8_t cameraButtonState = GPIO_PIN_SET;        // 当前消抖后的PA0状态
    static uint32_t lastCameraButtonDebounceTime = 0;       // PA0的消抖时间
    
    // 开机后防止自动模式切换的保护期
    if (systemJustStarted) {
        if (HAL_GetTick() > 3000) { // 开机3秒后才启用按钮
            systemJustStarted = 0;
            DisplayDebugInfo("Buttons Enabled");
        } else {
            return; // 忽略启动期间的所有按钮事件
        }
    }
    
    // 防止意外模式切换的逻辑
    if (currentSystemState == SYSTEM_STATE_CAMERA_MODE && autoModeChangeProtectionTimer == 0) {
        // 刚进入相机模式，启动保护计时器
        autoModeChangeProtectionTimer = HAL_GetTick() + 1000; // 1秒保护期
    }
    
    if (autoModeChangeProtectionTimer > 0 && HAL_GetTick() < autoModeChangeProtectionTimer) {
        // 在保护期内，不处理任何按钮事件
        return;
    }
    
    // ---- 处理模式切换按钮 (PA15) ----
    uint8_t modeReading = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15);
    
    // 添加原始按钮状态的调试输出
    if (modeReading != lastModeButtonReading) {
        char dbgMsg[32];
        sprintf(dbgMsg, "Mode Button Raw: %s", modeReading == GPIO_PIN_RESET ? "DOWN" : "UP");
        DisplayDebugInfo(dbgMsg);
        lastModeButtonReading = modeReading;
        lastModeButtonDebounceTime = HAL_GetTick(); // 重置消抖计时器
    }
    
    // 消抖延迟处理
    if ((HAL_GetTick() - lastModeButtonDebounceTime) > BUTTON_DEBOUNCE_TIME) {
        // 如果读取稳定且与当前状态不同，则更新
        if (modeReading != modeButtonState) {
            modeButtonState = modeReading;
            
            // 按钮按下事件 (低电平)
            if (modeButtonState == GPIO_PIN_RESET && !buttonPressed) {
                buttonPressed = 1;
                buttonPressTick = HAL_GetTick();
                buttonLongPressDetected = 0;
                
                // 显示按钮按下状态
                DisplayDebugInfo("Mode Button: DOWN");
            }
            // 按钮释放事件 (高电平)
            else if (modeButtonState == GPIO_PIN_SET && buttonPressed) {
                // 处理短按(如果没有检测到长按)
                if (!buttonLongPressDetected) {
                    // PA15短按现在不做任何操作
                    DisplayDebugInfo("Mode Button: SHORT PRESS (no action)");
                }
                
                buttonPressed = 0;
                
                // 显示按钮抬起状态
                DisplayDebugInfo("Mode Button: UP");
            }
        }
    }
    
    // PA15长按检测
    if (buttonPressed && !buttonLongPressDetected && (HAL_GetTick() - buttonPressTick > BUTTON_LONGPRESS_TIME)) {
        buttonLongPressDetected = 1;
        
        // 显示长按检测
        DisplayDebugInfo("Mode Button: LONG PRESS");
        
        // 参数显示模式下长按切换到相机模式
        if (currentSystemState == SYSTEM_STATE_PARAM_DISPLAY) {
            // 显示切换信息
            DisplayDebugInfo("Long Press - Entering Camera");
            
            // 延迟一点时间再切换以避免误触
            HAL_Delay(200);
            
            currentSystemState = SYSTEM_STATE_CAMERA_MODE;
            
            // 启动保护计时器
            autoModeChangeProtectionTimer = HAL_GetTick() + 1000;
            
            // 清屏，准备进入相机模式
            ST7789_FillScreen(BLACK);
            ST7789_WriteString(10, 10, "Camera Mode", WHITE, BLACK, 2);
            ST7789_WriteString(10, 40, "Mode button: hold to exit", YELLOW, BLACK, 1);
            ST7789_WriteString(10, 60, "Camera button: press to capture", YELLOW, BLACK, 1);
        }
        // 相机模式下长按切换到参数显示模式
        else if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
            // 显示切换信息
            DisplayDebugInfo("Long Press - Exiting Camera");
            
            // 延迟一点时间再切换
            HAL_Delay(200);
            
            // 先更新状态
            currentSystemState = SYSTEM_STATE_PARAM_DISPLAY;
            
            // 取消所有拍照请求
            capture_requested = 0;
            
            // 调用增强型WiFi通信恢复函数
            RestoreWiFiCommunication();
            
            // 显示参数屏幕
            DisplayParameterScreen();
            
            // 重置计时器以强制立即更新
            lastDistanceUpdateTick = 0;
            lastWifiRssiUpdateTick = 0;
            
            // 读取一次距离以确保显示正确
            frontDistance = HC_SR04_ReadDistance(HC_SR04_FRONT);
            sideDistance = HC_SR04_ReadDistance(HC_SR04_SIDE);
            
            // 更新显示
            UpdateDistanceDisplay();
            
            // 确保系统稳定
            HAL_Delay(100);
        }
    }
    
    // ---- 处理拍照按钮 (PA0) ----
    // 只在相机模式下处理拍照按钮
    if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
        uint8_t cameraReading = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
        
        // 添加拍照按钮状态的调试输出
        if (cameraReading != lastCameraButtonReading) {
            char dbgMsg[32];
            sprintf(dbgMsg, "Camera Button Raw: %s", cameraReading == GPIO_PIN_RESET ? "DOWN" : "UP");
            DisplayDebugInfo(dbgMsg);
            lastCameraButtonReading = cameraReading;
            lastCameraButtonDebounceTime = HAL_GetTick(); // 重置消抖计时器
        }
        
        // 消抖延迟处理
        if ((HAL_GetTick() - lastCameraButtonDebounceTime) > BUTTON_DEBOUNCE_TIME) {
            // 如果读取稳定且与当前状态不同，则更新
            if (cameraReading != cameraButtonState) {
                cameraButtonState = cameraReading;
                
                // 按钮按下事件 (低电平)
                if (cameraButtonState == GPIO_PIN_RESET && !cameraButtonPressed) {
                    cameraButtonPressed = 1;
                    cameraButtonPressTick = HAL_GetTick();
                    
                    // 显示按钮按下状态
                    DisplayDebugInfo("Camera Button: DOWN");
                }
                // 按钮释放事件 (高电平)
                else if (cameraButtonState == GPIO_PIN_SET && cameraButtonPressed) {
                    // 拍照按钮释放时拍照
                    capture_requested = 1;
                    DisplayDebugInfo("Camera Button pressed - Photo requested");
                    Buzzer_Beep_Short(); // 按钮按下提示音
                    
                    cameraButtonPressed = 0;
                    
                    // 显示按钮抬起状态
                    DisplayDebugInfo("Camera Button: UP");
                }
            }
        }
    }
}

/**
  * @brief  更新参数显示模式
  */
void ParameterDisplayModeUpdate(void) {
  if (currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;

  // 读取距离传感器
  frontDistance = HC_SR04_ReadDistance(HC_SR04_FRONT);
  sideDistance = HC_SR04_ReadDistance(HC_SR04_SIDE);
  
  // 每200ms更新距离显示
  if (HAL_GetTick() - lastDistanceUpdateTick >= 200 || lastDistanceUpdateTick == 0) {
    lastDistanceUpdateTick = HAL_GetTick();
    UpdateDistanceDisplay();
  }
  
  // 每5秒更新WiFi信息（信号强度）
  if (wifiConnected && (HAL_GetTick() - lastWifiRssiUpdateTick >= 5000 || lastWifiRssiUpdateTick == 0)) {
    lastWifiRssiUpdateTick = HAL_GetTick();
    rssiValue = ESP8266_GetRSSI();
    UpdateWiFiInfoDisplay();
  }
  
  // 每15秒更新IP地址
  if (wifiConnected && (HAL_GetTick() - lastWifiIpUpdateTick >= 15000 || lastWifiIpUpdateTick == 0)) {
    lastWifiIpUpdateTick = HAL_GetTick();
    ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress));
    UpdateWiFiInfoDisplay();
  }
  
  // 检查MQTT状态并处理命令
  if (wifiConnected) {
    CheckMQTTStatus();
    ProcessMQTTCommands();
  }
}

/**
  * @brief  更新相机模式
  */
void CameraModeUpdate(void) {
    if (currentSystemState != SYSTEM_STATE_CAMERA_MODE) return;
  
    // 判断是否处于图像传输状态
    static uint8_t showing_mqtt_transfer = 0;
    static uint32_t mqtt_transfer_start_time = 0;
    
    // 如果有新帧数据且不在传输过程中
    if (ov_rev_ok && !showing_mqtt_transfer) {
        // 显示帧处理信息
        Display_Message(DEBUG_START_Y + 60, "Processing new frame...", YELLOW);
        
        // 显示图像
        DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
        frame_counter++;
        
        // 显示拍照提示
        ST7789_WriteString(10, 10, "Camera Mode", WHITE, BLACK, 2);
        ST7789_WriteString(10, PIC_HEIGHT + 10, "Press PA0 to capture & send", YELLOW, BLACK, 1);
        
        // 如果需要拍照且SD卡初始化成功
        if (capture_requested && sd_init_status == MSD_OK && fatfs_init_status == 0) {
            // 记录开始时间
            photo_save_start_time = HAL_GetTick();
            
            // 显示保存状态
            Display_Message(DEBUG_START_Y + 80, "Saving image to SD card...", YELLOW);
            Fill_Rectangle(20, 60, 200, 80, BLUE); // 蓝色背景
            ST7789_WriteString(40, 70, "SAVING PHOTO...", WHITE, BLUE, 1);
            
            // 使用ImageSave系统保存图像
            if (ImageSave_IsIdle()) {
                // 启动图像保存
                uint8_t start_result = ImageSave_StartCapture(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
                sprintf(debug_buffer, "Start save: %d", start_result);
                Display_Message(DEBUG_START_Y + 90, debug_buffer, start_result ? GREEN : RED);
                
                // 处理保存过程
                uint32_t process_start = HAL_GetTick();
                uint32_t process_count = 0;
                uint32_t last_progress = 0;
                uint8_t progress_bar_width = 160; // 进度条宽度
                
                // 在写入过程中显示进度条
                ST7789_WriteString(40, 90, "Progress: ", WHITE, BLUE, 1);
                Fill_Rectangle(110, 90, progress_bar_width, 10, BLACK); // 进度条背景
                
                // 显示使用的保存模式
                sprintf(debug_buffer, "Mode: %s", (save_mode == SAVE_MODE_FATFS) ? "FatFs" : "Direct (DMA)");
                ST7789_WriteString(40, 110, debug_buffer, WHITE, BLUE, 1);
                
                // 保存状态处理循环
                uint32_t last_debug_time = HAL_GetTick();
                
                while (!ImageSave_IsIdle() && HAL_GetTick() - process_start < 30000) {
                    // 处理一步保存操作
                    ImageSave_Process();
                    process_count++;
                    
                    // 获取当前状态
                    SaveState_t current_state = ImageSave_GetState();
                    uint8_t error_code = ImageSave_GetError();
                    
                    // 获取并显示当前进度
                    uint8_t current_progress = ImageSave_GetProgress();
                    
                    // 更新进度条
                    if (current_progress != last_progress) {
                        last_progress = current_progress;
                        // 绘制进度条
                        uint16_t bar_width = (progress_bar_width * current_progress) / 100;
                        Fill_Rectangle(110, 90, bar_width, 10, GREEN); // 已完成部分
                        
                        // 显示进度百分比
                        sprintf(debug_buffer, "%d%%", current_progress);
                        ST7789_WriteString(110 + progress_bar_width + 5, 90, debug_buffer, WHITE, BLUE, 1);
                    }
                    
                    // 每50ms更新一次调试信息
                    if (HAL_GetTick() - last_debug_time > 50) {
                        // 显示调试信息
                        const char* debug_str = ImageSave_GetDebugInfo();
                        sprintf(debug_buffer, "Debug: %s", debug_str);
                        Display_Message(DEBUG_START_Y + 100, debug_buffer, CYAN);
                        
                        // 显示状态和错误码
                        sprintf(debug_buffer, "State: %d Err: %d", current_state, error_code);
                        Display_Message(DEBUG_START_Y + 130, debug_buffer, YELLOW);
                        
                        // 显示保存状态
                        Display_Save_Status();
                        
                        // 更新时间戳
                        last_debug_time = HAL_GetTick();
                    }
                    
                    // 处理按键，允许用户取消
                    ProcessButton();
                    
                    HAL_Delay(5); // 增加延时，减轻CPU负担
                }
                
                // 检查保存结果
                if (ImageSave_GetError() == SAVE_ERROR_NONE && ImageSave_IsIdle()) {
                    // 保存成功
                    uint32_t save_time = HAL_GetTick() - photo_save_start_time;
                    
                    // 获取当前文件索引
                    current_file_index = ImageSave_GetFileIndex();
                    
                    // 显示成功消息
                    Fill_Rectangle(20, 60, 200, 80, GREEN); // 绿色背景
                    ST7789_WriteString(30, 70, "PHOTO SAVED!", BLACK, GREEN, 2);
                    sprintf(debug_buffer, "Image #%u", (unsigned int)current_file_index);
                    ST7789_WriteString(45, 100, debug_buffer, BLACK, GREEN, 1);
                    sprintf(debug_buffer, "Time: %ums", (unsigned int)save_time);
                    ST7789_WriteString(45, 120, debug_buffer, BLACK, GREEN, 1);
                    
                    // 重命名为IMAGE.JPG用于MQTT传输
                    char source_file[32];
                    sprintf(source_file, "0:/IMG_%04u.BMP", (unsigned int)current_file_index);
                    f_rename(source_file, IMAGE_FILENAME);
                    
                    // 成功提示音
                    Buzzer_Beep_Short();
                    
                    // 设置成功标志，但不立即显示
                    photo_saved_success = 1;
                    success_display_time = HAL_GetTick();
                    
                    // 检查MQTT连接状态并准备发送图像
                    if (mqtt_connected) {
                        // 显示正在准备MQTT图像传输
                        Display_Message(DEBUG_START_Y + 140, "Preparing MQTT image transfer...", YELLOW);
                        
                        // 添加串口调试输出
                        sprintf(debug_buffer, "\r\n[CAMERA_MODE] Photo saved, preparing to send over MQTT\r\n");
                        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
                        
                        // 设置MQTT传输请求标志
                        mqtt_image_transfer_requested = 1;
                        mqtt_image_transfer_in_progress = 0;
                        
                        // 设置传输状态标志
                        showing_mqtt_transfer = 1;
                        mqtt_transfer_start_time = HAL_GetTick();
                    } else {
                        // 显示MQTT未连接提示
                        Display_Message(DEBUG_START_Y + 140, "MQTT not connected, image saved only", YELLOW);
                        
                        // 添加串口调试输出
                        sprintf(debug_buffer, "\r\n[CAMERA_MODE] Photo saved, but MQTT not connected\r\n");
                        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
                    }
                } else {
                    // 保存失败或超时
                    Fill_Rectangle(20, 60, 200, 80, RED); // 红色背景
                    ST7789_WriteString(30, 70, "SAVE FAILED!", WHITE, RED, 2);
                    sprintf(debug_buffer, "Error: %d", ImageSave_GetError());
                    ST7789_WriteString(45, 100, debug_buffer, WHITE, RED, 1);
                    sprintf(debug_buffer, "Steps: %u", (unsigned int)process_count);
                    ST7789_WriteString(45, 120, debug_buffer, WHITE, RED, 1);
                    
                    // 失败提示音
                    Buzzer_Beep_Times(3, 200);
                    
                    // 设置失败后等待一会儿，然后继续预览
                    HAL_Delay(3000);
                    DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
                }
            } else {
                // 系统忙，不能保存
                Fill_Rectangle(20, 60, 200, 40, RED);
                ST7789_WriteString(40, 70, "SYSTEM BUSY", WHITE, RED, 1);
                Buzzer_Beep_Short();
            }
            
            // 重置请求标志
            capture_requested = 0;
        }
        
        // 帧处理完成
        Display_Message(DEBUG_START_Y + 60, "Frame processed!", GREEN);
        
        // 重置标志
        ov_rev_ok = 0;
        
        // 重新启动DCMI捕获下一帧
        DCMI_Start();
    }
    
    // 处理MQTT图像传输逻辑
    if (showing_mqtt_transfer) {
        // 检查延迟启动时间
        if (HAL_GetTick() - mqtt_transfer_start_time > 500) {
            // 重置传输标志
            showing_mqtt_transfer = 0;
            
            if (mqtt_image_transfer_requested && !mqtt_image_transfer_in_progress) {
                // 设置传输进行中标志
                mqtt_image_transfer_in_progress = 1;
                
                // 开始MQTT图像传输
                SendImageViaMQTT(IMAGE_FILENAME);
                
                // 清除标志
                mqtt_image_transfer_requested = 0;
                mqtt_image_transfer_in_progress = 0;
            }
            
            // 传输完成后恢复预览
            HAL_Delay(1000); // 短暂显示
            DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
        }
    }
}

/**
  * @brief  检查DMA状态
  */
void Check_DMA_Status(void) {
    // 读取DMA状态寄存器
    uint32_t dma_isr = DMA2->LISR; // 包含状态信息寄存器，用于Stream 0-3
    
    // Stream 1标志标志
    uint8_t teif = (dma_isr & DMA_LISR_TEIF1) ? 1 : 0; // 传输错误标志
    uint8_t htif = (dma_isr & DMA_LISR_HTIF1) ? 1 : 0; // 半传输
    uint8_t tcif = (dma_isr & DMA_LISR_TCIF1) ? 1 : 0; // 传输完成
    uint8_t feif = (dma_isr & DMA_LISR_FEIF1) ? 1 : 0; // FIFO错误
    
    // 显示DMA状态
    sprintf(debug_buffer, "DMA: E:%d H:%d C:%d F:%d", teif, htif, tcif, feif);
    Display_Message(DEBUG_START_Y + 40, debug_buffer, YELLOW);
}

/**
  * @brief  显示DCMI状态
  */
void Display_DCMI_Status(void) {
    uint32_t dcmi_cr = hdcmi.Instance->CR;  // 控制寄存器
    uint32_t dcmi_sr = hdcmi.Instance->SR;  // 状态寄存器
    
    // 显示DCMI状态
    sprintf(debug_buffer, "DCMI CR:0x%02X SR:0x%02X", 
            (unsigned int)(dcmi_cr & 0xFF), 
            (unsigned int)(dcmi_sr & 0xFF));
    Display_Message(DEBUG_START_Y + 20, debug_buffer, CYAN);
}

/**
  * @brief  显示像素预览
  */
void Display_Pixel_Preview(void) {
    if(ov_rev_ok) {
        sprintf(debug_buffer, "PX: %04X %04X %04X %04X", 
                camera_buffer[0], camera_buffer[1], 
                camera_buffer[2], camera_buffer[3]);
        Display_Message(DEBUG_START_Y + 50, debug_buffer, WHITE);
    }
}

/**
  * @brief  显示SD卡存储状态
  */
void Display_Save_Status(void) {
    const char* state_names[] = {
        "IDLE", "WAITING", "PREPARING", "WR_HEADER", "WR_DATA", "FINISHING", "ERROR"
    };
    
    SaveState_t state = ImageSave_GetState();
    sprintf(debug_buffer, "Save state: %s", 
            (state < 7) ? state_names[state] : "UNKNOWN");
    Display_Message(DEBUG_START_Y + 120, debug_buffer, MAGENTA);
}

/**
  * @brief  设置SDIO中断优先级
  */
void Setup_SDIO_Interrupts(void) {
    // 设置SDIO和DMA中断优先级
    HAL_NVIC_SetPriority(SDIO_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
}

/**
  * @brief  确保SD卡上的目录存在
  */
void EnsureSDCardDirectory(void) {
  FRESULT res;
  DIR dir;
  
  // Check and create image save directory
  res = f_opendir(&dir, "0:/IMAGES");
  if (res != FR_OK) {
    // Directory doesn't exist, create it
    res = f_mkdir("0:/IMAGES");
    if (res == FR_OK) {
      DisplayDebugInfo("Created IMAGES directory");
    } else {
      DisplayDebugInfo("Failed to create directory");
    }
  } else {
    f_closedir(&dir);
  }
}

/**
  * @brief  GPIO初始化
  */
void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // 使能GPIO端口时钟
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // 初始化必要的输出引脚
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // LED引脚
  
  // 配置LED输出引脚
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // 初始化按钮
  Button_Init();
}

/**
  * @brief  主入口点
  * @retval int
  */
int main(void)
{
  // 系统初始化
  HAL_Init();
  SystemClock_Config();
  
  // 初始化延时
  delay_init();
  
  // 初始化LCD
  ST7789_Init();
  ST7789_FillScreen(BLACK);
  
  // 显示启动动画并执行所有初始化
  uint8_t init_status = ShowBootAnimationAndInit();
  
  // 在启动DCMI图像采集前先显示参数屏幕
  DisplayParameterScreen();
  
  // 启动DCMI图像采集
  DCMI_Start();
  
  // 主循环
  uint32_t last_update = 0;
  uint32_t loop_counter = 0;
  uint32_t mqtt_status_check_time = 0;
  
  // 设置初始模式
  currentSystemState = SYSTEM_STATE_PARAM_DISPLAY;
  
  // 更新一次距离传感器读数，保证参数页面有数据显示
  frontDistance = HC_SR04_ReadDistance(HC_SR04_FRONT);
  sideDistance = HC_SR04_ReadDistance(HC_SR04_SIDE);
  UpdateDistanceDisplay();
  
  // 调试信息
  sprintf(debug_buffer, "System ready (%02X)", init_status);
  DisplayDebugInfo(debug_buffer);
  
  // 添加串口调试输出
  sprintf(debug_buffer, "\r\n[MAIN] System initialized and ready\r\n");
  HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
  
  // MQTT状态信息
  if (mqtt_connected) {
      sprintf(debug_buffer, "[MAIN] MQTT is connected to broker\r\n");
      HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
      
      // 发布系统就绪状态
      Integration_PublishStatus("system_ready");
  } else {
      sprintf(debug_buffer, "[MAIN] MQTT is not connected\r\n");
      HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
  }
  
  while(1) {
    // 计数主循环迭代
    loop_counter++;
    
    // 处理按钮事件
    ProcessButton();
    
    // 基于当前状态更新
    if (currentSystemState == SYSTEM_STATE_PARAM_DISPLAY) {
      ParameterDisplayModeUpdate();
      
      // 在参数显示模式下，定期检查MQTT状态和处理命令
      if (HAL_GetTick() - mqtt_status_check_time > 5000) { // 每5秒检查一次
        mqtt_status_check_time = HAL_GetTick();
        
        if (wifiConnected) {
          CheckMQTTStatus();
          ProcessMQTTCommands();
        }
      }
    } else if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
      CameraModeUpdate();
    }
    
    // 每500ms更新一次状态信息
    if(HAL_GetTick() - last_update > 500) {
      last_update = HAL_GetTick();
      
      if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
        // 在相机模式下显示帧计数
        sprintf(debug_buffer, "Frames: %u  Loops: %u", 
                (unsigned int)frame_counter, 
                (unsigned int)loop_counter);
        Display_Message(DEBUG_START_Y, debug_buffer, WHITE);
        
        // 显示OK标志和datanum
        sprintf(debug_buffer, "OK: %d  DataNum: %u", 
                ov_rev_ok, (unsigned int)datanum);
        Display_Message(DEBUG_START_Y + 10, debug_buffer, GREEN);
        
        // 更新DCMI和DMA状态
        Display_DCMI_Status();
        Check_DMA_Status();
        
        // 显示像素内容
        Display_Pixel_Preview();
        
        // 检查DCMI是否仍在运行
        uint8_t dcmi_running = (hdcmi.Instance->CR & DCMI_CR_CAPTURE) != 0;
        if(!dcmi_running && !ov_rev_ok) {
          // DCMI没有运行，重新启动
          Display_Message(DEBUG_START_Y + 70, "DCMI not running, restarting...", RED);
          DCMI_Start();
          Display_Message(DEBUG_START_Y + 70, "DCMI restarted", GREEN);
          Buzzer_Beep_Short(); // 重启提示音
        }
      }
    }
    
    // 成功拍照显示3秒后隐藏提示框并恢复图像
    if (photo_saved_success && (HAL_GetTick() - success_display_time > 3000)) {
      photo_saved_success = 0;
      // 重新显示摄像头内容
      DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
      Buzzer_Beep_Short(); // 提示音
    }
    
    HAL_Delay(1); // 短延时减少循环负载
  }
}

/**
  * @brief  错误处理函数
  */
void Error_Handler(void)
{
  __disable_irq();
  
  ST7789_FillScreen(RED);
  ST7789_WriteString(10, 120, "SYSTEM ERROR!", WHITE, RED, 2);
  
  Buzzer_Beep_Times(5, 100); // 错误提示音
  
  HAL_Delay(3000);
  
  // 系统复位
  NVIC_SystemReset();
  
  while (1)
  {
  } 
}

// BSP写入完成回调
void BSP_SD_WriteCpltCallback(void)
{
    SD_DMA_TxComplete();
}

// BSP中断回调
void BSP_SD_AbortCallback(void)
{
    SD_DMA_TxError();
}

// SD卡DMA传输中断处理
void BSP_SD_DMA_Tx_IRQHandler(void)
{
    extern SD_HandleTypeDef uSdHandle;
    HAL_DMA_IRQHandler(uSdHandle.hdmatx);
}

// SD卡SDIO中断处理
void BSP_SD_IRQHandler(void)
{
    extern SD_HandleTypeDef uSdHandle;
    HAL_SD_IRQHandler(&uSdHandle);
}

// MQTT命令接收回调函数
void MQTT_CommandCallback(const char* command)
{
    // 将命令保存到全局缓冲区
    strncpy(command_buffer, command, sizeof(command_buffer)-1);
    command_buffer[sizeof(command_buffer)-1] = '\0';
    
    // 设置命令接收标志
    command_received = 1;
    
    // 添加串口调试输出
    sprintf(debug_buffer, "[MQTT] Command received: %s\r\n", command);
    HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
    
    // 根据命令内容进行不同的操作
    if (strstr(command, "capture") != NULL) {
        // 如果是拍照命令且当前在相机模式，则设置拍照请求标志
        if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
            capture_requested = 1;
            // 发送确认消息
            Integration_PublishStatus("capture_requested");
        } else {
            // 如果不在相机模式，发送错误信息
            Integration_PublishStatus("error_not_in_camera_mode");
        }
    } else if (strstr(command, "status") != NULL) {
        // 如果是状态查询命令，发送当前系统状态
        char status_data[256];
        sprintf(status_data, "{\"mode\":\"%s\",\"wifi\":%d,\"mqtt\":%d,\"sd_card\":%d}", 
                currentSystemState == SYSTEM_STATE_CAMERA_MODE ? "camera" : "parameter",
                wifiConnected, mqtt_connected, sd_init_status == MSD_OK ? 1 : 0);
        MQTT_Publish(MQTT_TOPIC_STATUS, status_data);
    }
}

/**
  * @brief  系统时钟配置
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7; // 分频为7得到48MHz时钟用于SDIO
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
      while(1);
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
      while(1);
  }
  
  /* 配置MCO1输出HSI时钟作为相机的XCLK */
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_4);
}
