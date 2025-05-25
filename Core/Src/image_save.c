/**
  ******************************************************************************
  * @file    image_save.c
  * @brief   ͼ�񱣴湦��ʵ��
  ******************************************************************************
  */

#include "image_save.h"
#include <string.h>
#include <stdio.h>
#include "ov7670.h"
#include "ff.h"          
#include "fatfs.h"       

// BMP�ļ�ͷ��С
#define BMP_HEADER_SIZE 54

// ȫ�ֱ���
ImageSave_t save_ctx;
static uint8_t bmp_header[BMP_HEADER_SIZE];

// ���建���� - ����ʹ��ALIGN_32BYTES��
static uint8_t write_buffer[512] __attribute__((aligned(32)));
static uint8_t write_buffer2[512] __attribute__((aligned(32))); // ˫����

// ����С�������صĻ�����
static uint8_t rgb_buffer[3*64] __attribute__((aligned(32)));

// �ļ��洢����
#define SD_MAX_FILES        1000    // ��󱣴��ļ���
#define SD_TIMEOUT_MS       30000   // SD��������ʱʱ��(����)
#define PIXELS_PER_CHUNK    64      // ÿ�δ���������� (���ӵ�64)
#define SD_WAIT_TIMEOUT     2000    // DMA����ȴ���ʱ(����)

// DMA����״̬
volatile uint8_t dma_transfer_complete = 0;
volatile uint8_t dma_transfer_error = 0;
volatile uint8_t current_buffer = 0;  // 0=write_buffer, 1=write_buffer2

// ������Ϣ
static char debug_info[64] = "No operation yet";
static uint8_t last_state = SAVE_IDLE;

// �ļ�����ȫ�ֱ���
static FIL save_file;
static uint8_t file_opened = 0;
static uint32_t current_pixel = 0;
static uint32_t total_pixels = 0;

// ���淽ʽ��־
static uint8_t save_mode = SAVE_MODE_FATFS;  // Ĭ��ʹ���ļ�ϵͳģʽ

// ֱ������д�����
#define SD_START_SECTOR  100   // ��ʼ����
static uint32_t current_sector = 0;
static uint32_t sector_offset = 0;

// DMA��ɻص����� - ����main.c�е�HAL_SD_TxCpltCallback�е���
void SD_DMA_TxComplete(void) {
    dma_transfer_complete = 1;
    dma_transfer_error = 0;
}

// DMA����ص����� - ����main.c�е�HAL_SD_ErrorCallback�е���
void SD_DMA_TxError(void) {
    dma_transfer_complete = 1;
    dma_transfer_error = 1;
}

// ��ʼ��ͼ�񱣴�ϵͳ
void ImageSave_Init(void)
{
    memset(&save_ctx, 0, sizeof(ImageSave_t));
    save_ctx.state = SAVE_IDLE;
    save_ctx.header_buffer = bmp_header;
    save_ctx.timeout_ms = SD_TIMEOUT_MS;
    
    // ����״̬����
    file_opened = 0;
    current_pixel = 0;
    total_pixels = 0;
    dma_transfer_complete = 0;
    dma_transfer_error = 0;
    current_buffer = 0;
    
    // Ĭ��ʹ���ļ�ϵͳģʽ
    save_mode = SAVE_MODE_FATFS;
    
    // ȷ��SD������
    uint8_t retry = 0;
    while (BSP_SD_GetCardState() != SD_TRANSFER_OK && retry < 10) {
        HAL_Delay(10);  // �ȴ�SD������
        retry++;
    }
    
    strcpy(debug_info, "Init completed");
}

// ���ñ���ģʽ (SAVE_MODE_FATFS �� SAVE_MODE_DIRECT)
void ImageSave_SetMode(uint8_t mode)
{
    save_mode = mode;
    sprintf(debug_info, "Save mode set to %d", mode);
}

