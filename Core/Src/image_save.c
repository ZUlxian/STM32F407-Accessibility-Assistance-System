/**
  ******************************************************************************
  * @file    image_save.c
  * @brief   图像保存功能实现
  ******************************************************************************
  */

#include "image_save.h"
#include <string.h>
#include <stdio.h>
#include "ov7670.h"
#include "ff.h"          
#include "fatfs.h"       

// BMP文件头大小
#define BMP_HEADER_SIZE 54

// 全局变量
ImageSave_t save_ctx;
static uint8_t bmp_header[BMP_HEADER_SIZE];

// 定义缓冲区 - 避免使用ALIGN_32BYTES宏
static uint8_t write_buffer[512] __attribute__((aligned(32)));
static uint8_t write_buffer2[512] __attribute__((aligned(32))); // 双缓存

// 保存小批量像素的缓冲区
static uint8_t rgb_buffer[3*64] __attribute__((aligned(32)));

// 文件存储参数
#define SD_MAX_FILES        1000    // 最大保存文件数
#define SD_TIMEOUT_MS       30000   // SD卡操作超时时间(毫秒)
#define PIXELS_PER_CHUNK    64      // 每次处理的像素数 (增加到64)
#define SD_WAIT_TIMEOUT     2000    // DMA传输等待超时(毫秒)

// DMA传输状态
volatile uint8_t dma_transfer_complete = 0;
volatile uint8_t dma_transfer_error = 0;
volatile uint8_t current_buffer = 0;  // 0=write_buffer, 1=write_buffer2

// 调试信息
static char debug_info[64] = "No operation yet";
static uint8_t last_state = SAVE_IDLE;

// 文件操作全局变量
static FIL save_file;
static uint8_t file_opened = 0;
static uint32_t current_pixel = 0;
static uint32_t total_pixels = 0;

// 保存方式标志
static uint8_t save_mode = SAVE_MODE_FATFS;  // 默认使用文件系统模式

// 直接扇区写入参数
#define SD_START_SECTOR  100   // 起始扇区
static uint32_t current_sector = 0;
static uint32_t sector_offset = 0;

// DMA完成回调函数 - 会在main.c中的HAL_SD_TxCpltCallback中调用
void SD_DMA_TxComplete(void) {
    dma_transfer_complete = 1;
    dma_transfer_error = 0;
}

// DMA错误回调函数 - 会在main.c中的HAL_SD_ErrorCallback中调用
void SD_DMA_TxError(void) {
    dma_transfer_complete = 1;
    dma_transfer_error = 1;
}

// 初始化图像保存系统
void ImageSave_Init(void)
{
    memset(&save_ctx, 0, sizeof(ImageSave_t));
    save_ctx.state = SAVE_IDLE;
    save_ctx.header_buffer = bmp_header;
    save_ctx.timeout_ms = SD_TIMEOUT_MS;
    
    // 重置状态变量
    file_opened = 0;
    current_pixel = 0;
    total_pixels = 0;
    dma_transfer_complete = 0;
    dma_transfer_error = 0;
    current_buffer = 0;
    
    // 默认使用文件系统模式
    save_mode = SAVE_MODE_FATFS;
    
    // 确保SD卡可用
    uint8_t retry = 0;
    while (BSP_SD_GetCardState() != SD_TRANSFER_OK && retry < 10) {
        HAL_Delay(10);  // 等待SD卡就绪
        retry++;
    }
    
    strcpy(debug_info, "Init completed");
}

// 设置保存模式 (SAVE_MODE_FATFS 或 SAVE_MODE_DIRECT)
void ImageSave_SetMode(uint8_t mode)
{
    save_mode = mode;
    sprintf(debug_info, "Save mode set to %d", mode);
}

