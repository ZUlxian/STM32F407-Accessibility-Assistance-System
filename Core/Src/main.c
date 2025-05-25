/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ͼ��ɼ���MQTT����ϵͳ
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

// ��ʾ�߶�
#define DISPLAY_HEIGHT 240

// ������Ϣ��ʾ����ʼ��
#define DEBUG_START_Y 160

// ��ť�������
#define BUTTON_LONGPRESS_TIME 2000  // ����ʱ����ֵ(����)
#define BUTTON_DEBOUNCE_TIME  50    // ��ť����ʱ��(����)

// ������ֵ
#define DISTANCE_CLOSE    15.0f  // �ǳ����ľ�����ֵ(����)
#define DISTANCE_MEDIUM   50.0f  // �еȾ�����ֵ(����)
#define DISTANCE_MAX     100.0f  // �����Ч������ֵ(����)

/* Private typedefs -----------------------------------------------------------*/
typedef enum {
  SYSTEM_STATE_PARAM_DISPLAY,  // ������ʾģʽ
  SYSTEM_STATE_CAMERA_MODE     // ���ģʽ
} SystemState;

/* Global variables ---------------------------------------------------------*/
// �������
extern uint32_t datanum;
extern uint8_t ov_rev_ok;
extern DMA_HandleTypeDef hdma_dcmi;
extern uint16_t camera_buffer[PIC_WIDTH*PIC_HEIGHT];

// UART����
UART_HandleTypeDef huart1;

// WiFi����
volatile uint8_t wifiConnected = 0;
volatile char ipAddress[20] = "Updating...";
volatile int8_t rssiValue = 0;
volatile uint32_t lastWifiRssiUpdateTick = 0;
volatile uint32_t lastWifiIpUpdateTick = 0;

// MQTT����
volatile uint8_t mqtt_connected = 0;
volatile uint8_t command_received = 0;
char command_buffer[128] = {0};

// ����ȫ�ֱ���
uint32_t frame_counter = 0;
uint8_t init_retry_count = 0;
uint8_t sd_init_status = 0;
uint8_t fatfs_init_status = 0;
uint8_t capture_requested = 0;    // ���������־
uint8_t photo_saved_success = 0;  // ���ճɹ���־
uint32_t success_display_time = 0; // ���ճɹ���ʾ��ʾʱ��
uint32_t photo_save_start_time = 0; // ���տ�ʼʱ��
uint32_t save_progress_time = 0;   // ������ȸ���ʱ��
uint32_t current_file_index = 1;   // ��ǰ�ļ�����
uint8_t save_mode = SAVE_MODE_FATFS;
uint32_t sd_status_check_time = 0; // SD��״̬���ʱ��
uint8_t mqtt_image_transfer_requested = 0; // MQTTͼ���������־
uint8_t mqtt_image_transfer_in_progress = 0; // MQTTͼ��������б�־
DMA_HandleTypeDef hdma_spi2_tx;
DMA_HandleTypeDef hdma_spi3_tx;
I2S_HandleTypeDef hi2s3;

// ϵͳ״̬����
volatile SystemState currentSystemState = SYSTEM_STATE_PARAM_DISPLAY;
volatile uint8_t systemJustStarted = 1;              // ��������־
volatile uint32_t autoModeChangeProtectionTimer = 0; // ��ֹ�Զ�ģʽ�л�

// ģʽ�л���ť�������(PA15)
volatile uint8_t buttonPressed = 0;
volatile uint32_t buttonPressTick = 0;
volatile uint8_t buttonLongPressDetected = 0;
volatile uint8_t buttonStateChanged = 0;
volatile uint32_t lastButtonChangeTime = 0; // ���ڰ�ť����

// ���հ�ť�������(PA0)
volatile uint8_t cameraButtonPressed = 0;
volatile uint32_t cameraButtonPressTick = 0;

// ����������������
volatile float frontDistance = 0.0f;
volatile float sideDistance = 0.0f;
volatile uint32_t lastDistanceUpdateTick = 0;

// ���Ի�����
char debug_buffer[128];

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
void Setup_SDIO_Interrupts(void);

// WiFi��غ���
void InitializeWiFi(void);
void UpdateWiFiInfoDisplay(void);
void RestoreWiFiCommunication(void);

// MQTT��غ���
void InitializeMQTT(void);
void CheckMQTTStatus(void);
void ProcessMQTTCommands(void);
uint8_t SendImageViaMQTT(const char* filename);

// ��ʾ�ͽ��溯��
void ShowBootAnimation(void);
void DisplayParameterScreen(void);
void UpdateDistanceDisplay(void);
void DisplayDebugInfo(const char* info);
void ProcessButton(void);
void Display_Message(uint16_t y, const char* msg, uint16_t color);
void Fill_Rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void DisplayCameraImage(uint16_t *camera_buf, uint16_t width, uint16_t height);
void UpdateProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress, uint16_t color);

// ģʽ��غ���
void ParameterDisplayModeUpdate(void);
void CameraModeUpdate(void);

// ��ť��غ���
void Button_Init(void);
uint8_t Is_Button_Pressed(void);
uint8_t Is_Camera_Button_Pressed(void);

// ������������
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
  * @brief �����ģʽ�л��ز�����ʾģʽʱ���ô˺����ָ�WiFi����
  */