// ׼��BMP�ļ�ͷ
static uint8_t PrepareBMPHeader(uint16_t width, uint16_t height)
{
    uint32_t file_size = BMP_HEADER_SIZE + width * height * 3; // RGB888��ʽ
    
    // BMP�ļ�ͷ (14�ֽ�)
    bmp_header[0] = 'B';
    bmp_header[1] = 'M';
    // �ļ���С
    bmp_header[2] = (uint8_t)(file_size);
    bmp_header[3] = (uint8_t)(file_size >> 8);
    bmp_header[4] = (uint8_t)(file_size >> 16);
    bmp_header[5] = (uint8_t)(file_size >> 24);
    // ����
    bmp_header[6] = 0;
    bmp_header[7] = 0;
    bmp_header[8] = 0;
    bmp_header[9] = 0;
    // ����ƫ��
    bmp_header[10] = BMP_HEADER_SIZE;
    bmp_header[11] = 0;
    bmp_header[12] = 0;
    bmp_header[13] = 0;
    
    // BMP��Ϣͷ (40�ֽ�)
    bmp_header[14] = 40;  // ��Ϣͷ��С
    bmp_header[15] = 0;
    bmp_header[16] = 0;
    bmp_header[17] = 0;
    // ͼ����
    bmp_header[18] = (uint8_t)(width);
    bmp_header[19] = (uint8_t)(width >> 8);
    bmp_header[20] = (uint8_t)(width >> 16);
    bmp_header[21] = (uint8_t)(width >> 24);
    // ͼ��߶�
    bmp_header[22] = (uint8_t)(height);
    bmp_header[23] = (uint8_t)(height >> 8);
    bmp_header[24] = (uint8_t)(height >> 16);
    bmp_header[25] = (uint8_t)(height >> 24);
    // ƽ����
    bmp_header[26] = 1;
    bmp_header[27] = 0;
    // ÿ����λ�� (24λ)
    bmp_header[28] = 24;
    bmp_header[29] = 0;
    // ѹ������ (0=��ѹ��)
    bmp_header[30] = 0;
    bmp_header[31] = 0;
    bmp_header[32] = 0;
    bmp_header[33] = 0;
    // ͼ�����ݴ�С
    uint32_t image_size = width * height * 3;
    bmp_header[34] = (uint8_t)(image_size);
    bmp_header[35] = (uint8_t)(image_size >> 8);
    bmp_header[36] = (uint8_t)(image_size >> 16);
    bmp_header[37] = (uint8_t)(image_size >> 24);
    // ˮƽ/��ֱ�ֱ��� (����/��)
    bmp_header[38] = 0;
    bmp_header[39] = 0;
    bmp_header[40] = 0;
    bmp_header[41] = 0;
    bmp_header[42] = 0;
    bmp_header[43] = 0;
    bmp_header[44] = 0;
    bmp_header[45] = 0;
    // ��ɫ���е���ɫ��
    bmp_header[46] = 0;
    bmp_header[47] = 0;
    bmp_header[48] = 0;
    bmp_header[49] = 0;
    // ��Ҫ��ɫ��
    bmp_header[50] = 0;
    bmp_header[51] = 0;
    bmp_header[52] = 0;
    bmp_header[53] = 0;
    
    return 0;
}

// RGB565תRGB888 - ʹ�ÿ��ټ��㷨
static void RGB565ToRGB888(uint16_t rgb565, uint8_t *rgb888)
{
    // ��ȡRGB565�е�RGB����
    uint8_t r = (rgb565 >> 11) & 0x1F;
    uint8_t g = (rgb565 >> 5) & 0x3F;
    uint8_t b = rgb565 & 0x1F;
    
    // ת����RGB888 (����չ)
    rgb888[0] = b << 3;  // ��ɫ
    rgb888[1] = g << 2;  // ��ɫ
    rgb888[2] = r << 3;  // ��ɫ
}