// 准备BMP文件头
static uint8_t PrepareBMPHeader(uint16_t width, uint16_t height)
{
    uint32_t file_size = BMP_HEADER_SIZE + width * height * 3; // RGB888格式
    
    // BMP文件头 (14字节)
    bmp_header[0] = 'B';
    bmp_header[1] = 'M';
    // 文件大小
    bmp_header[2] = (uint8_t)(file_size);
    bmp_header[3] = (uint8_t)(file_size >> 8);
    bmp_header[4] = (uint8_t)(file_size >> 16);
    bmp_header[5] = (uint8_t)(file_size >> 24);
    // 保留
    bmp_header[6] = 0;
    bmp_header[7] = 0;
    bmp_header[8] = 0;
    bmp_header[9] = 0;
    // 数据偏移
    bmp_header[10] = BMP_HEADER_SIZE;
    bmp_header[11] = 0;
    bmp_header[12] = 0;
    bmp_header[13] = 0;
    
    // BMP信息头 (40字节)
    bmp_header[14] = 40;  // 信息头大小
    bmp_header[15] = 0;
    bmp_header[16] = 0;
    bmp_header[17] = 0;
    // 图像宽度
    bmp_header[18] = (uint8_t)(width);
    bmp_header[19] = (uint8_t)(width >> 8);
    bmp_header[20] = (uint8_t)(width >> 16);
    bmp_header[21] = (uint8_t)(width >> 24);
    // 图像高度
    bmp_header[22] = (uint8_t)(height);
    bmp_header[23] = (uint8_t)(height >> 8);
    bmp_header[24] = (uint8_t)(height >> 16);
    bmp_header[25] = (uint8_t)(height >> 24);
    // 平面数
    bmp_header[26] = 1;
    bmp_header[27] = 0;
    // 每像素位数 (24位)
    bmp_header[28] = 24;
    bmp_header[29] = 0;
    // 压缩类型 (0=不压缩)
    bmp_header[30] = 0;
    bmp_header[31] = 0;
    bmp_header[32] = 0;
    bmp_header[33] = 0;
    // 图像数据大小
    uint32_t image_size = width * height * 3;
    bmp_header[34] = (uint8_t)(image_size);
    bmp_header[35] = (uint8_t)(image_size >> 8);
    bmp_header[36] = (uint8_t)(image_size >> 16);
    bmp_header[37] = (uint8_t)(image_size >> 24);
    // 水平/垂直分辨率 (像素/米)
    bmp_header[38] = 0;
    bmp_header[39] = 0;
    bmp_header[40] = 0;
    bmp_header[41] = 0;
    bmp_header[42] = 0;
    bmp_header[43] = 0;
    bmp_header[44] = 0;
    bmp_header[45] = 0;
    // 颜色表中的颜色数
    bmp_header[46] = 0;
    bmp_header[47] = 0;
    bmp_header[48] = 0;
    bmp_header[49] = 0;
    // 重要颜色数
    bmp_header[50] = 0;
    bmp_header[51] = 0;
    bmp_header[52] = 0;
    bmp_header[53] = 0;
    
    return 0;
}

// RGB565转RGB888 - 使用快速简化算法
static void RGB565ToRGB888(uint16_t rgb565, uint8_t *rgb888)
{
    // 提取RGB565中的RGB分量
    uint8_t r = (rgb565 >> 11) & 0x1F;
    uint8_t g = (rgb565 >> 5) & 0x3F;
    uint8_t b = rgb565 & 0x1F;
    
    // 转换到RGB888 (简单扩展)
    rgb888[0] = b << 3;  // 蓝色
    rgb888[1] = g << 2;  // 绿色
    rgb888[2] = r << 3;  // 红色
}

// 使用DMA写入扇区，带等待和超时
static uint8_t SD_WriteSector_DMA(uint32_t sector, uint8_t *buffer)
{
    uint32_t start_time;
    uint8_t status = MSD_OK;
    static uint8_t retry_count = 0;
    
    // 确保SD卡就绪 - 增加重试次数和等待时间
    for (uint8_t retry = 0; retry < 10; retry++) {
        if (BSP_SD_GetCardState() == SD_TRANSFER_OK) break;
        HAL_Delay(20);
        sprintf(debug_info, "SD busy retry: %d", retry);
    }
    
    // 如果SD卡仍未就绪，使用阻塞方式写入
    if (BSP_SD_GetCardState() != SD_TRANSFER_OK) {
        sprintf(debug_info, "SD busy, using blocking write");
        return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
    }
    
    // 重置DMA传输标志
    dma_transfer_complete = 0;
    dma_transfer_error = 0;
    
    // 开始DMA传输
    status = BSP_SD_WriteBlocks_DMA((uint32_t*)buffer, sector, 1);
    if (status != MSD_OK) {
        sprintf(debug_info, "DMA tx start failed: %d", status);
        retry_count++;
        
        // 当失败次数过多时，使用普通阻塞方式
        if (retry_count > 3) {
            sprintf(debug_info, "Too many DMA failures, using normal write");
            retry_count = 0;
            return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
        }
        return MSD_ERROR;
    }
    
    // 等待DMA传输完成，带超时
    start_time = HAL_GetTick();
    while (!dma_transfer_complete) {
        if (HAL_GetTick() - start_time > SD_WAIT_TIMEOUT) {
            sprintf(debug_info, "DMA tx timeout");
            
            // 尝试中止当前传输
            HAL_SD_Abort(&uSdHandle);
            
            // 当失败次数过多时，使用普通阻塞方式
            retry_count++;
            if (retry_count > 3) {
                sprintf(debug_info, "Too many DMA timeouts, using normal write");
                retry_count = 0;
                return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
            }
            return MSD_ERROR;
        }
        HAL_Delay(5);  // 增加等待时间，减少CPU负担
    }
    
    // 检查是否有错误
    if (dma_transfer_error) {
        sprintf(debug_info, "DMA tx error, trying normal write");
        return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
    }
    
    retry_count = 0; // 成功后重置计数器
    return MSD_OK;
}