void RestoreWiFiCommunication(void) {
    uint8_t retry;
    
    // ��ʾ״̬
    ST7789_FillRectangle(10, 270, 220, 10, BLACK);
    ST7789_WriteString(10, 270, "Checking ESP8266...", YELLOW, BLACK, 1);
    
    // ����ESP8266
    ESP8266_Restart();
    HAL_Delay(500);
    
    // ��ʼ��ESP8266
    if (!ESP8266_Init(&huart1)) {
        ST7789_FillRectangle(10, 270, 220, 10, BLACK);
        ST7789_WriteString(10, 270, "ESP8266 not responding!", RED, BLACK, 1);
        
        // ��������ESP8266��ͨ��
        // Ӳ��������Ҫ�����ϵ�
        wifiConnected = 0;
        return;
    }
    
    // ����WiFi����״̬
    wifiConnected = 0;
    
    // ��ʾ����״̬
    ST7789_FillRectangle(10, 270, 220, 10, BLACK);
    ST7789_WriteString(10, 270, "Checking WiFi connection...", YELLOW, BLACK, 1);
    
    // ���Լ������״̬
    for(retry = 0; retry < 3; retry++) {
        wifiConnected = ESP8266_CheckConnection();
        if(wifiConnected) {
            ST7789_FillRectangle(10, 270, 220, 10, BLACK);
            ST7789_WriteString(10, 270, "WiFi already connected", GREEN, BLACK, 1);
            break;
        }
        HAL_Delay(300);
    }
    
    // ����������ʧ�ܣ�������������
    if(!wifiConnected) {
        // ��ʾ����״̬
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
    
    // ����ɹ����ӣ���ȡWiFi��Ϣ
    if(wifiConnected) {
        // ���ü�ʱ������������WiFi��Ϣ
        lastWifiRssiUpdateTick = 0;
        lastWifiIpUpdateTick = 0;
        
        // ��ȡ�ź�ǿ�Ⱥ�IP��ַ
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
        
        // ����WiFi��Ϣ��ʾ
        UpdateWiFiInfoDisplay();
        
        // ��ʼ��MQTT����
        ST7789_FillRectangle(10, 270, 220, 10, BLACK);
        ST7789_WriteString(10, 270, "Initializing MQTT...", YELLOW, BLACK, 1);
        InitializeMQTT();
        
        // ���Դ��������Ƿ�����
        Beep(2);
        HAL_Delay(200);
        Vibrate(VIBRATOR_1, 200);
    } else {
        // ����ʧ�ܴ���
        strcpy((char*)ipAddress, "WiFi Failed");
        rssiValue = 0;
        mqtt_connected = 0;
        
        ST7789_FillRectangle(10, 270, 220, 10, BLACK);
        ST7789_WriteString(10, 270, "WiFi reconnect failed", RED, BLACK, 1);
        
        // ��ʾ���Ӳ����ʾ
        Beep(1);
        HAL_Delay(200);
        Beep(1);
    }
}

/**
  * @brief  ������Ļ�ϵ�WiFi��Ϣ
  */
void UpdateWiFiInfoDisplay(void) {
  if (!wifiConnected || currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;
  
  // �����ź�ǿ��
  ST7789_FillRectangle(10, 175, 220, 20, BLACK);
  char msgBuffer[128];
  sprintf(msgBuffer, "Signal: %d dBm", rssiValue);
  ST7789_WriteString(10, 175, msgBuffer, YELLOW, BLACK, 1);
  
  // ����IP��ַ
  ST7789_FillRectangle(10, 195, 220, 20, BLACK);
  ST7789_WriteString(10, 195, "IP: ", CYAN, BLACK, 1);
  ST7789_WriteString(40, 195, (const char*)ipAddress, GREEN, BLACK, 1);
  
  // ���MQTT״̬��ʾ
  ST7789_FillRectangle(10, 215, 220, 20, BLACK);
  sprintf(msgBuffer, "MQTT: %s", mqtt_connected ? "Connected" : "Disconnected");
  ST7789_WriteString(10, 215, msgBuffer, mqtt_connected ? GREEN : RED, BLACK, 1);
}

/**
  * @brief  ��ʼ��WiFi
  */
void InitializeWiFi(void) {
  ST7789_WriteString(10, 130, "Init WiFi...", CYAN, BLACK, 1);
  
  // ��ʼ��ESP8266
  uint8_t init_status = ESP8266_Init(&huart1);
  
  if (init_status) {
    ST7789_WriteString(200, 130, "OK", GREEN, BLACK, 1);
    
    // ��������WiFi
    ST7789_WriteString(10, 150, "Connecting to WiFi...", CYAN, BLACK, 1);
    
    // ��������WiFi (���ӵȴ�������)
    HAL_Delay(1000);
    wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
    
    if (wifiConnected) {
      ST7789_WriteString(200, 150, "OK", GREEN, BLACK, 1);
      ST7789_WriteString(10, 170, "Network: ", CYAN, BLACK, 1);
      ST7789_WriteString(60, 170, WIFI_SSID, WHITE, BLACK, 1);
      
      // ��ȡIP��ַ - �������Դ���
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
      
      // ��ȡ�ź�ǿ�� - �������Դ���
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
      
      // ��ʼ��MQTT����
      ST7789_WriteString(10, 230, "Init MQTT...", CYAN, BLACK, 1);
      InitializeMQTT();
      
      Beep(3);
    } else {
      ST7789_WriteString(200, 150, "FAIL", RED, BLACK, 1);
      
      // ��������
      ST7789_WriteString(10, 170, "Retrying...", YELLOW, BLACK, 1);
      
      // ���ӵȴ�ʱ������Դ���
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
          
          // ��ȡIP���ź�ǿ��
          ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress));
          rssiValue = ESP8266_GetRSSI();
          
          // ��ʼ��MQTT����
          ST7789_WriteString(10, 230, "Init MQTT...", CYAN, BLACK, 1);
          InitializeMQTT();
          
          break;
        }
      }
      
      if (!wifiConnected) {
        ST7789_WriteString(10, 190, "Failed - Hardware issue?", RED, BLACK, 1);
        BeepContinuous(500);
        
        // ��ʾESP8266Ӳ�������ʾ
        ST7789_WriteString(10, 210, "Check ESP8266 power/wiring", RED, BLACK, 1);
      }
    }
  } else {
    // ESP8266ͨ��ʧ��
    ST7789_WriteString(200, 130, "FAIL", RED, BLACK, 1);
    ST7789_WriteString(10, 150, "ESP8266 not responding", RED, BLACK, 1);
    ST7789_WriteString(10, 170, "Check power & wiring", RED, BLACK, 1);
    
    // ����ǿ������
    ST7789_WriteString(10, 190, "Trying restart...", YELLOW, BLACK, 1);
    ESP8266_Restart();
    
    HAL_Delay(2000); // ��ESP8266������ʱ��
    
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
  * @brief  ��ʼ��MQTT����
  */
void InitializeMQTT(void)
{
    // ���ESP8266�Ƿ��ѳ�ʼ�������ӵ�WiFi
    if (!wifiConnected) {
        DisplayDebugInfo("WiFi not connected, can't init MQTT");
        mqtt_connected = 0;
        return;
    }
    
    DisplayDebugInfo("Connecting to MQTT broker...");
    
    // ��Ӵ��ڵ������
    sprintf(debug_buffer, "MAIN: Initializing MQTT\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
    HAL_Delay(20);
    
    // ��ʼ��MQTT����
    if (Integration_Init()) {
        mqtt_connected = 1;
        DisplayDebugInfo("MQTT broker connected");
        ST7789_WriteString(200, 230, "OK", GREEN, BLACK, 1);
        
        // ��Ӵ��ڵ������
        sprintf(debug_buffer, "MAIN: MQTT connected successfully!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
        HAL_Delay(20);
        
        // ����ϵͳ״̬��Ϣ
        Integration_PublishStatus("system_ready");
    } else {
        mqtt_connected = 0;
        DisplayDebugInfo("MQTT connection failed");
        ST7789_WriteString(200, 230, "FAIL", RED, BLACK, 1);
        
        // ��Ӵ��ڵ������
        sprintf(debug_buffer, "MAIN: MQTT connection failed\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
        HAL_Delay(20);
        
        // ��������
        HAL_Delay(2000);
        DisplayDebugInfo("Retrying MQTT...");
        if (Integration_Init()) {
            mqtt_connected = 1;
            ST7789_WriteString(200, 230, "OK", GREEN, BLACK, 1);
            
            // ��Ӵ��ڵ������
            sprintf(debug_buffer, "MAIN: MQTT connected on retry!\r\n");
            HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
            HAL_Delay(20);
            
            // ����ϵͳ״̬��Ϣ
            Integration_PublishStatus("system_ready");
        } else {
            ST7789_WriteString(200, 230, "FAIL", RED, BLACK, 1);
            
            // ��Ӵ��ڵ������
            sprintf(debug_buffer, "MAIN: MQTT connection failed after retry\r\n");
            HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
            HAL_Delay(20);
        }
    }
}

/**
  * @brief  ���MQTT״̬����������
  */
void CheckMQTTStatus(void)
{
    if (mqtt_connected) {
        // ���MQTT�����Ƿ���Ȼ��Ծ
        if (!MQTT_Check()) {
            // ���ӶϿ�����������
            DisplayDebugInfo("MQTT disconnected. Reconnecting...");
            mqtt_connected = 0;
            
            // ���³�ʼ��MQTT
            if (Integration_Init()) {
                mqtt_connected = 1;
                DisplayDebugInfo("MQTT reconnected");
            } else {
                DisplayDebugInfo("MQTT reconnect failed");
            }
        } else {
            // ������ܵĴ�����Ϣ
            Integration_ProcessCommands();
        }
    } else if (wifiConnected) {
        // WiFi�����ӵ�MQTTδ���ӣ���������MQTT
        static uint32_t last_mqtt_reconnect = 0;
        if (HAL_GetTick() - last_mqtt_reconnect > 30000) { // ÿ30�볢��һ��
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
  * @brief  ����MQTT����
  */
void ProcessMQTTCommands(void)
{
    if (command_received) {
        command_received = 0;
        
        // ���������Ӵ����ض�����Ĵ���
        // ���磺����command_buffer��ִ����Ӧ�Ĳ���
        
        // ��ʱʾ������Ӧ���յ�������
        char response[128];
        sprintf(response, "{\"status\":\"command_received\",\"command\":\"%s\"}", command_buffer);
        MQTT_Publish(MQTT_TOPIC_STATUS, response);
        
        // ����������
        memset(command_buffer, 0, sizeof(command_buffer));
    }
}

/**
  * @brief  ͨ��MQTT����ͼ���ļ�
  * @param  filename: ͼ���ļ���
  * @retval 1: �ɹ�, 0: ʧ��
  */
uint8_t SendImageViaMQTT(const char* filename)
{
    // ���MQTT����״̬
    if (!mqtt_connected) {
        DisplayDebugInfo("MQTT not connected. Reconnecting...");
        mqtt_connected = 0;
        
        // ������������MQTT
        if (!Integration_Init()) {
            DisplayDebugInfo("MQTT reconnect failed");
            return 0;
        }
        DisplayDebugInfo("MQTT reconnected");
        mqtt_connected = 1;
    }
    
    // ��ʾ������Ϣ
    ST7789_FillScreen(BLACK);
    ST7789_WriteString(10, 10, "MQTT Image Transfer", WHITE, BLACK, 2);
    ST7789_WriteString(10, 40, "Sending image to server...", YELLOW, BLACK, 1);
    
    // ����ͼ��
    DisplayDebugInfo("Sending image via MQTT...");
    
    // �ý�������ʾ����
    UpdateProgressBar(20, 80, 200, 20, 0, CYAN);
    
    // ���ô����־
    mqtt_image_transfer_in_progress = 1;
    
    // ʵ�ʷ���ͼ��
    uint8_t result = Integration_SendImage(filename);
    
    if (result) {
        // ���½�����
        UpdateProgressBar(20, 80, 200, 20, 100, GREEN);
        ST7789_WriteString(10, 120, "Image sent successfully!", GREEN, BLACK, 1);
        
        // �ȴ��������
        ST7789_WriteString(10, 150, "Waiting for response...", YELLOW, BLACK, 1);
        
        char result_buffer[256] = {0};
        uint8_t result_received = Integration_WaitForResult(result_buffer, sizeof(result_buffer), 10000);
        
        if (result_received) {
            ST7789_WriteString(10, 180, "Response received:", GREEN, BLACK, 1);
            ST7789_WriteString(10, 210, result_buffer, WHITE, BLACK, 1);
        } else {
            ST7789_WriteString(10, 180, "No response from server", YELLOW, BLACK, 1);
        }
        
        // ���ͳɹ���Ч
        Beep(3);
        Vibrate(VIBRATOR_1, 200);
    } else {
        // ���½�����Ϊʧ��״̬
        UpdateProgressBar(20, 80, 200, 20, 100, RED);
        ST7789_WriteString(10, 120, "Failed to send image!", RED, BLACK, 1);
        
        // ������Ч
        Beep(1);
        HAL_Delay(200);
        Beep(1);
    }
    
    // ��������־
    mqtt_image_transfer_in_progress = 0;
    mqtt_image_transfer_requested = 0;
    
    // �ȴ���������
    ST7789_WriteString(10, 240, "Press any button to return", YELLOW, BLACK, 1);
    
    uint32_t start_wait = HAL_GetTick();
    uint8_t button_pressed = 0;
    while (!button_pressed && HAL_GetTick() - start_wait < 10000) {
        if (Is_Button_Pressed() || Is_Camera_Button_Pressed()) {
            button_pressed = 1;
        }
        HAL_Delay(50);
    }
    
    // �������ģʽ��������ʾ���ͼ��
    if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
        DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
        ST7789_WriteString(10, 10, "Camera Mode", WHITE, BLACK, 2);
        ST7789_WriteString(10, PIC_HEIGHT + 10, "Press PA0 to capture & send", YELLOW, BLACK, 1);
    }
    
    return result;
}

/**
  * @brief  USART1��ʼ��
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
  
  // ����USART1ʱ��
  __HAL_RCC_USART1_CLK_ENABLE();
  
  // ����GPIOAʱ��
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  // ����USART1����
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;  // PA9=TX, PA10=RX
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // ��ʼ��UART
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  ��ʾ��Ϣ
  */
void Display_Message(uint16_t y, const char* msg, uint16_t color) {
    ST7789_WriteString(0, y, msg, color, BLACK, 1);
}

/**
  * @brief  ����������
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
  * @brief  ��ʾ����ͷͼ��
  */
void DisplayCameraImage(uint16_t *camera_buf, uint16_t width, uint16_t height) {
    uint16_t x, y;
    
    // �����ػ���ͼ��
    for(y = 0; y < height; y++) {
        for(x = 0; x < width; x++) {
            ST7789_DrawPixel(x, y, camera_buf[y * width + x]);
        }
    }
}

/**
  * @brief  ���½�����
  */
void UpdateProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress, uint16_t color) {
    // ���ԭ�н�����
    Fill_Rectangle(x, y, width, height, BLACK);
    
    // ���㵱ǰ����
    uint16_t current_width = (width * progress) / 100;
    
    // ���ƽ�����
    Fill_Rectangle(x, y, current_width, height, color);
    
    // ��ʾ�ٷֱ�
    sprintf(debug_buffer, "%d%%", progress);
    ST7789_WriteString(x + width + 5, y, debug_buffer, WHITE, BLACK, 1);
}

/**
  * @brief  ��ʾ����������ִ�г�ʼ��
  * @return ��ʼ��״̬ (0: �ɹ�, ��0: ʧ��)
  */
uint8_t ShowBootAnimationAndInit(void) {
  uint8_t init_status = 0;
  char debug_buffer[64];
  
  // ��ʾ������Ļ
  ST7789_FillScreen(BLACK);
  ST7789_WriteString(10, 80, "Camera & MQTT System", WHITE, BLACK, 2);
  ST7789_WriteString(60, 110, "System v1.0", CYAN, BLACK, 2);
  
  // ���ƽ��������
  uint16_t barWidth = 200;
  uint16_t barHeight = 20;
  uint16_t barX = (240 - barWidth) / 2;
  uint16_t barY = 160;
  
  ST7789_DrawRectangle(barX, barY, barWidth, barHeight, WHITE);
  
  // ״̬��ʾ����
  uint16_t statusY = barY + barHeight + 10;
  uint16_t detailY = statusY + 20;
  
  // ��ʼ������
  uint8_t progress = 0;
  
  // 0-10%: ϵͳ������ʼ��
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "System Initializing...", CYAN, BLACK, 1);
  
  // �ⲿ������main������ʼ���: HAL_Init, SystemClock_Config, delay_init
  for (progress = 0; progress <= 10; progress += 2) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(50);
  }
  
  // 10-25%: ��ʼ��GPIO�Ͱ�ť
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing I/O...", CYAN, BLACK, 1);
  
  MX_GPIO_Init();
  Button_Init();
  Haptic_Init(); // ��ʼ����������
  
  Beep(1); // ����ʾ
  
  for (; progress <= 25; progress += 3) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(30);
  }
  
  // 25-40%: ��ʼ��SD��
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing SD Card...", CYAN, BLACK, 1);
  
  sd_init_status = BSP_SD_Init();
  sprintf(debug_buffer, "SD Card: %s", sd_init_status == MSD_OK ? "OK" : "FAIL");
  ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, detailY, debug_buffer, sd_init_status == MSD_OK ? GREEN : RED, BLACK, 1);
  
  if (sd_init_status == MSD_OK) {
    // �����ж����ȼ�
    Setup_SDIO_Interrupts();
    
    // ��ȡSD����Ϣ
    HAL_SD_CardInfoTypeDef cardInfo;
    BSP_SD_GetCardInfo(&cardInfo);
    
    // �����ļ�ϵͳ
    fatfs_init_status = FATFS_Init();
    if (fatfs_init_status == 0) {
      ImageSave_Init();
      ImageSave_SetMode(SAVE_MODE_FATFS);
      // ȷ�����ڱ�Ҫ��Ŀ¼
      EnsureSDCardDirectory();
    } else if (fatfs_init_status == 101) {
      // ��Ҫ��ʽ��
      ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
      ST7789_WriteString(barX, detailY, "Formatting SD Card...", YELLOW, BLACK, 1);
      fatfs_init_status = FATFS_Format();
      if (fatfs_init_status == 0) {
        ImageSave_Init();
        ImageSave_SetMode(SAVE_MODE_FATFS);
        ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
        ST7789_WriteString(barX, detailY, "Format successful", GREEN, BLACK, 1);
        // ������Ҫ��Ŀ¼
        EnsureSDCardDirectory();
      }
    }
  } else {
    init_status |= 0x01; // SD�������־
  }
  
  for (; progress <= 40; progress += 3) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(30);
  }
  
  // 40-60%: ��ʼ��UART��WiFi
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing WiFi...", CYAN, BLACK, 1);
  
  MX_USART1_UART_Init();
  
  for (; progress <= 50; progress += 2) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(20);
  }
  
  // ��ʼ��ESP8266
  uint8_t esp8266_status = ESP8266_Init(&huart1);
  if (esp8266_status) {
    ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY, "ESP8266 OK", GREEN, BLACK, 1);
  } else {
    ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY, "ESP8266 Failed", RED, BLACK, 1);
    init_status |= 0x02; // WiFi�����־
  }
  
  // ����WiFi
  ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, detailY, "Connecting to network...", WHITE, BLACK, 1);
  
  wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
  if (!wifiConnected) {
    // ����һ��
    wifiConnected = ESP8266_ConnectToAP(WIFI_SSID, WIFI_PASSWORD);
  }
  
  if (wifiConnected) {
    ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress));
    rssiValue = ESP8266_GetRSSI();
    
    ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
    ST7789_WriteString(barX, detailY, "WiFi Connected", GREEN, BLACK, 1);
    
    // ��ʼ��MQTT����
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
    init_status |= 0x02; // WiFi�����־
  }
  
  for (; progress <= 60; progress += 2) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(20);
  }
  
  // 60-80%: ��ʼ��������
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing Sensors...", CYAN, BLACK, 1);
  
  // ��ʼ��������������
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
  
  // 80-100%: ��ʼ�����
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, statusY, "Initializing Camera...", CYAN, BLACK, 1);
  
  uint8_t camera_init_result = OV7670_Init();
  sprintf(debug_buffer, "Camera: %s", camera_init_result == 0 ? "OK" : "FAIL");
  ST7789_FillRectangle(barX, detailY, barWidth, 20, BLACK);
  ST7789_WriteString(barX, detailY, debug_buffer, camera_init_result == 0 ? GREEN : RED, BLACK, 1);
  
  if (camera_init_result != 0) {
    // �򵥵��������
    camera_init_result = OV7670_Init();
    if (camera_init_result != 0) {
      init_status |= 0x04; // ��������־
    }
  }
  
  for (; progress <= 100; progress += 4) {
    ST7789_FillRectangle(barX + 2, barY + 2, (barWidth - 4) * progress / 100, barHeight - 4, CYAN);
    HAL_Delay(30);
  }
  
  // �ܽ��ʼ��״̬
  ST7789_FillRectangle(barX, statusY, barWidth, 20, BLACK);
  
  if (init_status == 0) {
    ST7789_WriteString(barX, statusY, "System Ready!", GREEN, BLACK, 1);
    Beep(3); // �ɹ���ʾ��
  } else {
    sprintf(debug_buffer, "Ready with warnings (%02X)", init_status);
    ST7789_WriteString(barX, statusY, debug_buffer, YELLOW, BLACK, 1);
    Beep(2); // ������ʾ��
  }
  
  HAL_Delay(1000);
  return init_status;
}

