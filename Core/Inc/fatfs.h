#ifndef __FATFS_H
#define __FATFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "ff.h"

/* FatFs配置 */
#define SD_DRIVE_NUMBER    0   // 逻辑驱动器编号

/* 注释掉这个宏定义，避免与sdio_sd.h中的定义冲突 */
/* #define SD_PRESENT         1   // SD卡存在标志 */

/* SD卡命令定义 */
#define CMD0   (0)      // GO_IDLE_STATE - 复位SD卡到空闲状态
#define CMD1   (1)      // SEND_OP_COND - 发送操作条件
#define CMD8   (8)      // SEND_IF_COND - 发送接口条件
#define CMD9   (9)      // SEND_CSD - 请求CSD寄存器数据
#define CMD10  (10)     // SEND_CID - 请求CID寄存器数据
#define CMD12  (12)     // STOP_TRANSMISSION - 停止传输
#define CMD16  (16)     // SET_BLOCKLEN - 设置块长度(单位:字节)
#define CMD17  (17)     // READ_SINGLE_BLOCK - 读单块
#define CMD18  (18)     // READ_MULTIPLE_BLOCK - 读多块
#define CMD23  (23)     // SET_BLOCK_COUNT - 设置块计数(MMC)
#define CMD24  (24)     // WRITE_BLOCK - 写单块
#define CMD25  (25)     // WRITE_MULTIPLE_BLOCK - 写多块
#define CMD55  (55)     // APP_CMD - 下一个命令是应用程序特定命令
#define CMD58  (58)     // READ_OCR - 读OCR寄存器
#define ACMD23 (0x80+23) // SET_WR_BLK_ERASE_COUNT - 设置预擦除块数(为多块写入做准备)(SD)
#define ACMD41 (0x80+41) // SD_SEND_OP_COND - 发送操作条件(SD)

/* 卡类型标志 */
#define CT_MMC      0x01    // MMC版本3
#define CT_SD1      0x02    // SD版本1
#define CT_SD2      0x04    // SD版本2
#define CT_SDC      (CT_SD1|CT_SD2) // SD卡
#define CT_BLOCK    0x08    // 块寻址

/* 函数声明 */
uint8_t FATFS_Init(void);
uint8_t FATFS_Format(void); // 添加格式化函数的声明
uint8_t SaveBufferToFile(const char* filePath, uint8_t* buffer, uint32_t size);
uint8_t ReadFileToBuffer(const char* filePath, uint8_t* buffer, uint32_t maxSize, uint32_t* actualSize);
uint8_t RenameFile(const char* oldPath, const char* newPath);
uint8_t FileExists(const char* filePath);
uint8_t DeleteFile(const char* filePath);
uint8_t CreateDirectory(const char* dirPath);

#ifdef __cplusplus
}
#endif

#endif /* __FATFS_H */