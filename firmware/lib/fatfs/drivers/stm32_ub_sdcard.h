//--------------------------------------------------------------
// File     : stm32_ub_usbdisk.h
//--------------------------------------------------------------

//--------------------------------------------------------------
#ifndef __STM32F4_UB_SDCARD_H
#define __STM32F4_UB_SDCARD_H

//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "stm32f4xx.h"
#include "diskio.h"
#include "stm32_ub_spi2.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_tim.h"
#include "misc.h"

//--------------------------------------------------------------
// defines der SPI Schnittstelle
// SPI2_CLK = 42MHz
//--------------------------------------------------------------
#define SD_SPI_PORT               SPI2
#define SD_SPI_MODE               SPI_MODE_0
#define SD_SPI_CLK_SLOW           SPI_BaudRatePrescaler_128  // 0.5*656 kHz
#define SD_SPI_CLK_FAST           SPI_BaudRatePrescaler_32   // 1.3125 MHz

//--------------------------------------------------------------
// interner PullUp fuer MISO
// aktivieren oder deaktivieren
//    1 = mit internem PullUp
//    0 = ohne internem PullUp -> externer PullUp notwendig
//--------------------------------------------------------------
#define USE_INTERNAL_MISO_PULLUP  0  // (mit internem PullUp)

#define SD_SPI_MISO_PORT          GPIOB
#define SD_SPI_MISO_PIN           GPIO_Pin_14

//--------------------------------------------------------------
// SlaveSelect der SD-Karte (PB12)
//--------------------------------------------------------------
#define SD_SLAVESEL_PIN           GPIO_Pin_12
#define SD_SLAVESEL_GPIO_PORT     GPIOB
#define SD_SLAVESEL_GPIO_CLK      RCC_AHB1Periph_GPIOB

//--------------------------------------------------------------
// Detect-Pin
// aktivieren oder deaktivieren
//    1 = mit Detect-Pin
//    0 = ohne Detect-Pin
//--------------------------------------------------------------
#define   USE_DETECT_PIN          0  // (ohne Detect-Pin)

//--------------------------------------------------------------
// Detect-Pin der SD-Karte (PC0)
//--------------------------------------------------------------
#if USE_DETECT_PIN==1
  #define SD_DETECT_PIN           GPIO_Pin_0
  #define SD_DETECT_GPIO_PORT     GPIOC
  #define SD_DETECT_GPIO_CLK      RCC_AHB1Periph_GPIOC
#endif

//--------------------------------------------------------------
// Timer f�r 1ms Interrupt => TIM6
// Grundfrequenz = 2*APB1 (APB1=42MHz) => TIM_CLK=84MHz
// TIM_Frq = TIM_CLK/(periode+1)/(vorteiler+1)
// TIM_Frq = 1kHz => 1ms
//--------------------------------------------------------------
#define   SD_1MS_TIM              TIM6
#define   SD_1MS_TIM_CLK          RCC_APB1Periph_TIM6
#define   SD_1MS_TIM_PERIODE      999
#define   SD_1MS_TIM_PRESCALE     83
#define   SD_1MS_TIM_IRQ          TIM6_DAC_IRQn
#define   SD_1MS_TIM_ISR_HANDLER  TIM6_DAC_IRQHandler

//--------------------------------------------------------------
// Schreibschutz
// aktivieren oder deaktivieren
//    1 = mit Schreibschutz
//    0 = ohne Schreibschutz
//--------------------------------------------------------------
#define	USE_WRITE_PROTECTION	  0  // (ohne Schreibschutz)

//--------------------------------------------------------------
/* MMC/SD command */
//--------------------------------------------------------------
#define CMD0	(0)        // GO_IDLE_STATE
#define CMD1	(1)        // SEND_OP_COND (MMC)
#define	ACMD41	(0x80+41)  // SEND_OP_COND (SDC)
#define CMD8	(8)        // SEND_IF_COND
#define CMD9	(9)        // SEND_CSD
#define CMD10	(10)       // SEND_CID
#define CMD12	(12)       // STOP_TRANSMISSION
#define ACMD13	(0x80+13)  // SD_STATUS (SDC)
#define CMD16	(16)       // SET_BLOCKLEN
#define CMD17	(17)       // READ_SINGLE_BLOCK
#define CMD18	(18)       // READ_MULTIPLE_BLOCK
#define CMD23	(23)       // SET_BLOCK_COUNT (MMC)
#define	ACMD23	(0x80+23)  // SET_WR_BLK_ERASE_COUNT (SDC)
#define CMD24	(24)       // WRITE_BLOCK
#define CMD25	(25)       // WRITE_MULTIPLE_BLOCK
#define CMD32	(32)       // ERASE_ER_BLK_START
#define CMD33	(33)       // ERASE_ER_BLK_END
#define CMD38	(38)       // ERASE
#define CMD55	(55)       // APP_CMD
#define CMD58	(58)       // READ_OCR

#define NULL    0
#define SD_PRESENT        ((uint8_t)0x01)
#define SD_NOT_PRESENT    ((uint8_t)0x00)

//--------------------------------------------------------------
// Globale Funktionen
//--------------------------------------------------------------
void UB_SDCard_Init(void);
uint8_t UB_SDCard_CheckMedia(void);
DSTATUS MMC_disk_initialize(void);
DSTATUS MMC_disk_status(void);
DRESULT MMC_disk_read(BYTE *buff,DWORD sector,UINT count);
DRESULT MMC_disk_write(const BYTE *buff,DWORD sector,	UINT count);
DRESULT MMC_disk_ioctl(BYTE cmd,void *buff);

//--------------------------------------------------------------
#endif // __STM32F4_UB_SDCARD_H
