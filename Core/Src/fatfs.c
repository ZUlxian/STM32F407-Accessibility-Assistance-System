/**
  * @file    fatfs.c
  * @brief   FatFs文件系统实现 - 仅使用SDIO接口
  */

#include "fatfs.h"
#include "sdio_sd.h"
#include "ff_gen_drv.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>
#include "user_diskio.h"
extern Diskio_drvTypeDef USER_Driver;

/* 定义缺省的宏 */
#ifndef FF_MAX_SS
#define FF_MAX_SS 512  /* 最大扇区大小 */
#endif

#ifndef FF_MAX_LFN
#define FF_MAX_LFN 255  /* 最大长文件名长度 */
#endif

/* 本地变量 */
FATFS fs;  // 文件系统对象
FIL file;  // 文件对象
char fatfs_path[4]; // 逻辑驱动路径

/* 挂载超时时间 (毫秒) */
#define MOUNT_TIMEOUT 5000
#define FORMAT_TIMEOUT 10000

/**
  * @brief  初始化SD卡，但不初始化FatFs (分离功能)
  * @retval 返回0表示成功，非0表示错误
  */
uint8_t SD_Card_Init(void)
{
    return BSP_SD_Init();
}

/**
  * @brief  非阻塞方式初始化FatFs文件系统
  * @retval 返回0表示成功，1表示正在挂载，其他值表示错误
  */
uint8_t FATFS_Init(void)
{
    static uint32_t start_time = 0;
    static uint8_t init_state = 0;  // 0=未开始, 1=挂载中
    FRESULT res;
    
    // 首次调用，初始化
    if (init_state == 0) {
        // 注册工作区
        #if FF_VOLUMES > 1
        fatfs_path[0] = '0' + SD_DRIVE_NUMBER;
        fatfs_path[1] = ':';
        fatfs_path[2] = '/';
        fatfs_path[3] = 0;
        #else
        fatfs_path[0] = '/';
        fatfs_path[1] = 0;
        #endif
        
        // 检查SD卡是否已经初始化
        if (BSP_SD_IsDetected() != SD_PRESENT) {
            return 10; // SD卡不存在
        }
        
        // 确保已经链接了SD卡驱动
        FATFS_LinkDriver(&USER_Driver, fatfs_path);  // 使用USER_Driver而不是SD_Driver
        
        // 设置为挂载状态
        init_state = 1;
        start_time = HAL_GetTick();
        
        // 使用强制挂载模式
        res = f_mount(&fs, fatfs_path, 1);
        
        // 如果成功，直接返回
        if (res == FR_OK) {
            init_state = 0;  // 重置状态
            return 0;
        }
        
        // 如果是没有文件系统或无效驱动器，需要格式化
        if (res == FR_NO_FILESYSTEM || res == FR_INVALID_DRIVE) {
            init_state = 0;  // 重置状态
            return 101;  // 需要格式化
        }
        
        // 其他错误
        init_state = 0;  // 重置状态
        return res;
    }
    // 如果已经在挂载中，检查超时
    else if (init_state == 1) {
        // 检查是否超时
        if (HAL_GetTick() - start_time > MOUNT_TIMEOUT) {
            init_state = 0;  // 重置状态
            return 255;  // 超时错误代码
        }
        
        return 1;  // 正在挂载
    }
    
    return 0;  // 默认返回成功
}

/**
  * @brief  格式化SD卡
  * @retval 返回0表示成功，非0表示错误
  */
uint8_t FATFS_Format(void)
{
    FRESULT res;
    uint32_t start_time = HAL_GetTick();
    uint8_t *workBuffer = NULL;
    
    // 分配工作缓冲区
    workBuffer = (uint8_t *)malloc(FF_MAX_SS);
    if (workBuffer == NULL) {
        return 20; // 内存分配失败
    }
    
    // 执行格式化，不带挂载，并带有简单的超时检查
    res = f_mkfs(fatfs_path, FM_FAT32, 0, workBuffer, FF_MAX_SS);
    free(workBuffer);
    
    if (HAL_GetTick() - start_time > FORMAT_TIMEOUT) {
        return 21; // 格式化超时
    }
    
    if (res != FR_OK) {
        return 22 + res; // 格式化失败
    }
    
    // 尝试挂载
    res = f_mount(&fs, fatfs_path, 0);
    if (res != FR_OK) {
        return 50 + res; // 挂载失败
    }
    
    return 0; // 成功
}

/**
  * @brief  保存缓冲区数据到文件
  * @param  filePath: 文件路径
  * @param  buffer: 数据缓冲区
  * @param  size: 数据大小
  * @retval 返回0表示成功，非0表示错误
  */