/**
  * @brief  ��ʾ������Ļ
  */
void DisplayParameterScreen(void) {
    if (currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;
    
    ST7789_FillScreen(BLACK);
    ST7789_WriteString(10, 10, "Camera & MQTT System", WHITE, BLACK, 2);
    ST7789_WriteString(10, 50, "WiFi: ", WHITE, BLACK, 2);
    ST7789_WriteString(90, 50, wifiConnected ? "Connected" : "Not Connected", 
                     wifiConnected ? GREEN : RED, BLACK, 2);
    
    // �̶�����Ԫ��
    ST7789_WriteString(10, 90, "Front:", WHITE, BLACK, 2);
    ST7789_WriteString(10, 120, "Side:", WHITE, BLACK, 2);
    
    // SD��״̬
    ST7789_WriteString(10, 150, "SD Card: Ready", GREEN, BLACK, 1);
    
    // WiFi��Ϣ����
    if (wifiConnected) {
        ST7789_WriteString(10, 175, "Signal: Updating...", YELLOW, BLACK, 1);
        ST7789_WriteString(10, 195, "IP: Updating...", CYAN, BLACK, 1);
        
        // ���MQTT״̬��Ϣ
        if (mqtt_connected) {
            ST7789_WriteString(10, 215, "MQTT: Connected", GREEN, BLACK, 1);
        } else {
            ST7789_WriteString(10, 215, "MQTT: Disconnected", RED, BLACK, 1);
        }
    }
    
    // ���״̬
    ST7789_WriteString(10, 235, "Camera: Ready", GREEN, BLACK, 1);
    
    // ����ʹ��˵����ָʾ��Ҫ����
    ST7789_WriteString(10, 255, "Long press mode btn (PA15) for camera", WHITE, BLACK, 1);
    
    // ������Ϣ����
    ST7789_WriteString(10, 280, "System Ready", GREEN, BLACK, 1);
}

/**
  * @brief  ������Ļ�ϵľ�����ʾ
  */
void UpdateDistanceDisplay(void) {
  if (currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;
                            
  // ����ǰ������
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
  
  // ���²������
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
  * @brief  ��ʾ������Ϣ
  */
void DisplayDebugInfo(const char* info) {
  ST7789_FillRectangle(10, 270, 220, 10, BLACK);
  ST7789_WriteString(10, 270, info, YELLOW, BLACK, 1);
  
  // ���ڵ������
  sprintf(debug_buffer, "DEBUG: %s\r\n", info);
  HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
  HAL_Delay(20); // ���Ӷ��ӳ�ȷ���������
}

/**
  * @brief  ��ť��ʼ��
  */
void Button_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // ����GPIOAʱ��
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // ����PA15Ϊ����ģʽ(ģʽ�л���ť)
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    // ����PA0Ϊ����ģʽ(���հ�ť)
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief  ���PA15��ť�Ƿ��£����ǰ�������
  */
uint8_t Is_Button_Pressed(void) {
    static uint8_t last_state = 1;    // ������һ�ΰ���״̬
    static uint32_t debounce_time = 0; // ����ʱ��
    
    uint8_t current_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15);
    
    // ����״̬�����仯���Ѵﵽ�������
    if (current_state != last_state && HAL_GetTick() - debounce_time > 100) {
        debounce_time = HAL_GetTick();
        last_state = current_state;
        
        // �����⵽���������£�����1
        if (current_state == GPIO_PIN_RESET) {
            return 1;
        }
    }
    
    return 0;
}

/**
  * @brief  ���PA0��ť�Ƿ��£����ǰ�������
  */
uint8_t Is_Camera_Button_Pressed(void) {
    static uint8_t last_state = 1;    // ������һ�ΰ���״̬
    static uint32_t debounce_time = 0; // ����ʱ��
    
    uint8_t current_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    
    // ����״̬�����仯���Ѵﵽ�������
    if (current_state != last_state && HAL_GetTick() - debounce_time > 100) {
        debounce_time = HAL_GetTick();
        last_state = current_state;
        
        // �����⵽���������£�����1
        if (current_state == GPIO_PIN_RESET) {
            return 1;
        }
    }
    
    return 0;
}

/**
  * @brief  ��ť������������ʵ��
  */
void ProcessButton(void) {
    static uint8_t lastModeButtonReading = GPIO_PIN_SET;    // �ϴζ�ȡPA15��״̬
    static uint8_t modeButtonState = GPIO_PIN_SET;          // ��ǰ�������PA15״̬
    static uint32_t lastModeButtonDebounceTime = 0;         // PA15������ʱ��
    
    static uint8_t lastCameraButtonReading = GPIO_PIN_SET;  // �ϴζ�ȡPA0��״̬
    static uint8_t cameraButtonState = GPIO_PIN_SET;        // ��ǰ�������PA0״̬
    static uint32_t lastCameraButtonDebounceTime = 0;       // PA0������ʱ��
    
    // �������ֹ�Զ�ģʽ�л��ı�����
    if (systemJustStarted) {
        if (HAL_GetTick() > 3000) { // ����3�������ð�ť
            systemJustStarted = 0;
            DisplayDebugInfo("Buttons Enabled");
        } else {
            return; // ���������ڼ�����а�ť�¼�
        }
    }
    
    // ��ֹ����ģʽ�л����߼�
    if (currentSystemState == SYSTEM_STATE_CAMERA_MODE && autoModeChangeProtectionTimer == 0) {
        // �ս������ģʽ������������ʱ��
        autoModeChangeProtectionTimer = HAL_GetTick() + 1000; // 1�뱣����
    }
    
    if (autoModeChangeProtectionTimer > 0 && HAL_GetTick() < autoModeChangeProtectionTimer) {
        // �ڱ������ڣ��������κΰ�ť�¼�
        return;
    }
    
    // ---- ����ģʽ�л���ť (PA15) ----
    uint8_t modeReading = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15);
    
    // ���ԭʼ��ť״̬�ĵ������
    if (modeReading != lastModeButtonReading) {
        char dbgMsg[32];
        sprintf(dbgMsg, "Mode Button Raw: %s", modeReading == GPIO_PIN_RESET ? "DOWN" : "UP");
        DisplayDebugInfo(dbgMsg);
        lastModeButtonReading = modeReading;
        lastModeButtonDebounceTime = HAL_GetTick(); // ����������ʱ��
    }
    
    // �����ӳٴ���
    if ((HAL_GetTick() - lastModeButtonDebounceTime) > BUTTON_DEBOUNCE_TIME) {
        // �����ȡ�ȶ����뵱ǰ״̬��ͬ�������
        if (modeReading != modeButtonState) {
            modeButtonState = modeReading;
            
            // ��ť�����¼� (�͵�ƽ)
            if (modeButtonState == GPIO_PIN_RESET && !buttonPressed) {
                buttonPressed = 1;
                buttonPressTick = HAL_GetTick();
                buttonLongPressDetected = 0;
                
                // ��ʾ��ť����״̬
                DisplayDebugInfo("Mode Button: DOWN");
            }
            // ��ť�ͷ��¼� (�ߵ�ƽ)
            else if (modeButtonState == GPIO_PIN_SET && buttonPressed) {
                // ����̰�(���û�м�⵽����)
                if (!buttonLongPressDetected) {
                    // PA15�̰����ڲ����κβ���
                    DisplayDebugInfo("Mode Button: SHORT PRESS (no action)");
                }
                
                buttonPressed = 0;
                
                // ��ʾ��ţ̌��״̬
                DisplayDebugInfo("Mode Button: UP");
            }
        }
    }
    
    // PA15�������
    if (buttonPressed && !buttonLongPressDetected && (HAL_GetTick() - buttonPressTick > BUTTON_LONGPRESS_TIME)) {
        buttonLongPressDetected = 1;
        
        // ��ʾ�������
        DisplayDebugInfo("Mode Button: LONG PRESS");
        
        // ������ʾģʽ�³����л������ģʽ
        if (currentSystemState == SYSTEM_STATE_PARAM_DISPLAY) {
            // ��ʾ�л���Ϣ
            DisplayDebugInfo("Long Press - Entering Camera");
            
            // �ӳ�һ��ʱ�����л��Ա�����
            HAL_Delay(200);
            
            currentSystemState = SYSTEM_STATE_CAMERA_MODE;
            
            // ����������ʱ��
            autoModeChangeProtectionTimer = HAL_GetTick() + 1000;
            
            // ������׼���������ģʽ
            ST7789_FillScreen(BLACK);
            ST7789_WriteString(10, 10, "Camera Mode", WHITE, BLACK, 2);
            ST7789_WriteString(10, 40, "Mode button: hold to exit", YELLOW, BLACK, 1);
            ST7789_WriteString(10, 60, "Camera button: press to capture", YELLOW, BLACK, 1);
        }
        // ���ģʽ�³����л���������ʾģʽ
        else if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
            // ��ʾ�л���Ϣ
            DisplayDebugInfo("Long Press - Exiting Camera");
            
            // �ӳ�һ��ʱ�����л�
            HAL_Delay(200);
            
            // �ȸ���״̬
            currentSystemState = SYSTEM_STATE_PARAM_DISPLAY;
            
            // ȡ��������������
            capture_requested = 0;
            
            // ������ǿ��WiFiͨ�Żָ�����
            RestoreWiFiCommunication();
            
            // ��ʾ������Ļ
            DisplayParameterScreen();
            
            // ���ü�ʱ����ǿ����������
            lastDistanceUpdateTick = 0;
            lastWifiRssiUpdateTick = 0;
            
            // ��ȡһ�ξ�����ȷ����ʾ��ȷ
            frontDistance = HC_SR04_ReadDistance(HC_SR04_FRONT);
            sideDistance = HC_SR04_ReadDistance(HC_SR04_SIDE);
            
            // ������ʾ
            UpdateDistanceDisplay();
            
            // ȷ��ϵͳ�ȶ�
            HAL_Delay(100);
        }
    }
    
    // ---- �������հ�ť (PA0) ----
    // ֻ�����ģʽ�´������հ�ť
    if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
        uint8_t cameraReading = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
        
        // ������հ�ť״̬�ĵ������
        if (cameraReading != lastCameraButtonReading) {
            char dbgMsg[32];
            sprintf(dbgMsg, "Camera Button Raw: %s", cameraReading == GPIO_PIN_RESET ? "DOWN" : "UP");
            DisplayDebugInfo(dbgMsg);
            lastCameraButtonReading = cameraReading;
            lastCameraButtonDebounceTime = HAL_GetTick(); // ����������ʱ��
        }
        
        // �����ӳٴ���
        if ((HAL_GetTick() - lastCameraButtonDebounceTime) > BUTTON_DEBOUNCE_TIME) {
            // �����ȡ�ȶ����뵱ǰ״̬��ͬ�������
            if (cameraReading != cameraButtonState) {
                cameraButtonState = cameraReading;
                
                // ��ť�����¼� (�͵�ƽ)
                if (cameraButtonState == GPIO_PIN_RESET && !cameraButtonPressed) {
                    cameraButtonPressed = 1;
                    cameraButtonPressTick = HAL_GetTick();
                    
                    // ��ʾ��ť����״̬
                    DisplayDebugInfo("Camera Button: DOWN");
                }
                // ��ť�ͷ��¼� (�ߵ�ƽ)
                else if (cameraButtonState == GPIO_PIN_SET && cameraButtonPressed) {
                    // ���հ�ť�ͷ�ʱ����
                    capture_requested = 1;
                    DisplayDebugInfo("Camera Button pressed - Photo requested");
                    Buzzer_Beep_Short(); // ��ť������ʾ��
                    
                    cameraButtonPressed = 0;
                    
                    // ��ʾ��ţ̌��״̬
                    DisplayDebugInfo("Camera Button: UP");
                }
            }
        }
    }
}

