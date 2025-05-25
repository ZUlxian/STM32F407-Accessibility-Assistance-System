#ifndef __IMAGE_SAVE_H
#define __IMAGE_SAVE_H

#include "stm32f4xx_hal.h"
#include "sdio_sd.h"

// 图像保存状态
typedef enum {
    SAVE_IDLE,                // 空闲状态
    SAVE_WAITING_IMAGE,       // 等待图像捕获完成
    SAVE_PREPARING_FILE,      // 准备文件
    SAVE_WRITING_HEADER,      // 写入文件头(BMP格式)
    SAVE_WRITING_DATA,        // 写入图像数据
    SAVE_FINISHING,           // 完成保存
    SAVE_ERROR                // 错误状态
} SaveState_t;

// 错误代码定义
#define SAVE_ERROR_NONE       0  // 无错误
#define SAVE_ERROR_SD_WRITE   1  // SD卡写入错误
#define SAVE_ERROR_SD_READ    2  // SD卡读取错误
#define SAVE_ERROR_TIMEOUT    3  // 操作超时
#define SAVE_ERROR_INIT       4  // 初始化错误

// 保存参数结构体
typedef struct {
    SaveState_t state;        // 当前状态
    uint32_t file_index;      // 文件索引号(用于文件名)
    uint32_t current_sector;  // 当前写入扇区
    uint32_t total_sectors;   // 总扇区数
    uint16_t width;           // 图像宽度
    uint16_t height;          // 图像高度
    uint8_t *header_buffer;   // 文件头缓冲区
    uint16_t *image_buffer;   // 图像数据指针
    uint32_t write_offset;    // 当前写入偏移
    uint8_t error_code;       // 错误代码
    uint8_t operation_complete; // 操作完成标志
    uint32_t operation_start_time; // 操作开始时间(用于超时检测)
    uint32_t timeout_ms;      // 超时时间(毫秒)
} ImageSave_t;

// 保存模式定义
#define SAVE_MODE_FATFS 0     // 使用FatFs库保存(标准方式)
#define SAVE_MODE_DIRECT 1    // 使用直接扇区写入方式(备用方式)

// 函数声明
void ImageSave_Init(void);
void ImageSave_SetMode(uint8_t mode); // 新增：设置保存模式
uint8_t ImageSave_StartCapture(uint16_t *image_data, uint16_t width, uint16_t height);
void ImageSave_Process(void);
uint8_t ImageSave_IsIdle(void);
uint8_t ImageSave_GetError(void);
uint32_t ImageSave_GetFileIndex(void);
SaveState_t ImageSave_GetState(void);
uint8_t ImageSave_GetProgress(void);
const char* ImageSave_GetDebugInfo(void);

// DMA回调函数
void SD_DMA_TxComplete(void);
void SD_DMA_TxError(void);

// 访问SD句柄
extern SD_HandleTypeDef uSdHandle;

#endif /* __IMAGE_SAVE_H */