// ʹ��DMAд�����������ȴ��ͳ�ʱ
static uint8_t SD_WriteSector_DMA(uint32_t sector, uint8_t *buffer)
{
    uint32_t start_time;
    uint8_t status = MSD_OK;
    static uint8_t retry_count = 0;
    
    // ȷ��SD������ - �������Դ����͵ȴ�ʱ��
    for (uint8_t retry = 0; retry < 10; retry++) {
        if (BSP_SD_GetCardState() == SD_TRANSFER_OK) break;
        HAL_Delay(20);
        sprintf(debug_info, "SD busy retry: %d", retry);
    }
    
    // ���SD����δ������ʹ��������ʽд��
    if (BSP_SD_GetCardState() != SD_TRANSFER_OK) {
        sprintf(debug_info, "SD busy, using blocking write");
        return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
    }
    
    // ����DMA�����־
    dma_transfer_complete = 0;
    dma_transfer_error = 0;
    
    // ��ʼDMA����
    status = BSP_SD_WriteBlocks_DMA((uint32_t*)buffer, sector, 1);
    if (status != MSD_OK) {
        sprintf(debug_info, "DMA tx start failed: %d", status);
        retry_count++;
        
        // ��ʧ�ܴ�������ʱ��ʹ����ͨ������ʽ
        if (retry_count > 3) {
            sprintf(debug_info, "Too many DMA failures, using normal write");
            retry_count = 0;
            return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
        }
        return MSD_ERROR;
    }
    
    // �ȴ�DMA������ɣ�����ʱ
    start_time = HAL_GetTick();
    while (!dma_transfer_complete) {
        if (HAL_GetTick() - start_time > SD_WAIT_TIMEOUT) {
            sprintf(debug_info, "DMA tx timeout");
            
            // ������ֹ��ǰ����
            HAL_SD_Abort(&uSdHandle);
            
            // ��ʧ�ܴ�������ʱ��ʹ����ͨ������ʽ
            retry_count++;
            if (retry_count > 3) {
                sprintf(debug_info, "Too many DMA timeouts, using normal write");
                retry_count = 0;
                return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
            }
            return MSD_ERROR;
        }
        HAL_Delay(5);  // ���ӵȴ�ʱ�䣬����CPU����
    }
    
    // ����Ƿ��д���
    if (dma_transfer_error) {
        sprintf(debug_info, "DMA tx error, trying normal write");
        return BSP_SD_WriteBlocks((uint32_t*)buffer, sector, 1, SD_DATATIMEOUT);
    }
    
    retry_count = 0; // �ɹ������ü�����
    return MSD_OK;
}

// ��ʼͼ�񲶻�ͱ���
uint8_t ImageSave_StartCapture(uint16_t *image_data, uint16_t width, uint16_t height)
{
    // ����ܷ�ʼ�²���
    if (save_ctx.state != SAVE_IDLE) {
        strcpy(debug_info, "Cannot start - not idle");
        return 0;
    }
    
    // ����״̬�ͼ�����
    file_opened = 0;
    current_pixel = 0;
    total_pixels = 0;
    dma_transfer_complete = 0;
    dma_transfer_error = 0;
    current_buffer = 0;
    
    // ����״̬�Ͳ���
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
    
    // ׼��BMP�ļ�ͷ
    PrepareBMPHeader(width, height);
    
    // ������ʼ����
    if (save_mode == SAVE_MODE_DIRECT) {
        current_sector = SD_START_SECTOR + (save_ctx.file_index * 300); // ����300������/�ļ�
        sector_offset = 0;
    }
    
    sprintf(debug_info, "Capture started, mode %d", save_mode);
    return 1; // �ɹ�����
}