/**
  * @brief  ���²�����ʾģʽ
  */
void ParameterDisplayModeUpdate(void) {
  if (currentSystemState != SYSTEM_STATE_PARAM_DISPLAY) return;

  // ��ȡ���봫����
  frontDistance = HC_SR04_ReadDistance(HC_SR04_FRONT);
  sideDistance = HC_SR04_ReadDistance(HC_SR04_SIDE);
  
  // ÿ200ms���¾�����ʾ
  if (HAL_GetTick() - lastDistanceUpdateTick >= 200 || lastDistanceUpdateTick == 0) {
    lastDistanceUpdateTick = HAL_GetTick();
    UpdateDistanceDisplay();
  }
  
  // ÿ5�����WiFi��Ϣ���ź�ǿ�ȣ�
  if (wifiConnected && (HAL_GetTick() - lastWifiRssiUpdateTick >= 5000 || lastWifiRssiUpdateTick == 0)) {
    lastWifiRssiUpdateTick = HAL_GetTick();
    rssiValue = ESP8266_GetRSSI();
    UpdateWiFiInfoDisplay();
  }
  
  // ÿ15�����IP��ַ
  if (wifiConnected && (HAL_GetTick() - lastWifiIpUpdateTick >= 15000 || lastWifiIpUpdateTick == 0)) {
    lastWifiIpUpdateTick = HAL_GetTick();
    ESP8266_GetIP((char*)ipAddress, sizeof(ipAddress));
    UpdateWiFiInfoDisplay();
  }
  
  // ���MQTT״̬����������
  if (wifiConnected) {
    CheckMQTTStatus();
    ProcessMQTTCommands();
  }
}

