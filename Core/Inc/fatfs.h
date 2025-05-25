#ifndef __FATFS_H
#define __FATFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "ff.h"

/* FatFs���� */
#define SD_DRIVE_NUMBER    0   // �߼����������

/* ע�͵�����궨�壬������sdio_sd.h�еĶ����ͻ */
/* #define SD_PRESENT         1   // SD�����ڱ�־ */

/* SD������� */
#define CMD0   (0)      // GO_IDLE_STATE - ��λSD��������״̬
#define CMD1   (1)      // SEND_OP_COND - ���Ͳ�������
#define CMD8   (8)      // SEND_IF_COND - ���ͽӿ�����
#define CMD9   (9)      // SEND_CSD - ����CSD�Ĵ�������
#define CMD10  (10)     // SEND_CID - ����CID�Ĵ�������
#define CMD12  (12)     // STOP_TRANSMISSION - ֹͣ����
#define CMD16  (16)     // SET_BLOCKLEN - ���ÿ鳤��(��λ:�ֽ�)
#define CMD17  (17)     // READ_SINGLE_BLOCK - ������
#define CMD18  (18)     // READ_MULTIPLE_BLOCK - �����
#define CMD23  (23)     // SET_BLOCK_COUNT - ���ÿ����(MMC)
#define CMD24  (24)     // WRITE_BLOCK - д����
#define CMD25  (25)     // WRITE_MULTIPLE_BLOCK - д���
#define CMD55  (55)     // APP_CMD - ��һ��������Ӧ�ó����ض�����
#define CMD58  (58)     // READ_OCR - ��OCR�Ĵ���
#define ACMD23 (0x80+23) // SET_WR_BLK_ERASE_COUNT - ����Ԥ��������(Ϊ���д����׼��)(SD)
#define ACMD41 (0x80+41) // SD_SEND_OP_COND - ���Ͳ�������(SD)

/* �����ͱ�־ */
#define CT_MMC      0x01    // MMC�汾3
#define CT_SD1      0x02    // SD�汾1
#define CT_SD2      0x04    // SD�汾2
#define CT_SDC      (CT_SD1|CT_SD2) // SD��
#define CT_BLOCK    0x08    // ��Ѱַ

/* �������� */
uint8_t FATFS_Init(void);
uint8_t FATFS_Format(void); // ��Ӹ�ʽ������������
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