// 开始图像捕获和保存
uint8_t ImageSave_StartCapture(uint16_t *image_data, uint16_t width, uint16_t height)
{
    // 检查能否开始新捕获
    if (save_ctx.state != SAVE_IDLE) {
        strcpy(debug_info, "Cannot start - not idle");
        return 0;
    }
    
    // 重置状态和计数器
    file_opened = 0;
    current_pixel = 0;
    total_pixels = 0;
    dma_transfer_complete = 0;
    dma_transfer_error = 0;
    current_buffer = 0;
    
    // 更新状态和参数
    save_ctx.state = SAVE_PREPARING_FILE;
    save_ctx.file_index++;
    if (save_ctx.file_index >= SD_MAX_FILES)
        save_ctx.file_index = 1;
    
    save_ctx.image_buffer = image_data;
    save_ctx.width = width;
    save_ctx.height = height;
    total_pixels = width * height;
    
    save_ctx.operation_complete = 0;
    save_ctx.error_code = SAVE_ERROR_NONE;
    save_ctx.operation_start_time = HAL_GetTick();
    
    // 准备BMP文件头
    PrepareBMPHeader(width, height);
    
    // 计算起始扇区
    if (save_mode == SAVE_MODE_DIRECT) {
        current_sector = SD_START_SECTOR + (save_ctx.file_index * 300); // 扩大到300个扇区/文件
        sector_offset = 0;
    }
    
    sprintf(debug_info, "Capture started, mode %d", save_mode);
    return 1; // 成功启动
}