/**
  * @brief  �������ģʽ
  */
void CameraModeUpdate(void) {
    if (currentSystemState != SYSTEM_STATE_CAMERA_MODE) return;
  
    // �ж��Ƿ���ͼ����״̬
    static uint8_t showing_mqtt_transfer = 0;
    static uint32_t mqtt_transfer_start_time = 0;
    
    // �������֡�����Ҳ��ڴ��������
    if (ov_rev_ok && !showing_mqtt_transfer) {
        // ��ʾ֡������Ϣ
        Display_Message(DEBUG_START_Y + 60, "Processing new frame...", YELLOW);
        
        // ��ʾͼ��
        DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
        frame_counter++;
        
        // ��ʾ������ʾ
        ST7789_WriteString(10, 10, "Camera Mode", WHITE, BLACK, 2);
        ST7789_WriteString(10, PIC_HEIGHT + 10, "Press PA0 to capture & send", YELLOW, BLACK, 1);
        
        // �����Ҫ������SD����ʼ���ɹ�
        if (capture_requested && sd_init_status == MSD_OK && fatfs_init_status == 0) {
            // ��¼��ʼʱ��
            photo_save_start_time = HAL_GetTick();
            
            // ��ʾ����״̬
            Display_Message(DEBUG_START_Y + 80, "Saving image to SD card...", YELLOW);
            Fill_Rectangle(20, 60, 200, 80, BLUE); // ��ɫ����
            ST7789_WriteString(40, 70, "SAVING PHOTO...", WHITE, BLUE, 1);
            
            // ʹ��ImageSaveϵͳ����ͼ��
            if (ImageSave_IsIdle()) {
                // ����ͼ�񱣴�
                uint8_t start_result = ImageSave_StartCapture(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
                sprintf(debug_buffer, "Start save: %d", start_result);
                Display_Message(DEBUG_START_Y + 90, debug_buffer, start_result ? GREEN : RED);
                
                // ���������
                uint32_t process_start = HAL_GetTick();
                uint32_t process_count = 0;
                uint32_t last_progress = 0;
                uint8_t progress_bar_width = 160; // ���������
                
                // ��д���������ʾ������
                ST7789_WriteString(40, 90, "Progress: ", WHITE, BLUE, 1);
                Fill_Rectangle(110, 90, progress_bar_width, 10, BLACK); // ����������
                
                // ��ʾʹ�õı���ģʽ
                sprintf(debug_buffer, "Mode: %s", (save_mode == SAVE_MODE_FATFS) ? "FatFs" : "Direct (DMA)");
                ST7789_WriteString(40, 110, debug_buffer, WHITE, BLUE, 1);
                
                // ����״̬����ѭ��
                uint32_t last_debug_time = HAL_GetTick();
                
                while (!ImageSave_IsIdle() && HAL_GetTick() - process_start < 30000) {
                    // ����һ���������
                    ImageSave_Process();
                    process_count++;
                    
                    // ��ȡ��ǰ״̬
                    SaveState_t current_state = ImageSave_GetState();
                    uint8_t error_code = ImageSave_GetError();
                    
                    // ��ȡ����ʾ��ǰ����
                    uint8_t current_progress = ImageSave_GetProgress();
                    
                    // ���½�����
                    if (current_progress != last_progress) {
                        last_progress = current_progress;
                        // ���ƽ�����
                        uint16_t bar_width = (progress_bar_width * current_progress) / 100;
                        Fill_Rectangle(110, 90, bar_width, 10, GREEN); // ����ɲ���
                        
                        // ��ʾ���Ȱٷֱ�
                        sprintf(debug_buffer, "%d%%", current_progress);
                        ST7789_WriteString(110 + progress_bar_width + 5, 90, debug_buffer, WHITE, BLUE, 1);
                    }
                    
                    // ÿ50ms����һ�ε�����Ϣ
                    if (HAL_GetTick() - last_debug_time > 50) {
                        // ��ʾ������Ϣ
                        const char* debug_str = ImageSave_GetDebugInfo();
                        sprintf(debug_buffer, "Debug: %s", debug_str);
                        Display_Message(DEBUG_START_Y + 100, debug_buffer, CYAN);
                        
                        // ��ʾ״̬�ʹ�����
                        sprintf(debug_buffer, "State: %d Err: %d", current_state, error_code);
                        Display_Message(DEBUG_START_Y + 130, debug_buffer, YELLOW);
                        
                        // ��ʾ����״̬
                        Display_Save_Status();
                        
                        // ����ʱ���
                        last_debug_time = HAL_GetTick();
                    }
                    
                    // �������������û�ȡ��
                    ProcessButton();
                    
                    HAL_Delay(5); // ������ʱ������CPU����
                }
                
                // ��鱣����
                if (ImageSave_GetError() == SAVE_ERROR_NONE && ImageSave_IsIdle()) {
                    // ����ɹ�
                    uint32_t save_time = HAL_GetTick() - photo_save_start_time;
                    
                    // ��ȡ��ǰ�ļ�����
                    current_file_index = ImageSave_GetFileIndex();
                    
                    // ��ʾ�ɹ���Ϣ
                    Fill_Rectangle(20, 60, 200, 80, GREEN); // ��ɫ����
                    ST7789_WriteString(30, 70, "PHOTO SAVED!", BLACK, GREEN, 2);
                    sprintf(debug_buffer, "Image #%u", (unsigned int)current_file_index);
                    ST7789_WriteString(45, 100, debug_buffer, BLACK, GREEN, 1);
                    sprintf(debug_buffer, "Time: %ums", (unsigned int)save_time);
                    ST7789_WriteString(45, 120, debug_buffer, BLACK, GREEN, 1);
                    
                    // ������ΪIMAGE.JPG����MQTT����
                    char source_file[32];
                    sprintf(source_file, "0:/IMG_%04u.BMP", (unsigned int)current_file_index);
                    f_rename(source_file, IMAGE_FILENAME);
                    
                    // �ɹ���ʾ��
                    Buzzer_Beep_Short();
                    
                    // ���óɹ���־������������ʾ
                    photo_saved_success = 1;
                    success_display_time = HAL_GetTick();
                    
                    // ���MQTT����״̬��׼������ͼ��
                    if (mqtt_connected) {
                        // ��ʾ����׼��MQTTͼ����
                        Display_Message(DEBUG_START_Y + 140, "Preparing MQTT image transfer...", YELLOW);
                        
                        // ��Ӵ��ڵ������
                        sprintf(debug_buffer, "\r\n[CAMERA_MODE] Photo saved, preparing to send over MQTT\r\n");
                        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
                        
                        // ����MQTT���������־
                        mqtt_image_transfer_requested = 1;
                        mqtt_image_transfer_in_progress = 0;
                        
                        // ���ô���״̬��־
                        showing_mqtt_transfer = 1;
                        mqtt_transfer_start_time = HAL_GetTick();
                    } else {
                        // ��ʾMQTTδ������ʾ
                        Display_Message(DEBUG_START_Y + 140, "MQTT not connected, image saved only", YELLOW);
                        
                        // ��Ӵ��ڵ������
                        sprintf(debug_buffer, "\r\n[CAMERA_MODE] Photo saved, but MQTT not connected\r\n");
                        HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
                    }
                } else {
                    // ����ʧ�ܻ�ʱ
                    Fill_Rectangle(20, 60, 200, 80, RED); // ��ɫ����
                    ST7789_WriteString(30, 70, "SAVE FAILED!", WHITE, RED, 2);
                    sprintf(debug_buffer, "Error: %d", ImageSave_GetError());
                    ST7789_WriteString(45, 100, debug_buffer, WHITE, RED, 1);
                    sprintf(debug_buffer, "Steps: %u", (unsigned int)process_count);
                    ST7789_WriteString(45, 120, debug_buffer, WHITE, RED, 1);
                    
                    // ʧ����ʾ��
                    Buzzer_Beep_Times(3, 200);
                    
                    // ����ʧ�ܺ�ȴ�һ�����Ȼ�����Ԥ��
                    HAL_Delay(3000);
                    DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
                }
            } else {
                // ϵͳæ�����ܱ���
                Fill_Rectangle(20, 60, 200, 40, RED);
                ST7789_WriteString(40, 70, "SYSTEM BUSY", WHITE, RED, 1);
                Buzzer_Beep_Short();
            }
            
            // ���������־
            capture_requested = 0;
        }
        
        // ֡�������
        Display_Message(DEBUG_START_Y + 60, "Frame processed!", GREEN);
        
        // ���ñ�־
        ov_rev_ok = 0;
        
        // ��������DCMI������һ֡
        DCMI_Start();
    }
    
    // ����MQTTͼ�����߼�
    if (showing_mqtt_transfer) {
        // ����ӳ�����ʱ��
        if (HAL_GetTick() - mqtt_transfer_start_time > 500) {
            // ���ô����־
            showing_mqtt_transfer = 0;
            
            if (mqtt_image_transfer_requested && !mqtt_image_transfer_in_progress) {
                // ���ô�������б�־
                mqtt_image_transfer_in_progress = 1;
                
                // ��ʼMQTTͼ����
                SendImageViaMQTT(IMAGE_FILENAME);
                
                // �����־
                mqtt_image_transfer_requested = 0;
                mqtt_image_transfer_in_progress = 0;
            }
            
            // ������ɺ�ָ�Ԥ��
            HAL_Delay(1000); // ������ʾ
            DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
        }
    }
}

/**
  * @brief  ���DMA״̬
  */
void Check_DMA_Status(void) {
    // ��ȡDMA״̬�Ĵ���
    uint32_t dma_isr = DMA2->LISR; // ����״̬��Ϣ�Ĵ���������Stream 0-3
    
    // Stream 1��־��־
    uint8_t teif = (dma_isr & DMA_LISR_TEIF1) ? 1 : 0; // ��������־
    uint8_t htif = (dma_isr & DMA_LISR_HTIF1) ? 1 : 0; // �봫��
    uint8_t tcif = (dma_isr & DMA_LISR_TCIF1) ? 1 : 0; // �������
    uint8_t feif = (dma_isr & DMA_LISR_FEIF1) ? 1 : 0; // FIFO����
    
    // ��ʾDMA״̬
    sprintf(debug_buffer, "DMA: E:%d H:%d C:%d F:%d", teif, htif, tcif, feif);
    Display_Message(DEBUG_START_Y + 40, debug_buffer, YELLOW);
}

/**
  * @brief  ��ʾDCMI״̬
  */
void Display_DCMI_Status(void) {
    uint32_t dcmi_cr = hdcmi.Instance->CR;  // ���ƼĴ���
    uint32_t dcmi_sr = hdcmi.Instance->SR;  // ״̬�Ĵ���
    
    // ��ʾDCMI״̬
    sprintf(debug_buffer, "DCMI CR:0x%02X SR:0x%02X", 
            (unsigned int)(dcmi_cr & 0xFF), 
            (unsigned int)(dcmi_sr & 0xFF));
    Display_Message(DEBUG_START_Y + 20, debug_buffer, CYAN);
}

/**
  * @brief  ��ʾ����Ԥ��
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
  * @brief  ��ʾSD���洢״̬
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
  * @brief  ����SDIO�ж����ȼ�
  */
void Setup_SDIO_Interrupts(void) {
    // ����SDIO��DMA�ж����ȼ�
    HAL_NVIC_SetPriority(SDIO_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SDIO_IRQn);
    
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
    
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
}

/**
  * @brief  ȷ��SD���ϵ�Ŀ¼����
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
  * @brief  GPIO��ʼ��
  */
void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  // ʹ��GPIO�˿�ʱ��
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  // ��ʼ����Ҫ���������
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // LED����
  
  // ����LED�������
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  // ��ʼ����ť
  Button_Init();
}

