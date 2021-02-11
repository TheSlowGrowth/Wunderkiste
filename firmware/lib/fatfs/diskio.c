/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
/* Angepasst von UB                                                      */
/* V:1.0 / 27.11.2014                                                    */
/*-----------------------------------------------------------------------*/



#include "diskio.h"		// FatFs lower layer API
#include "drivers/stm32_ub_usbdisk.h"	// UB: USB drive control
#include "drivers/stm32_ub_atadrive.h"	// UB: ATA drive control
#include "drivers/stm32_ub_sdcard.h"	// UB: MMC/SDC control

/* Definitions of physical drive number for each media */
#define MMC		0
#define USB		1
#define ATA		2


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	int result;

	switch (pdrv) {
	case ATA :
		result = ATA_disk_status();

		// translate the reslut code here
		if(result==0) {
		  stat=0;
		}
		else {
		  stat=STA_NODISK | STA_NOINIT;
		}                

		return stat;

	case MMC :
		stat = MMC_disk_status();
                return stat;

	case USB :
		result = USB_disk_status();

		// translate the reslut code here
		if(result==0) {
		  stat=0;
		}
		else {
		  stat=STA_NODISK | STA_NOINIT;
		}

		return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;
	int result;

	switch (pdrv) {
	case ATA :
		result = ATA_disk_initialize();

		// translate the reslut code here
		if(result==0) {
		  stat=0;
		}
		else {
		  stat=STA_NOINIT;
		}              

		return stat;

	case MMC :
		stat = MMC_disk_initialize();
                return stat;

	case USB :
		result = USB_disk_initialize();

		// translate the reslut code here
		if(result==0) {
		  stat=0;
		}
		else {
		  stat=STA_NOINIT;
		}                

		return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res;
	int result;

	switch (pdrv) {
	case ATA :
		// translate the arguments here

		result = ATA_disk_read(buff, sector, count);

		// translate the reslut code here
		if(result==0) {
		  res=RES_OK;
		}
		else {
		  res=RES_ERROR;
		}                

		return res;

	case MMC :
		// translate the arguments here

		res = MMC_disk_read(buff, sector, count);
                return res;

	case USB :
		// translate the arguments here

		result = USB_disk_read(buff, sector, count);

		// translate the reslut code here
		if(result==0) {
		  res=RES_OK;
		}
		else {
		  res=RES_ERROR;
		}                

		return res;
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res;
	int result;

	switch (pdrv) {
	case ATA :
		// translate the arguments here

		result = ATA_disk_write(buff, sector, count);

		// translate the reslut code here
		if(result==0) {
		  res=RES_OK;
		}
		else {
		  res=RES_ERROR;
		}                

		return res;

	case MMC :
		// translate the arguments here

		res = MMC_disk_write(buff, sector, count);
                return res;

	case USB :
		// translate the arguments here

		result = USB_disk_write(buff, sector, count);

		// translate the reslut code here
		if(result==0) {
		  res=RES_OK;
		}
		else {
		  res=RES_ERROR;
		}                

		return res;
	}

	return RES_PARERR;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DWORD get_fattime (void)
{
	return	((2006UL-1980) << 25)	      // Year = 2006
			| (2UL << 21)	      // Month = Feb
			| (9UL << 16)	      // Day = 9
			| (22U << 11)	      // Hour = 22
			| (30U << 5)	      // Min = 30
			| (0U >> 1)	      // Sec = 0
			;
}


#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	int result;

	switch (pdrv) {
	case ATA :
		result = ATA_disk_ioctl(cmd, buff);

		// Process of the command for the ATA drive
		if(result==0) {
		  res=RES_OK;
		}
		else {
		  res=RES_ERROR;
		}          

		return res;

	case MMC :
		res = MMC_disk_ioctl(cmd, buff);
                return res;

	case USB :
		result = USB_disk_ioctl(cmd, buff);

		// Process of the command the USB drive
		if(result==0) {
		  res=RES_OK;
		}
		else {
		  res=RES_ERROR;
		}          

		return res;
	}

	return RES_PARERR;
}
#endif