// 图像保存处理函数 - 分步处理
void ImageSave_Process(void)
{
    FRESULT res;
    UINT bytesWritten;
    char filename[32];
    uint8_t status;
    uint32_t i;
    uint8_t *active_buffer;
    
    // 检查状态变化
    if (save_ctx.state != last_state) {
        sprintf(debug_info, "State: %d->%d", last_state, save_ctx.state);
        last_state = save_ctx.state;
    }
    
    // 全局超时检查
    uint32_t elapsed_time = HAL_GetTick() - save_ctx.operation_start_time;
    if (save_ctx.state != SAVE_IDLE && 
        save_ctx.state != SAVE_ERROR &&
        elapsed_time > save_ctx.timeout_ms) {
        
        save_ctx.error_code = SAVE_ERROR_TIMEOUT;
        save_ctx.state = SAVE_ERROR;
        sprintf(debug_info, "Timeout at %u/%u px", current_pixel, total_pixels);
        
        if (file_opened) {
            f_close(&save_file);
            file_opened = 0;
        }
        return;
    }
    
    // 状态机处理
    switch (save_ctx.state)
    {
        case SAVE_IDLE:
            // 空闲状态无需处理
            break;
            
        case SAVE_PREPARING_FILE:
            // 根据保存模式进行不同操作
            if (save_mode == SAVE_MODE_FATFS) {
                // === 文件系统方式 ===
                
                // 生成文件名 - 移除前导斜杠，防止路径格式问题
                sprintf(filename, "IMG_%04u.BMP", save_ctx.file_index);
                sprintf(debug_info, "Opening: %s", filename);
                
                // 尝试打开文件
                res = f_open(&save_file, filename, FA_CREATE_ALWAYS | FA_WRITE);
                if (res != FR_OK) {
                    sprintf(debug_info, "Open err: %d", res);
                    save_ctx.error_code = SAVE_ERROR_INIT;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                // 写入BMP头
                res = f_write(&save_file, save_ctx.header_buffer, BMP_HEADER_SIZE, &bytesWritten);
                if (res != FR_OK || bytesWritten != BMP_HEADER_SIZE) {
                    sprintf(debug_info, "Header err: %d,%u", res, bytesWritten);
                    f_close(&save_file);
                    file_opened = 0;
                    save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                // 设置标志和同步
                file_opened = 1;
                f_sync(&save_file); // 添加同步，确保头信息写入
                current_pixel = 0;
            }
            else {
                // === 直接扇区写入方式 ===
                
                // 检查SD卡状态
                if (BSP_SD_GetCardState() != SD_TRANSFER_OK) {
                    // 如果SD卡忙，等待一下再重试
                    sprintf(debug_info, "SD card busy, waiting");
                    HAL_Delay(10);
                    break;  // 保持当前状态，下次再试
                }
                
                // 准备第一个扇区 - 使用write_buffer
                current_buffer = 0;
                active_buffer = write_buffer;
                
                memset(active_buffer, 0, 512);
                memcpy(active_buffer, save_ctx.header_buffer, BMP_HEADER_SIZE);
                sector_offset = BMP_HEADER_SIZE;
                
                // 使用DMA写入第一个扇区
                sprintf(debug_info, "Writing header sector %u", current_sector);
                status = SD_WriteSector_DMA(current_sector, active_buffer);
                if (status != MSD_OK) {
                    sprintf(debug_info, "Header sector err: %d", status);
                    save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                current_sector++;
                current_pixel = 0;
                current_buffer = 0;
                sector_offset = 0;
            }
            
            // 进入数据写入状态
            save_ctx.state = SAVE_WRITING_DATA;
            strcpy(debug_info, "Preparing done");
            break;
            
        case SAVE_WRITING_DATA:
            // 检查是否所有像素已处理
            if (current_pixel >= total_pixels) {
                // 结束处理
                if (save_mode == SAVE_MODE_FATFS && file_opened) {
                    f_close(&save_file);
                    file_opened = 0;
                }
                else if (save_mode == SAVE_MODE_DIRECT && sector_offset > 0) {
                    // 写入最后一个不完整扇区
                    active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
                    
                    sprintf(debug_info, "Writing final sector %u", current_sector);
                    status = SD_WriteSector_DMA(current_sector, active_buffer);
                    if (status != MSD_OK) {
                        // 尝试使用阻塞模式
                        sprintf(debug_info, "Final DMA failed, trying normal write");
                        status = BSP_SD_WriteBlocks((uint32_t*)active_buffer, current_sector, 1, SD_DATATIMEOUT);
                        if (status != MSD_OK) {
                            sprintf(debug_info, "Final sector err: %d", status);
                            save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                            save_ctx.state = SAVE_ERROR;
                            break;
                        }
                    }
                }
                
                // 标记完成
                save_ctx.operation_complete = 1;
                save_ctx.state = SAVE_IDLE;
                sprintf(debug_info, "All %u pixels done", total_pixels);
                break;
            }
            
            // 获取当前活动缓冲区
            active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
            
            // 处理部分像素
            uint32_t pixels_to_process = PIXELS_PER_CHUNK;
            if (current_pixel + pixels_to_process > total_pixels) {
                pixels_to_process = total_pixels - current_pixel;
            }
            
            // 转换像素格式 - 直接顺序保存
            for (i = 0; i < pixels_to_process; i++) {
                uint32_t pixel_index = current_pixel + i;
                if (pixel_index >= total_pixels) break;
                
                // 简化版RGB转换
                RGB565ToRGB888(save_ctx.image_buffer[pixel_index], &rgb_buffer[i*3]);
            }
            
            // 保存处理
            if (save_mode == SAVE_MODE_FATFS) {
                // 文件系统方式
                if (!file_opened) {
                    sprintf(debug_info, "File not open");
                    save_ctx.error_code = SAVE_ERROR_INIT;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                // 写入像素数据
                sprintf(debug_info, "Writing %upx to file", pixels_to_process);
                res = f_write(&save_file, rgb_buffer, pixels_to_process * 3, &bytesWritten);
                if (res != FR_OK || bytesWritten != pixels_to_process * 3) {
                    sprintf(debug_info, "Write err:%d (%u/%u)", res, bytesWritten, pixels_to_process * 3);
                    f_close(&save_file);
                    file_opened = 0;
                    save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                // 定期同步，确保数据写入
                if (current_pixel % 5000 == 0) {
                    res = f_sync(&save_file);
                    if (res != FR_OK) {
                        sprintf(debug_info, "Sync err: %d", res);
                        // 仅记录错误，不中断操作
                    }
                }
            }
            else {
                // 直接扇区写入方式 - 使用DMA和双缓存
                
                // 检查扇区是否已满
                if (sector_offset + pixels_to_process * 3 > 512) {
                    // 复制部分像素到当前缓冲区
                    uint32_t first_part = 512 - sector_offset;
                    uint32_t first_pixels = first_part / 3;
                    
                    memcpy(active_buffer + sector_offset, rgb_buffer, first_part);
                    
                    // 使用DMA写入当前扇区
                    sprintf(debug_info, "DMA write sector %u", current_sector);
                    status = SD_WriteSector_DMA(current_sector, active_buffer);
                    if (status != MSD_OK) {
                        // 尝试使用阻塞模式
                        sprintf(debug_info, "DMA sector err: %d, trying normal", status);
                        status = BSP_SD_WriteBlocks((uint32_t*)active_buffer, current_sector, 1, SD_DATATIMEOUT);
                        if (status != MSD_OK) {
                            sprintf(debug_info, "Sector write failed: %d", status);
                            save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                            save_ctx.state = SAVE_ERROR;
                            break;
                        }
                    }
                    
                    // 切换到另一个缓冲区
                    current_buffer = 1 - current_buffer;
                    active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
                    memset(active_buffer, 0, 512);
                    
                    // 将剩余像素复制到新缓冲区
                    if (first_pixels < pixels_to_process) {
                        memcpy(active_buffer, rgb_buffer + first_part, (pixels_to_process - first_pixels) * 3);
                        sector_offset = (pixels_to_process - first_pixels) * 3;
                    } else {
                        sector_offset = 0;
                    }
                    
                    current_sector++;
                }
                else {
                    // 常规情况：将像素复制到当前缓冲区
                    memcpy(active_buffer + sector_offset, rgb_buffer, pixels_to_process * 3);
                    sector_offset += pixels_to_process * 3;
                    
                    // 如果缓冲区刚好填满，立即写入
                    if (sector_offset == 512) {
                        sprintf(debug_info, "DMA write full sector %u", current_sector);
                        status = SD_WriteSector_DMA(current_sector, active_buffer);
                        if (status != MSD_OK) {
                            // 尝试使用阻塞模式
                            sprintf(debug_info, "DMA full sect err: %d, trying normal", status);
                            status = BSP_SD_WriteBlocks((uint32_t*)active_buffer, current_sector, 1, SD_DATATIMEOUT);
                            if (status != MSD_OK) {
                                sprintf(debug_info, "Full sector write failed: %d", status);
                                save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                                save_ctx.state = SAVE_ERROR;
                                break;
                            }
                        }
                        
                        // 切换缓冲区
                        current_buffer = 1 - current_buffer;
                        active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
                        memset(active_buffer, 0, 512);
                        
                        current_sector++;
                        sector_offset = 0;
                    }
                }
            }
            
            // 更新已处理的像素数
            current_pixel += pixels_to_process;
            sprintf(debug_info, "Processed: %u/%u", current_pixel, total_pixels);
            break;
            
        case SAVE_ERROR:
            // 错误状态处理
            if (save_mode == SAVE_MODE_FATFS && file_opened) {
                f_close(&save_file);
                file_opened = 0;
            }
            
            sprintf(debug_info, "Error %d at %u/%u px", 
                   save_ctx.error_code, current_pixel, total_pixels);
            save_ctx.operation_complete = 0;
            save_ctx.state = SAVE_IDLE;
            break;
            
        default:
            // 未知状态复位
            if (save_mode == SAVE_MODE_FATFS && file_opened) {
                f_close(&save_file);
                file_opened = 0;
            }
            
            sprintf(debug_info, "Unknown state reset");
            save_ctx.state = SAVE_IDLE;
            break;
    }
}

// 检查是否空闲
uint8_t ImageSave_IsIdle(void)
{
    return (save_ctx.state == SAVE_IDLE);
}

// 获取错误代码
uint8_t ImageSave_GetError(void)
{
    return save_ctx.error_code;
}

// 获取当前文件索引
uint32_t ImageSave_GetFileIndex(void)
{
    return save_ctx.file_index;
}

// 获取当前状态
SaveState_t ImageSave_GetState(void)
{
    return save_ctx.state;
}

// 获取保存进度 (百分比，0-100)
uint8_t ImageSave_GetProgress(void)
{
    if (total_pixels == 0) return 0;
    uint8_t progress = (uint8_t)((current_pixel * 100) / total_pixels);
    
    // 确保不返回0，除非真的是0
    if (current_pixel > 0 && progress == 0) {
        progress = 1;
    }
    
    return progress;
}

// 获取调试信息
const char* ImageSave_GetDebugInfo(void)
{
    return debug_info;
}