/**
  * @brief  ����ڵ�
  * @retval int
  */
int main(void)
{
  // ϵͳ��ʼ��
  HAL_Init();
  SystemClock_Config();
  
  // ��ʼ����ʱ
  delay_init();
  
  // ��ʼ��LCD
  ST7789_Init();
  ST7789_FillScreen(BLACK);
  
  // ��ʾ����������ִ�����г�ʼ��
  uint8_t init_status = ShowBootAnimationAndInit();
  
  // ������DCMIͼ��ɼ�ǰ����ʾ������Ļ
  DisplayParameterScreen();
  
  // ����DCMIͼ��ɼ�
  DCMI_Start();
  
  // ��ѭ��
  uint32_t last_update = 0;
  uint32_t loop_counter = 0;
  uint32_t mqtt_status_check_time = 0;
  
  // ���ó�ʼģʽ
  currentSystemState = SYSTEM_STATE_PARAM_DISPLAY;
  
  // ����һ�ξ��봫������������֤����ҳ����������ʾ
  frontDistance = HC_SR04_ReadDistance(HC_SR04_FRONT);
  sideDistance = HC_SR04_ReadDistance(HC_SR04_SIDE);
  UpdateDistanceDisplay();
  
  // ������Ϣ
  sprintf(debug_buffer, "System ready (%02X)", init_status);
  DisplayDebugInfo(debug_buffer);
  
  // ��Ӵ��ڵ������
  sprintf(debug_buffer, "\r\n[MAIN] System initialized and ready\r\n");
  HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
  
  // MQTT״̬��Ϣ
  if (mqtt_connected) {
      sprintf(debug_buffer, "[MAIN] MQTT is connected to broker\r\n");
      HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
      
      // ����ϵͳ����״̬
      Integration_PublishStatus("system_ready");
  } else {
      sprintf(debug_buffer, "[MAIN] MQTT is not connected\r\n");
      HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
  }
  
  while(1) {
    // ������ѭ������
    loop_counter++;
    
    // ����ť�¼�
    ProcessButton();
    
    // ���ڵ�ǰ״̬����
    if (currentSystemState == SYSTEM_STATE_PARAM_DISPLAY) {
      ParameterDisplayModeUpdate();
      
      // �ڲ�����ʾģʽ�£����ڼ��MQTT״̬�ʹ�������
      if (HAL_GetTick() - mqtt_status_check_time > 5000) { // ÿ5����һ��
        mqtt_status_check_time = HAL_GetTick();
        
        if (wifiConnected) {
          CheckMQTTStatus();
          ProcessMQTTCommands();
        }
      }
    } else if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
      CameraModeUpdate();
    }
    
    // ÿ500ms����һ��״̬��Ϣ
    if(HAL_GetTick() - last_update > 500) {
      last_update = HAL_GetTick();
      
      if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
        // �����ģʽ����ʾ֡����
        sprintf(debug_buffer, "Frames: %u  Loops: %u", 
                (unsigned int)frame_counter, 
                (unsigned int)loop_counter);
        Display_Message(DEBUG_START_Y, debug_buffer, WHITE);
        
        // ��ʾOK��־��datanum
        sprintf(debug_buffer, "OK: %d  DataNum: %u", 
                ov_rev_ok, (unsigned int)datanum);
        Display_Message(DEBUG_START_Y + 10, debug_buffer, GREEN);
        
        // ����DCMI��DMA״̬
        Display_DCMI_Status();
        Check_DMA_Status();
        
        // ��ʾ��������
        Display_Pixel_Preview();
        
        // ���DCMI�Ƿ���������
        uint8_t dcmi_running = (hdcmi.Instance->CR & DCMI_CR_CAPTURE) != 0;
        if(!dcmi_running && !ov_rev_ok) {
          // DCMIû�����У���������
          Display_Message(DEBUG_START_Y + 70, "DCMI not running, restarting...", RED);
          DCMI_Start();
          Display_Message(DEBUG_START_Y + 70, "DCMI restarted", GREEN);
          Buzzer_Beep_Short(); // ������ʾ��
        }
      }
    }
    
    // �ɹ�������ʾ3���������ʾ�򲢻ָ�ͼ��
    if (photo_saved_success && (HAL_GetTick() - success_display_time > 3000)) {
      photo_saved_success = 0;
      // ������ʾ����ͷ����
      DisplayCameraImage(camera_buffer, PIC_WIDTH, PIC_HEIGHT);
      Buzzer_Beep_Short(); // ��ʾ��
    }
    
    HAL_Delay(1); // ����ʱ����ѭ������
  }
}

