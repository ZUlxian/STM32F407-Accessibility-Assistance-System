/**
  * @file    fatfs.c
  * @brief   FatFs�ļ�ϵͳʵ�� - ��ʹ��SDIO�ӿ�
  */

#include "fatfs.h"
#include "sdio_sd.h"
#include "ff_gen_drv.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>
#include "user_diskio.h"
extern Diskio_drvTypeDef USER_Driver;

/* ����ȱʡ�ĺ� */
#ifndef FF_MAX_SS
#define FF_MAX_SS 512  /* ���������С */
#endif

#ifndef FF_MAX_LFN
#define FF_MAX_LFN 255  /* ����ļ������� */
#endif

/* ���ر��� */
FATFS fs;  // �ļ�ϵͳ����
FIL file;  // �ļ�����
char fatfs_path[4]; // �߼�����·��

/* ���س�ʱʱ�� (����) */
#define MOUNT_TIMEOUT 5000
#define FORMAT_TIMEOUT 10000

/**
  * @brief  ��ʼ��SD����������ʼ��FatFs (���빦��)
  * @retval ����0��ʾ�ɹ�����0��ʾ����
  */
uint8_t SD_Card_Init(void)
{
    return BSP_SD_Init();
}

/**
  * @brief  ��������ʽ��ʼ��FatFs�ļ�ϵͳ
  * @retval ����0��ʾ�ɹ���1��ʾ���ڹ��أ�����ֵ��ʾ����
  */
uint8_t FATFS_Init(void)
{
    static uint32_t start_time = 0;
    static uint8_t init_state = 0;  // 0=δ��ʼ, 1=������
    FRESULT res;
    
    // �״ε��ã���ʼ��
    if (init_state == 0) {
        // ע�Ṥ����
        #if FF_VOLUMES > 1
        fatfs_path[0] = '0' + SD_DRIVE_NUMBER;
        fatfs_path[1] = ':';
        fatfs_path[2] = '/';
        fatfs_path[3] = 0;
        #else
        fatfs_path[0] = '/';
        fatfs_path[1] = 0;
        #endif
        
        // ���SD���Ƿ��Ѿ���ʼ��
        if (BSP_SD_IsDetected() != SD_PRESENT) {
            return 10; // SD��������
        }
        
        // ȷ���Ѿ�������SD������
        FATFS_LinkDriver(&USER_Driver, fatfs_path);  // ʹ��USER_Driver������SD_Driver
        
        // ����Ϊ����״̬
        init_state = 1;
        start_time = HAL_GetTick();
        
        // ʹ��ǿ�ƹ���ģʽ
        res = f_mount(&fs, fatfs_path, 1);
        
        // ����ɹ���ֱ�ӷ���
        if (res == FR_OK) {
            init_state = 0;  // ����״̬
            return 0;
        }
        
        // �����û���ļ�ϵͳ����Ч����������Ҫ��ʽ��
        if (res == FR_NO_FILESYSTEM || res == FR_INVALID_DRIVE) {
            init_state = 0;  // ����״̬
            return 101;  // ��Ҫ��ʽ��
        }
        
        // ��������
        init_state = 0;  // ����״̬
        return res;
    }
    // ����Ѿ��ڹ����У���鳬ʱ
    else if (init_state == 1) {
        // ����Ƿ�ʱ
        if (HAL_GetTick() - start_time > MOUNT_TIMEOUT) {
            init_state = 0;  // ����״̬
            return 255;  // ��ʱ�������
        }
        
        return 1;  // ���ڹ���
    }
    
    return 0;  // Ĭ�Ϸ��سɹ�
}

/**
  * @brief  ��ʽ��SD��
  * @retval ����0��ʾ�ɹ�����0��ʾ����
  */
uint8_t FATFS_Format(void)
{
    FRESULT res;
    uint32_t start_time = HAL_GetTick();
    uint8_t *workBuffer = NULL;
    
    // ���乤��������
    workBuffer = (uint8_t *)malloc(FF_MAX_SS);
    if (workBuffer == NULL) {
        return 20; // �ڴ����ʧ��
    }
    
    // ִ�и�ʽ�����������أ������м򵥵ĳ�ʱ���
    res = f_mkfs(fatfs_path, FM_FAT32, 0, workBuffer, FF_MAX_SS);
    free(workBuffer);
    
    if (HAL_GetTick() - start_time > FORMAT_TIMEOUT) {
        return 21; // ��ʽ����ʱ
    }
    
    if (res != FR_OK) {
        return 22 + res; // ��ʽ��ʧ��
    }
    
    // ���Թ���
    res = f_mount(&fs, fatfs_path, 0);
    if (res != FR_OK) {
        return 50 + res; // ����ʧ��
    }
    
    return 0; // �ɹ�
}

/**
  * @brief  ���滺�������ݵ��ļ�
  * @param  filePath: �ļ�·��
  * @param  buffer: ���ݻ�����
  * @param  size: ���ݴ�С
  * @retval ����0��ʾ�ɹ�����0��ʾ����
  */
