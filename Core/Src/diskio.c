/**
  * @file    diskio.c
  * @brief   FatFs�ײ����I/O����ʵ��
  */

#include "diskio.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include <string.h>

/* ���ڴ洢������ע��Ĵ������� */
extern Disk_drvTypeDef disk;

/**
  * @brief  ��ʼ������
  * @param  pdrv: ���������� (0..)
  * @retval DSTATUS: ����״̬
  */
DSTATUS disk_initialize(BYTE pdrv)
{
  DSTATUS stat = RES_ERROR;
  
  if(disk.is_initialized[pdrv] == 0)
  { 
    disk.is_initialized[pdrv] = 1;
    stat = disk.drv[pdrv]->disk_initialize(disk.lun[pdrv]);
  }
  return stat;
}

/**
  * @brief  ��ȡ����״̬
  * @param  pdrv: ���������� (0..)
  * @retval DSTATUS: ����״̬
  */
DSTATUS disk_status(BYTE pdrv)
{
  DSTATUS stat;
  
  stat = disk.drv[pdrv]->disk_status(disk.lun[pdrv]);
  return stat;
}

/**
  * @brief  ��ȡ��������
  * @param  pdrv: ���������� (0..)
  * @param  buff: ������ݻ�����
  * @param  sector: ������ַ (LBA)
  * @param  count: �������� (1..128)
  * @retval DRESULT: �������
  */
DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
  DRESULT res;
  
  res = disk.drv[pdrv]->disk_read(disk.lun[pdrv], buff, sector, count);
  return res;
}

/**
  * @brief  д����������
  * @param  pdrv: ���������� (0..)
  * @param  buff: �������ݻ�����
  * @param  sector: ������ַ (LBA)
  * @param  count: �������� (1..128)
  * @retval DRESULT: �������
  */
#if _USE_WRITE == 1
DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
  DRESULT res;
  
  res = disk.drv[pdrv]->disk_write(disk.lun[pdrv], buff, sector, count);
  return res;
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O���Ʋ���
  * @param  pdrv: ���������� (0..)
  * @param  cmd: ��������
  * @param  buff: ������
  * @retval DRESULT: �������
  */
#if _USE_IOCTL == 1
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
  DRESULT res;
  
  res = disk.drv[pdrv]->disk_ioctl(disk.lun[pdrv], cmd, buff);
  return res;
}
#endif /* _USE_IOCTL == 1 */

/**
  * @brief  ��ȡ��ǰʱ�䣬�����ļ�ϵͳʱ���
  * @param  None
  * @retval �����ʱ�����ݣ���ʽ�μ�FatFs�ĵ�
  */
DWORD get_fattime(void)
{
  /* �̶�����2023/1/1 00:00:00 */
  return ((DWORD)(2023 - 1980) << 25) // ��(0-127, ��1980�꿪ʼ)
       | ((DWORD)1 << 21)             // ��(1-12)
       | ((DWORD)1 << 16)             // ��(1-31)
       | ((DWORD)0 << 11)             // ʱ(0-23)
       | ((DWORD)0 << 5)              // ��(0-59)
       | ((DWORD)0 >> 1);             // ��/2(0-29)
}