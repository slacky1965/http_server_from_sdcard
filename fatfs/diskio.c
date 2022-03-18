/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */

int sd_read_sector(uint32_t start_block, uint8_t *buffer, uint32_t sector_count);
int sd_write_sector(uint32_t start_block, uint8_t *buffer, uint32_t sector_count);
bool get_sdcard_status();
//uint64_t sd_get_capacity();
//uint32_t sd_get_sector_size();


/* Definitions of physical drive number for each drive */
#define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS ICACHE_FLASH_ATTR disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
    if (get_sdcard_status())
        return RES_OK;

    return RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS ICACHE_FLASH_ATTR disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
    return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT ICACHE_FLASH_ATTR disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
    if (sd_read_sector(sector, (uint8_t*)buff, count)) {
        return RES_OK;
    }

    return RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT ICACHE_FLASH_ATTR disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
    if (sd_write_sector(sector, (uint8_t*)buff, count)) {
        return RES_OK;
    }

    return RES_ERROR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT ICACHE_FLASH_ATTR disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{


//    DRESULT res = RES_PARERR;
//
//    switch(cmd) {
//        case CTRL_SYNC:
//            res = RES_OK;
//            break;
//        case GET_SECTOR_COUNT:
//            *(DWORD*)buff = (uint32_t)(sd_get_capacity()/sd_get_sector_size());
//            res = RES_OK;
//            break;
//        case GET_SECTOR_SIZE:
//            *(DWORD*)buff = sd_get_sector_size();
//            res = RES_OK;
//            break;
//        case GET_BLOCK_SIZE:
//            *(DWORD*)buff = 1;
//            res = RES_OK;
//            break;
//        case CTRL_TRIM:
//            res = RES_OK;
//            break;
//        default:
//            break;
//    }
//
//    return res;
//
//    return RES_PARERR;

    if(cmd == CTRL_SYNC) {
        return RES_OK;
    } else {
        // should never be called
        return RES_ERROR;
    }
}

DWORD ICACHE_FLASH_ATTR get_fattime(void)
{
  return 0;
}
