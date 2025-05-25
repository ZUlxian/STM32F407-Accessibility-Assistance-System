#ifndef __IMAGE_SAVE_H
#define __IMAGE_SAVE_H

#include "stm32f4xx_hal.h"
#include "sdio_sd.h"

// ͼ�񱣴�״̬
typedef enum {
    SAVE_IDLE,                // ����״̬
    SAVE_WAITING_IMAGE,       // �ȴ�ͼ�񲶻����
    SAVE_PREPARING_FILE,      // ׼���ļ�
    SAVE_WRITING_HEADER,      // д���ļ�ͷ(BMP��ʽ)
    SAVE_WRITING_DATA,        // д��ͼ������
    SAVE_FINISHING,           // ��ɱ���
    SAVE_ERROR                // ����״̬
} SaveState_t;

// ������붨��
#define SAVE_ERROR_NONE       0  // �޴���
#define SAVE_ERROR_SD_WRITE   1  // SD��д�����
#define SAVE_ERROR_SD_READ    2  // SD����ȡ����
#define SAVE_ERROR_TIMEOUT    3  // ������ʱ
#define SAVE_ERROR_INIT       4  // ��ʼ������

// ��������ṹ��
typedef struct {
    SaveState_t state;        // ��ǰ״̬
    uint32_t file_index;      // �ļ�������(�����ļ���)
    uint32_t current_sector;  // ��ǰд������
    uint32_t total_sectors;   // ��������
    uint16_t width;           // ͼ����
    uint16_t height;          // ͼ��߶�
    uint8_t *header_buffer;   // �ļ�ͷ������
    uint16_t *image_buffer;   // ͼ������ָ��
    uint32_t write_offset;    // ��ǰд��ƫ��
    uint8_t error_code;       // �������
    uint8_t operation_complete; // ������ɱ�־
    uint32_t operation_start_time; // ������ʼʱ��(���ڳ�ʱ���)
    uint32_t timeout_ms;      // ��ʱʱ��(����)
} ImageSave_t;

// ����ģʽ����
#define SAVE_MODE_FATFS 0     // ʹ��FatFs�Ᵽ��(��׼��ʽ)
#define SAVE_MODE_DIRECT 1    // ʹ��ֱ������д�뷽ʽ(���÷�ʽ)

// ��������
void ImageSave_Init(void);
void ImageSave_SetMode(uint8_t mode); // ���������ñ���ģʽ
uint8_t ImageSave_StartCapture(uint16_t *image_data, uint16_t width, uint16_t height);
void ImageSave_Process(void);
uint8_t ImageSave_IsIdle(void);
uint8_t ImageSave_GetError(void);
uint32_t ImageSave_GetFileIndex(void);
SaveState_t ImageSave_GetState(void);
uint8_t ImageSave_GetProgress(void);
const char* ImageSave_GetDebugInfo(void);

// DMA�ص�����
void SD_DMA_TxComplete(void);
void SD_DMA_TxError(void);

// ����SD���
extern SD_HandleTypeDef uSdHandle;

#endif /* __IMAGE_SAVE_H */