// ͼ�񱣴洦���� - �ֲ�����
void ImageSave_Process(void)
{
    FRESULT res;
    UINT bytesWritten;
    char filename[32];
    uint8_t status;
    uint32_t i;
    uint8_t *active_buffer;
    
    // ���״̬�仯
    if (save_ctx.state != last_state) {
        sprintf(debug_info, "State: %d->%d", last_state, save_ctx.state);
        last_state = save_ctx.state;
    }
    
    // ȫ�ֳ�ʱ���
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
    
    // ״̬������
    switch (save_ctx.state)
    {
        case SAVE_IDLE:
            // ����״̬���账��
            break;
            
        case SAVE_PREPARING_FILE:
            // ���ݱ���ģʽ���в�ͬ����
            if (save_mode == SAVE_MODE_FATFS) {
                // === �ļ�ϵͳ��ʽ ===
                
                // �����ļ��� - �Ƴ�ǰ��б�ܣ���ֹ·����ʽ����
                sprintf(filename, "IMG_%04u.BMP", save_ctx.file_index);
                sprintf(debug_info, "Opening: %s", filename);
                
                // ���Դ��ļ�
                res = f_open(&save_file, filename, FA_CREATE_ALWAYS | FA_WRITE);
                if (res != FR_OK) {
                    sprintf(debug_info, "Open err: %d", res);
                    save_ctx.error_code = SAVE_ERROR_INIT;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                // д��BMPͷ
                res = f_write(&save_file, save_ctx.header_buffer, BMP_HEADER_SIZE, &bytesWritten);
                if (res != FR_OK || bytesWritten != BMP_HEADER_SIZE) {
                    sprintf(debug_info, "Header err: %d,%u", res, bytesWritten);
                    f_close(&save_file);
                    file_opened = 0;
                    save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                // ���ñ�־��ͬ��
                file_opened = 1;
                f_sync(&save_file); // ���ͬ����ȷ��ͷ��Ϣд��
                current_pixel = 0;
            }
            else {
                // === ֱ������д�뷽ʽ ===
                
                // ���SD��״̬
                if (BSP_SD_GetCardState() != SD_TRANSFER_OK) {
                    // ���SD��æ���ȴ�һ��������
                    sprintf(debug_info, "SD card busy, waiting");
                    HAL_Delay(10);
                    break;  // ���ֵ�ǰ״̬���´�����
                }
                
                // ׼����һ������ - ʹ��write_buffer
                current_buffer = 0;
                active_buffer = write_buffer;
                
                memset(active_buffer, 0, 512);
                memcpy(active_buffer, save_ctx.header_buffer, BMP_HEADER_SIZE);
                sector_offset = BMP_HEADER_SIZE;
                
                // ʹ��DMAд���һ������
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
            
            // ��������д��״̬
            save_ctx.state = SAVE_WRITING_DATA;
            strcpy(debug_info, "Preparing done");
            break;
            
        case SAVE_WRITING_DATA:
            // ����Ƿ����������Ѵ���
            if (current_pixel >= total_pixels) {
                // ��������
                if (save_mode == SAVE_MODE_FATFS && file_opened) {
                    f_close(&save_file);
                    file_opened = 0;
                }
                else if (save_mode == SAVE_MODE_DIRECT && sector_offset > 0) {
                    // д�����һ������������
                    active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
                    
                    sprintf(debug_info, "Writing final sector %u", current_sector);
                    status = SD_WriteSector_DMA(current_sector, active_buffer);
                    if (status != MSD_OK) {
                        // ����ʹ������ģʽ
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
                
                // ������
                save_ctx.operation_complete = 1;
                save_ctx.state = SAVE_IDLE;
                sprintf(debug_info, "All %u pixels done", total_pixels);
                break;
            }
            
            // ��ȡ��ǰ�������
            active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
            
            // ����������
            uint32_t pixels_to_process = PIXELS_PER_CHUNK;
            if (current_pixel + pixels_to_process > total_pixels) {
                pixels_to_process = total_pixels - current_pixel;
            }
            
            // ת�����ظ�ʽ - ֱ��˳�򱣴�
            for (i = 0; i < pixels_to_process; i++) {
                uint32_t pixel_index = current_pixel + i;
                if (pixel_index >= total_pixels) break;
                
                // �򻯰�RGBת��
                RGB565ToRGB888(save_ctx.image_buffer[pixel_index], &rgb_buffer[i*3]);
            }
            
            // ���洦��
            if (save_mode == SAVE_MODE_FATFS) {
                // �ļ�ϵͳ��ʽ
                if (!file_opened) {
                    sprintf(debug_info, "File not open");
                    save_ctx.error_code = SAVE_ERROR_INIT;
                    save_ctx.state = SAVE_ERROR;
                    break;
                }
                
                // д����������
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
                
                // ����ͬ����ȷ������д��
                if (current_pixel % 5000 == 0) {
                    res = f_sync(&save_file);
                    if (res != FR_OK) {
                        sprintf(debug_info, "Sync err: %d", res);
                        // ����¼���󣬲��жϲ���
                    }
                }
            }
            else {
                // ֱ������д�뷽ʽ - ʹ��DMA��˫����
                
                // ��������Ƿ�����
                if (sector_offset + pixels_to_process * 3 > 512) {
                    // ���Ʋ������ص���ǰ������
                    uint32_t first_part = 512 - sector_offset;
                    uint32_t first_pixels = first_part / 3;
                    
                    memcpy(active_buffer + sector_offset, rgb_buffer, first_part);
                    
                    // ʹ��DMAд�뵱ǰ����
                    sprintf(debug_info, "DMA write sector %u", current_sector);
                    status = SD_WriteSector_DMA(current_sector, active_buffer);
                    if (status != MSD_OK) {
                        // ����ʹ������ģʽ
                        sprintf(debug_info, "DMA sector err: %d, trying normal", status);
                        status = BSP_SD_WriteBlocks((uint32_t*)active_buffer, current_sector, 1, SD_DATATIMEOUT);
                        if (status != MSD_OK) {
                            sprintf(debug_info, "Sector write failed: %d", status);
                            save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                            save_ctx.state = SAVE_ERROR;
                            break;
                        }
                    }
                    
                    // �л�����һ��������
                    current_buffer = 1 - current_buffer;
                    active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
                    memset(active_buffer, 0, 512);
                    
                    // ��ʣ�����ظ��Ƶ��»�����
                    if (first_pixels < pixels_to_process) {
                        memcpy(active_buffer, rgb_buffer + first_part, (pixels_to_process - first_pixels) * 3);
                        sector_offset = (pixels_to_process - first_pixels) * 3;
                    } else {
                        sector_offset = 0;
                    }
                    
                    current_sector++;
                }
                else {
                    // ��������������ظ��Ƶ���ǰ������
                    memcpy(active_buffer + sector_offset, rgb_buffer, pixels_to_process * 3);
                    sector_offset += pixels_to_process * 3;
                    
                    // ����������պ�����������д��
                    if (sector_offset == 512) {
                        sprintf(debug_info, "DMA write full sector %u", current_sector);
                        status = SD_WriteSector_DMA(current_sector, active_buffer);
                        if (status != MSD_OK) {
                            // ����ʹ������ģʽ
                            sprintf(debug_info, "DMA full sect err: %d, trying normal", status);
                            status = BSP_SD_WriteBlocks((uint32_t*)active_buffer, current_sector, 1, SD_DATATIMEOUT);
                            if (status != MSD_OK) {
                                sprintf(debug_info, "Full sector write failed: %d", status);
                                save_ctx.error_code = SAVE_ERROR_SD_WRITE;
                                save_ctx.state = SAVE_ERROR;
                                break;
                            }
                        }
                        
                        // �л�������
                        current_buffer = 1 - current_buffer;
                        active_buffer = (current_buffer == 0) ? write_buffer : write_buffer2;
                        memset(active_buffer, 0, 512);
                        
                        current_sector++;
                        sector_offset = 0;
                    }
                }
            }
            
            // �����Ѵ����������
            current_pixel += pixels_to_process;
            sprintf(debug_info, "Processed: %u/%u", current_pixel, total_pixels);
            break;
            
        case SAVE_ERROR:
            // ����״̬����
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
            // δ֪״̬��λ
            if (save_mode == SAVE_MODE_FATFS && file_opened) {
                f_close(&save_file);
                file_opened = 0;
            }
            
            sprintf(debug_info, "Unknown state reset");
            save_ctx.state = SAVE_IDLE;
            break;
    }
}

// ����Ƿ����
uint8_t ImageSave_IsIdle(void)
{
    return (save_ctx.state == SAVE_IDLE);
}

// ��ȡ�������
uint8_t ImageSave_GetError(void)
{
    return save_ctx.error_code;
}

// ��ȡ��ǰ�ļ�����
uint32_t ImageSave_GetFileIndex(void)
{
    return save_ctx.file_index;
}

// ��ȡ��ǰ״̬
SaveState_t ImageSave_GetState(void)
{
    return save_ctx.state;
}

// ��ȡ������� (�ٷֱȣ�0-100)
uint8_t ImageSave_GetProgress(void)
{
    if (total_pixels == 0) return 0;
    uint8_t progress = (uint8_t)((current_pixel * 100) / total_pixels);
    
    // ȷ��������0�����������0
    if (current_pixel > 0 && progress == 0) {
        progress = 1;
    }
    
    return progress;
}

// ��ȡ������Ϣ
const char* ImageSave_GetDebugInfo(void)
{
    return debug_info;
}