uint8_t SaveBufferToFile(const char* filePath, uint8_t* buffer, uint32_t size) {
    FRESULT res;
    UINT bytesWritten;
    char fullPath[FF_MAX_LFN + 1];
    
    // ��������·�� - �Ƴ�ǰ��б�ܣ�ȷ��·����ʽ��ȷ
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // ����ǵ������ã�����Ҫ���������ǰ׺
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // ���ļ� (�����򸲸�)
    res = f_open(&file, fullPath, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        return 1; // ��ʧ��
    }
    
    // д������
    res = f_write(&file, buffer, size, &bytesWritten);
    if (res != FR_OK || bytesWritten != size) {
        f_close(&file);
        return 2; // д��ʧ��
    }
    
    // ȷ������д�����
    res = f_sync(&file);
    if (res != FR_OK) {
        f_close(&file);
        return 4; // ͬ��ʧ��
    }
    
    // �ر��ļ�
    res = f_close(&file);
    if (res != FR_OK) {
        return 3; // �ر�ʧ��
    }
    
    return 0; // �ɹ�
}

/**
  * @brief  ���ļ���ȡ���ݵ�������
  * @param  filePath: �ļ�·��
  * @param  buffer: ���ݻ�����
  * @param  maxSize: ����������С
  * @param  actualSize: ʵ�ʶ�ȡ��С
  * @retval ����0��ʾ�ɹ�����0��ʾ����
  */
uint8_t ReadFileToBuffer(const char* filePath, uint8_t* buffer, uint32_t maxSize, uint32_t* actualSize) {
    FRESULT res;
    UINT bytesRead;
    char fullPath[FF_MAX_LFN + 1];
    
    // ��������·�� - �޸�·����ʽ����
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // ����ǵ������ã�����Ҫ���������ǰ׺
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // ���ļ�
    res = f_open(&file, fullPath, FA_READ);
    if (res != FR_OK) {
        return 1; // ��ʧ��
    }
    
    // ��ȡ����
    res = f_read(&file, buffer, maxSize, &bytesRead);
    if (res != FR_OK) {
        f_close(&file);
        return 2; // ��ȡʧ��
    }
    
    *actualSize = bytesRead;
    
    // �ر��ļ�
    res = f_close(&file);
    if (res != FR_OK) {
        return 3; // �ر�ʧ��
    }
    
    return 0; // �ɹ�
}

/**
  * @brief  �������ļ�
  * @param  oldPath: ԭ�ļ�·��
  * @param  newPath: ���ļ�·��
  * @retval ����0��ʾ�ɹ�����0��ʾ����
  */
uint8_t RenameFile(const char* oldPath, const char* newPath) {
    FRESULT res;
    char fullOldPath[FF_MAX_LFN + 1];
    char fullNewPath[FF_MAX_LFN + 1];
    
    // ��������·�� - �޸�·����ʽ����
    #if FF_VOLUMES > 1
    if (oldPath[0] == '/') {
        strcpy(fullOldPath, fatfs_path);
        strcat(fullOldPath, oldPath + 1);
    } else {
        strcpy(fullOldPath, fatfs_path);
        strcat(fullOldPath, oldPath);
    }
    
    if (newPath[0] == '/') {
        strcpy(fullNewPath, fatfs_path);
        strcat(fullNewPath, newPath + 1);
    } else {
        strcpy(fullNewPath, fatfs_path);
        strcat(fullNewPath, newPath);
    }
    #else
    // ����ǵ������ã�����·����ʽ
    if (oldPath[0] == '/') {
        strcpy(fullOldPath, oldPath + 1);
    } else {
        strcpy(fullOldPath, oldPath);
    }
    
    if (newPath[0] == '/') {
        strcpy(fullNewPath, newPath + 1);
    } else {
        strcpy(fullNewPath, newPath);
    }
    #endif
    
    // �������ļ�
    res = f_rename(fullOldPath, fullNewPath);
    
    return (res == FR_OK) ? 0 : 1;
}

/**
  * @brief  ����ļ��Ƿ����
  * @param  filePath: �ļ�·��
  * @retval ����1��ʾ���ڣ�0��ʾ������
  */
uint8_t FileExists(const char* filePath) {
    FRESULT res;
    FILINFO fno;
    char fullPath[FF_MAX_LFN + 1];
    
    // ��������·�� - �޸�·����ʽ����
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // ����ǵ������ã�����·����ʽ
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // ��ȡ�ļ���Ϣ
    res = f_stat(fullPath, &fno);
    
    return (res == FR_OK) ? 1 : 0;
}

/**
  * @brief  ɾ���ļ�
  * @param  filePath: �ļ�·��
  * @retval ����0��ʾ�ɹ�����0��ʾ����
  */
uint8_t DeleteFile(const char* filePath) {
    FRESULT res;
    char fullPath[FF_MAX_LFN + 1];
    
    // ��������·�� - �޸�·����ʽ����
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // ����ǵ������ã�����·����ʽ
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // ɾ���ļ�
    res = f_unlink(fullPath);
    
    return (res == FR_OK) ? 0 : 1;
}

/**
  * @brief  ����Ŀ¼
  * @param  dirPath: Ŀ¼·��
  * @retval ����0��ʾ�ɹ�����0��ʾ����
  */
uint8_t CreateDirectory(const char* dirPath) {
    FRESULT res;
    char fullPath[FF_MAX_LFN + 1];
    
    // ��������·�� - �޸�·����ʽ����
    #if FF_VOLUMES > 1
    if (dirPath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, dirPath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, dirPath);
    }
    #else
    // ����ǵ������ã�����·����ʽ
    if (dirPath[0] == '/') {
        strcpy(fullPath, dirPath + 1);
    } else {
        strcpy(fullPath, dirPath);
    }
    #endif
    
    // ����Ŀ¼
    res = f_mkdir(fullPath);
    
    return (res == FR_OK || res == FR_EXIST) ? 0 : 1;
}