/**
  * @brief  ��������
  */
void Error_Handler(void)
{
  __disable_irq();
  
  ST7789_FillScreen(RED);
  ST7789_WriteString(10, 120, "SYSTEM ERROR!", WHITE, RED, 2);
  
  Buzzer_Beep_Times(5, 100); // ������ʾ��
  
  HAL_Delay(3000);
  
  // ϵͳ��λ
  NVIC_SystemReset();
  
  while (1)
  {
  } 
}

// BSPд����ɻص�
void BSP_SD_WriteCpltCallback(void)
{
    SD_DMA_TxComplete();
}

// BSP�жϻص�
void BSP_SD_AbortCallback(void)
{
    SD_DMA_TxError();
}

// SD��DMA�����жϴ���
void BSP_SD_DMA_Tx_IRQHandler(void)
{
    extern SD_HandleTypeDef uSdHandle;
    HAL_DMA_IRQHandler(uSdHandle.hdmatx);
}

// SD��SDIO�жϴ���
void BSP_SD_IRQHandler(void)
{
    extern SD_HandleTypeDef uSdHandle;
    HAL_SD_IRQHandler(&uSdHandle);
}

// MQTT������ջص�����
void MQTT_CommandCallback(const char* command)
{
    // ������浽ȫ�ֻ�����
    strncpy(command_buffer, command, sizeof(command_buffer)-1);
    command_buffer[sizeof(command_buffer)-1] = '\0';
    
    // ����������ձ�־
    command_received = 1;
    
    // ��Ӵ��ڵ������
    sprintf(debug_buffer, "[MQTT] Command received: %s\r\n", command);
    HAL_UART_Transmit(&huart1, (uint8_t*)debug_buffer, strlen(debug_buffer), 300);
    
    // �����������ݽ��в�ͬ�Ĳ���
    if (strstr(command, "capture") != NULL) {
        // ��������������ҵ�ǰ�����ģʽ�����������������־
        if (currentSystemState == SYSTEM_STATE_CAMERA_MODE) {
            capture_requested = 1;
            // ����ȷ����Ϣ
            Integration_PublishStatus("capture_requested");
        } else {
            // ����������ģʽ�����ʹ�����Ϣ
            Integration_PublishStatus("error_not_in_camera_mode");
        }
    } else if (strstr(command, "status") != NULL) {
        // �����״̬��ѯ������͵�ǰϵͳ״̬
        char status_data[256];
        sprintf(status_data, "{\"mode\":\"%s\",\"wifi\":%d,\"mqtt\":%d,\"sd_card\":%d}", 
                currentSystemState == SYSTEM_STATE_CAMERA_MODE ? "camera" : "parameter",
                wifiConnected, mqtt_connected, sd_init_status == MSD_OK ? 1 : 0);
        MQTT_Publish(MQTT_TOPIC_STATUS, status_data);
    }
}

/**
  * @brief  ϵͳʱ������
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
  RCC_OscInitStruct.PLL.PLLQ = 7; // ��ƵΪ7�õ�48MHzʱ������SDIO
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
  
  /* ����MCO1���HSIʱ����Ϊ�����XCLK */
  HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_4);
}