uint8_t SaveBufferToFile(const char* filePath, uint8_t* buffer, uint32_t size) {
    FRESULT res;
    UINT bytesWritten;
    char fullPath[FF_MAX_LFN + 1];
    
    // 构建完整路径 - 移除前导斜杠，确保路径格式正确
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // 如果是单卷配置，不需要添加驱动器前缀
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // 打开文件 (创建或覆盖)
    res = f_open(&file, fullPath, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        return 1; // 打开失败
    }
    
    // 写入数据
    res = f_write(&file, buffer, size, &bytesWritten);
    if (res != FR_OK || bytesWritten != size) {
        f_close(&file);
        return 2; // 写入失败
    }
    
    // 确保数据写入磁盘
    res = f_sync(&file);
    if (res != FR_OK) {
        f_close(&file);
        return 4; // 同步失败
    }
    
    // 关闭文件
    res = f_close(&file);
    if (res != FR_OK) {
        return 3; // 关闭失败
    }
    
    return 0; // 成功
}

/**
  * @brief  从文件读取数据到缓冲区
  * @param  filePath: 文件路径
  * @param  buffer: 数据缓冲区
  * @param  maxSize: 缓冲区最大大小
  * @param  actualSize: 实际读取大小
  * @retval 返回0表示成功，非0表示错误
  */
uint8_t ReadFileToBuffer(const char* filePath, uint8_t* buffer, uint32_t maxSize, uint32_t* actualSize) {
    FRESULT res;
    UINT bytesRead;
    char fullPath[FF_MAX_LFN + 1];
    
    // 构建完整路径 - 修复路径格式问题
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // 如果是单卷配置，不需要添加驱动器前缀
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // 打开文件
    res = f_open(&file, fullPath, FA_READ);
    if (res != FR_OK) {
        return 1; // 打开失败
    }
    
    // 读取数据
    res = f_read(&file, buffer, maxSize, &bytesRead);
    if (res != FR_OK) {
        f_close(&file);
        return 2; // 读取失败
    }
    
    *actualSize = bytesRead;
    
    // 关闭文件
    res = f_close(&file);
    if (res != FR_OK) {
        return 3; // 关闭失败
    }
    
    return 0; // 成功
}

/**
  * @brief  重命名文件
  * @param  oldPath: 原文件路径
  * @param  newPath: 新文件路径
  * @retval 返回0表示成功，非0表示错误
  */
uint8_t RenameFile(const char* oldPath, const char* newPath) {
    FRESULT res;
    char fullOldPath[FF_MAX_LFN + 1];
    char fullNewPath[FF_MAX_LFN + 1];
    
    // 构建完整路径 - 修复路径格式问题
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
    // 如果是单卷配置，处理路径格式
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
    
    // 重命名文件
    res = f_rename(fullOldPath, fullNewPath);
    
    return (res == FR_OK) ? 0 : 1;
}

/**
  * @brief  检查文件是否存在
  * @param  filePath: 文件路径
  * @retval 返回1表示存在，0表示不存在
  */
uint8_t FileExists(const char* filePath) {
    FRESULT res;
    FILINFO fno;
    char fullPath[FF_MAX_LFN + 1];
    
    // 构建完整路径 - 修复路径格式问题
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // 如果是单卷配置，处理路径格式
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // 获取文件信息
    res = f_stat(fullPath, &fno);
    
    return (res == FR_OK) ? 1 : 0;
}

/**
  * @brief  删除文件
  * @param  filePath: 文件路径
  * @retval 返回0表示成功，非0表示错误
  */
uint8_t DeleteFile(const char* filePath) {
    FRESULT res;
    char fullPath[FF_MAX_LFN + 1];
    
    // 构建完整路径 - 修复路径格式问题
    #if FF_VOLUMES > 1
    if (filePath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, filePath);
    }
    #else
    // 如果是单卷配置，处理路径格式
    if (filePath[0] == '/') {
        strcpy(fullPath, filePath + 1);
    } else {
        strcpy(fullPath, filePath);
    }
    #endif
    
    // 删除文件
    res = f_unlink(fullPath);
    
    return (res == FR_OK) ? 0 : 1;
}

/**
  * @brief  创建目录
  * @param  dirPath: 目录路径
  * @retval 返回0表示成功，非0表示错误
  */
uint8_t CreateDirectory(const char* dirPath) {
    FRESULT res;
    char fullPath[FF_MAX_LFN + 1];
    
    // 构建完整路径 - 修复路径格式问题
    #if FF_VOLUMES > 1
    if (dirPath[0] == '/') {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, dirPath + 1);
    } else {
        strcpy(fullPath, fatfs_path);
        strcat(fullPath, dirPath);
    }
    #else
    // 如果是单卷配置，处理路径格式
    if (dirPath[0] == '/') {
        strcpy(fullPath, dirPath + 1);
    } else {
        strcpy(fullPath, dirPath);
    }
    #endif
    
    // 创建目录
    res = f_mkdir(fullPath);
    
    return (res == FR_OK || res == FR_EXIST) ? 0 : 1;
}