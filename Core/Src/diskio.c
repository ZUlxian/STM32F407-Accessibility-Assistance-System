/**
  * @file    diskio.c
  * @brief   FatFs底层磁盘I/O函数实现
  */

#include "diskio.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include <string.h>

/* 用于存储所有已注册的磁盘驱动 */
extern Disk_drvTypeDef disk;

/**
  * @brief  初始化磁盘
  * @param  pdrv: 物理驱动号 (0..)
  * @retval DSTATUS: 操作状态
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
  * @brief  获取磁盘状态
  * @param  pdrv: 物理驱动号 (0..)
  * @retval DSTATUS: 操作状态
  */
DSTATUS disk_status(BYTE pdrv)
{
  DSTATUS stat;
  
  stat = disk.drv[pdrv]->disk_status(disk.lun[pdrv]);
  return stat;
}

/**
  * @brief  读取扇区数据
  * @param  pdrv: 物理驱动号 (0..)
  * @param  buff: 输出数据缓冲区
  * @param  sector: 扇区地址 (LBA)
  * @param  count: 扇区数量 (1..128)
  * @retval DRESULT: 操作结果
  */
DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
  DRESULT res;
  
  res = disk.drv[pdrv]->disk_read(disk.lun[pdrv], buff, sector, count);
  return res;
}

/**
  * @brief  写入扇区数据
  * @param  pdrv: 物理驱动号 (0..)
  * @param  buff: 输入数据缓冲区
  * @param  sector: 扇区地址 (LBA)
  * @param  count: 扇区数量 (1..128)
  * @retval DRESULT: 操作结果
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
  * @brief  I/O控制操作
  * @param  pdrv: 物理驱动号 (0..)
  * @param  cmd: 控制命令
  * @param  buff: 缓冲区
  * @retval DRESULT: 操作结果
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
  * @brief  获取当前时间，用于文件系统时间戳
  * @param  None
  * @retval 打包的时间数据，格式参见FatFs文档
  */
DWORD get_fattime(void)
{
  /* 固定返回2023/1/1 00:00:00 */
  return ((DWORD)(2023 - 1980) << 25) // 年(0-127, 从1980年开始)
       | ((DWORD)1 << 21)             // 月(1-12)
       | ((DWORD)1 << 16)             // 日(1-31)
       | ((DWORD)0 << 11)             // 时(0-23)
       | ((DWORD)0 << 5)              // 分(0-59)
       | ((DWORD)0 >> 1);             // 秒/2(0-29)
}