//--------------------------------------------------------------
// File     : stm32_ub_sdcard.c
// Datum    : 27.11.2014
// Version  : 1.0
// Autor    : UB
// EMail    : mc-4u(@)t-online.de
// Web      : www.mikrocontroller-4u.de
// CPU      : STM32F4
// IDE      : CooCox CoIDE 1.7.4
// GCC      : 4.7 2012q4
// Module   : STM32_UB_SPI2, GPIO, TIM, MISC
// Funktion : FATFS-Dateisystem fuer SD-Medien
//            LoLevel-IO-Modul
//            Quelle = STM32-Example von ChaN
// Hinweis  : Betrieb per SPI-Schnittstelle
//            (SPI Settings : stm32_ub_sdcard.h)
//            Fuer die 1kHz ISR wird TIM6 benutzt !
//
//   PB12 -> ChipSelect = SD-Karte CS   (CD)
//   PB13 -> SPI-SCK    = SD-Karte SCLK (CLK)
//   PB14 -> SPI-MISO   = SD-Karte DO   (DAT0) (*)
//   PB15 -> SPI-MOSI   = SD-Karte DI   (CMD)
//    (*) MISO needs PullUp (internal or external)
//
// mit Detect-Pin :
//
//   PC0  -> SD_Detect-Pin (Hi=ohne SD-Karte)
//
//--------------------------------------------------------------
//
// Copyright (C) 2014, ChaN, all right reserved.
//
// * This software is a free software and there is NO WARRANTY.
// * No restriction on use. You can use, modify and redistribute it for
//   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
// * Redistributions of source code must retain the above copyright notice.
//
//-------------------------------------------------------------------------

/*#define 	CT_MMC   0x01
#define 	CT_SD1   0x02
#define 	CT_SD2   0x04
#define 	CT_SDC   (CT_SD1|CT_SD2)
#define 	CT_BLOCK   0x08*/

//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
//#include "integer.h"
#include "ff.h"
#include "stm32_ub_sdcard.h"

//--------------------------------------------------------------
// Globale Variabeln
//--------------------------------------------------------------
static volatile DSTATUS Stat = STA_NOINIT;
static volatile UINT Timer1, Timer2;
static BYTE CardType;



//--------------------------------------------------------------
// interne Funktionen
//--------------------------------------------------------------
void init_gpio(void);
void init_tim(void);
void init_nvic(void);
void init_spi(void);
void CS_HIGH(void);
void CS_LOW(void);
void FCLK_SLOW(void);
void FCLK_FAST(void);
uint8_t SD_Detect(void);
//--------------------------------------------------------------
static BYTE xchg_spi(BYTE dat);
static void rcvr_spi_multi(BYTE *buff, UINT btr);
static void xmit_spi_multi(const BYTE *buff, UINT btx);
static int wait_ready(UINT wt);
static void deselect(void);
static int select(void);
static int rcvr_datablock (BYTE *buff, UINT btr);
static int xmit_datablock(const BYTE *buff,BYTE token);
static BYTE send_cmd (BYTE cmd, DWORD arg);
static BYTE send_cmd_no_wait_ready (BYTE cmd, DWORD arg);
void disk_timerproc(void);
//-------------------------------------------------------------- 

 

//--------------------------------------------------------------
// init der Hardware fuer die SDCard-Funktionen
// muss vor der Benutzung einmal gemacht werden
//--------------------------------------------------------------
void UB_SDCard_Init(void)
{
  // first : timer, nvic, chipselect
  init_tim();
  init_nvic();  
  init_gpio();
  CS_HIGH();			
  // second : spi
  init_spi();
  for(Timer1 = 10; Timer1; ); // 10ms
}


//--------------------------------------------------------------
// Check ob Medium eingelegt ist
// Return Wert :
//   > 0  = wenn Medium eingelegt ist
//     0  = wenn kein Medium eingelegt ist
//--------------------------------------------------------------
uint8_t UB_SDCard_CheckMedia(void) 
{
  uint8_t ret_wert=0;
  
  ret_wert=SD_Detect();

  return(ret_wert);
}


//--------------------------------------------------------------
// init der Disk
//-------------------------------------------------------------- 
DSTATUS MMC_disk_initialize(void)
{ 
  UB_SDCard_Init(); 

  BYTE n, cmd, ty, ocr[4];
  
  for (Timer1 = 10; Timer1; ) ; // 10ms

  if (Stat & STA_NODISK) return Stat;

  FCLK_SLOW();
  for (n = 10; n; n--) xchg_spi(0xFF);	// Send 80 dummy clocks 

  ty = 0;
  CS_LOW(); 
  if (send_cmd_no_wait_ready(CMD0, 0) == 1) { //Put the card SPI/Idle state 
    Timer1 = 1000; // Initialization timeout = 1 sec  
    if (send_cmd_no_wait_ready(CMD8, 0x1AA) == 1) {	// SDv2?  
      for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF); // Get 32 bit return value of R7 resp 
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) { // Is the card supports vcc of 2.7-3.6V? 
        while (Timer1 && send_cmd(ACMD41, 1UL << 30)) ; // Wait for end of initialization with ACMD41(HCS) 
        if (Timer1 && send_cmd(CMD58, 0) == 0) { // Check CCS bit in the OCR 
          for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
          ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2; // Card id SDv2 
        }
      }
    }
    else { // Not SDv2 card 
      if (send_cmd(ACMD41, 0) <= 1) { // SDv1 or MMC? 
        ty = CT_SD1; cmd = ACMD41; //SDv1 (ACMD41(0)) 
      } else {
        ty = CT_MMC; cmd = CMD1; // MMCv3 (CMD1(0)) 
      }
      while (Timer1 && send_cmd(cmd, 0)) ; // Wait for end of initialization 
      if (!Timer1 || send_cmd(CMD16, 512) != 0) ty = 0; // Set block length: 512         
    }
  }
  CardType = ty; // Card type 
  deselect();

  if (ty) { // OK 
    FCLK_FAST(); // Set fast clock 
    Stat &= ~STA_NOINIT; // Clear STA_NOINIT flag 
  } else { // Failed 
    Stat = STA_NOINIT;
  }
  
  return Stat;
}


//--------------------------------------------------------------
// Disk Status abfragen
//-------------------------------------------------------------- 
DSTATUS MMC_disk_status(void)
{
  return Stat;
}


//--------------------------------------------------------------
// READ-Funktion
// buff : Pointer to the data buffer to store read data
// sector : Start sector number (LBA)
// count : Number of sectors to read (1..128)
//--------------------------------------------------------------
DRESULT MMC_disk_read(BYTE *buff,DWORD sector,UINT count)
{
  if (!count) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  if (!(CardType & CT_BLOCK)) sector *= 512; // LBA ot BA conversion (byte addressing cards) 

  if (count == 1) { // Single sector read 
    // READ_SINGLE_BLOCK
    if (send_cmd(CMD17, sector) == 0)
    {
      if (rcvr_datablock(buff, 512))
        count = 0;
    }
  }
  else { // Multiple sector read 
    if (send_cmd(CMD18, sector) == 0) { // READ_MULTIPLE_BLOCK 
      do {
        if (!rcvr_datablock(buff, 512)) break;
        buff += 512;
      } while (--count);
      send_cmd(CMD12, 0); // STOP_TRANSMISSION 
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;
}


//--------------------------------------------------------------
// WRITE-Funktion
// buff : Ponter to the data to write
// sector : Start sector number (LBA)
// count : Number of sectors to write (1..128)
//--------------------------------------------------------------
#if _USE_WRITE
DRESULT MMC_disk_write(const BYTE *buff,DWORD sector,	UINT count)
{
  if (!count) return RES_PARERR;	
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  if (Stat & STA_PROTECT) return RES_WRPRT;

  if (!(CardType & CT_BLOCK)) sector *= 512; // LBA ==> BA conversion (byte addressing cards) 

  if (count == 1) { // Single sector write 
    // WRITE_BLOCK
    if (send_cmd(CMD24, sector) == 0) 
    {
        if (xmit_datablock(buff, 0xFE)) 
          count = 0;
    }
  }
  else { // Multiple sector write 
    if (CardType & CT_SDC) send_cmd(ACMD23, count); // Predefine number of sectors 
    if (send_cmd(CMD25, sector) == 0) { // WRITE_MULTIPLE_BLOCK
      do {
        if (!xmit_datablock(buff, 0xFC)) break;
        buff += 512;
      } while (--count);
      //STOP_TRAN token
      if (!xmit_datablock(0, 0xFD)) count = 1;
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;
}
#endif


//--------------------------------------------------------------
// IOCTL-Funktion
// cmd : Control command code
// buff : Pointer to the conrtol data
//--------------------------------------------------------------
#if _USE_IOCTL
DRESULT MMC_disk_ioctl(BYTE cmd,void *buff)
{
  DRESULT res;
  BYTE n, csd[16];
  DWORD *dp, st, ed, csize;

  if (Stat & STA_NOINIT) return RES_NOTRDY;

  res = RES_ERROR;

  switch (cmd) {
    case CTRL_SYNC : // Wait for end of internal write process of the drive 
      if (select()) res = RES_OK;
    break;

    case GET_SECTOR_COUNT : // Get drive capacity in unit of sector (DWORD) 
      if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
        if ((csd[0] >> 6) == 1) { // SDC ver 2.00 
          csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
          *(DWORD*)buff = csize << 10;
        } else { // SDC ver 1.XX or MMC ver 3 
          n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
          csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
          *(DWORD*)buff = csize << (n - 9);
        }
        res = RES_OK;
      }
    break;

    case GET_BLOCK_SIZE : // Get erase block size in unit of sector (DWORD) 
      if (CardType & CT_SD2) { // SDC ver 2.00 
        if (send_cmd(ACMD13, 0) == 0) { // Read SD status
          xchg_spi(0xFF);
          if (rcvr_datablock(csd, 16)) { // Read partial block 
            for (n = 64 - 16; n; n--) xchg_spi(0xFF);	// Purge trailing data 
            *(DWORD*)buff = 16UL << (csd[10] >> 4);
            res = RES_OK;
          }
        }
      } else { // SDC ver 1.XX or MMC 
        if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) { // Read CSD 
          if (CardType & CT_SD1) { // SDC ver 1.XX 
            *(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
          } else { // MMC 
            *(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
          }
          res = RES_OK;
        }
      }
    break;

    case CTRL_TRIM : // Erase a block of sectors (used when _USE_ERASE == 1) 
      if (!(CardType & CT_SDC)) break; // Check if the card is SDC 
      if (MMC_disk_ioctl(MMC_GET_CSD, csd)!=0) break; // Get CSD 
      if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break; // Check if sector erase can be applied to the card 
      dp = buff; st = dp[0]; ed = dp[1]; // Load sector block 
      if (!(CardType & CT_BLOCK)) {
        st *= 512; ed *= 512;
      }
      // Erase sector block 
      if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000))	     
        res = RES_OK; // FatFs does not check result of this command 
    break;

    default:
      res = RES_PARERR;
  }

  deselect();

  return res;
}
#endif


//--------------------------------------------------------------
// init aller GPIOs
//--------------------------------------------------------------
void init_gpio(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure;
  
  // Clock enable 
  #if USE_DETECT_PIN==1
    RCC_AHB1PeriphClockCmd(SD_DETECT_GPIO_CLK  | SD_SLAVESEL_GPIO_CLK, ENABLE);
  #else
    RCC_AHB1PeriphClockCmd(SD_SLAVESEL_GPIO_CLK, ENABLE);
  #endif
  
  // Config ChipSelect als Digital-Ausgang
  GPIO_InitStructure.GPIO_Pin = SD_SLAVESEL_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
  GPIO_Init(SD_SLAVESEL_GPIO_PORT, &GPIO_InitStructure);  
  
  #if USE_DETECT_PIN==1
    // Config CardDetect als Digital-Eingang (mit PullUp)
    GPIO_InitStructure.GPIO_Pin = SD_DETECT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(SD_DETECT_GPIO_PORT, &GPIO_InitStructure);   
  #endif  
}


//--------------------------------------------------------------
// timer init auf 1kHz
//--------------------------------------------------------------
void init_tim(void)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

  // Clock enable
  RCC_APB1PeriphClockCmd(SD_1MS_TIM_CLK, ENABLE);

  // Timer init
  TIM_TimeBaseStructure.TIM_Period =  SD_1MS_TIM_PERIODE;
  TIM_TimeBaseStructure.TIM_Prescaler = SD_1MS_TIM_PRESCALE;
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(SD_1MS_TIM, &TIM_TimeBaseStructure);

  // Timer enable
  TIM_ARRPreloadConfig(SD_1MS_TIM, ENABLE);
  TIM_Cmd(SD_1MS_TIM, ENABLE);
}


//--------------------------------------------------------------
// nvic init
//--------------------------------------------------------------
void init_nvic(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

  //---------------------------------------------
  // init vom Timer Interrupt
  //---------------------------------------------
  TIM_ITConfig(SD_1MS_TIM,TIM_IT_Update,ENABLE);

  // NVIC konfig
  NVIC_InitStructure.NVIC_IRQChannel = SD_1MS_TIM_IRQ;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}


//--------------------------------------------------------------
// spi init
// (set internal PullUp for MISO)
//--------------------------------------------------------------
void init_spi(void)
{ 
  // init spi
  UB_SPI2_Init(SD_SPI_MODE);
  
  #if USE_INTERNAL_MISO_PULLUP==1
  GPIO_InitTypeDef  GPIO_InitStructure;  
  
  // activate internal pullUp for MISO
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;  
  GPIO_InitStructure.GPIO_Pin = SD_SPI_MISO_PIN;
  GPIO_Init(SD_SPI_MISO_PORT, &GPIO_InitStructure);
  #endif
}


//--------------------------------------------------------------
// CS-Pin=Hi
//--------------------------------------------------------------
void CS_HIGH(void)
{
  // pin auf HI
  SD_SLAVESEL_GPIO_PORT->BSRRL = SD_SLAVESEL_PIN;  
}


//--------------------------------------------------------------
// CS-Pin=Lo
//--------------------------------------------------------------
void CS_LOW(void)
{
  // pin auf LO
  SD_SLAVESEL_GPIO_PORT->BSRRH = SD_SLAVESEL_PIN;  
}


//--------------------------------------------------------------
// SPI-Frq=Slow
//--------------------------------------------------------------
void FCLK_SLOW(void)
{
  uint16_t tmpreg = 0;

  tmpreg = SD_SPI_PORT->CR1; 
  tmpreg&=~0x38;
  tmpreg|=SD_SPI_CLK_SLOW;
  SD_SPI_PORT->CR1 = tmpreg;
}


//--------------------------------------------------------------
// SPI-Frq=Fast
//--------------------------------------------------------------
void FCLK_FAST(void)
{
  uint16_t tmpreg = 0;

  tmpreg = SD_SPI_PORT->CR1; 
  tmpreg&=~0x38;
  tmpreg|=SD_SPI_CLK_FAST;
  SD_SPI_PORT->CR1 = tmpreg;  
}


//--------------------------------------------------------------
// check ob Karte vorhanden
// ret_wert : 0=ohne Karte
//            1=mit Karte
//--------------------------------------------------------------
uint8_t SD_Detect(void)
{
  uint8_t ret_wert=1;
  
  #if USE_DETECT_PIN==1
    if (GPIO_ReadInputDataBit(SD_DETECT_GPIO_PORT, SD_DETECT_PIN) != Bit_RESET) {
      // keine Karte eingelegt
      ret_wert=0;  
    }
  #endif
  
  return(ret_wert);
}


//--------------------------------------------------------------
// Exchange a byte
// dat : Data to send
//--------------------------------------------------------------
static BYTE xchg_spi(BYTE dat)
{
  BYTE ret;
  UB_SPI2_SendAndReceiveMultipleBytes(&dat, 1, &ret, 1);
  return ret;
}

//--------------------------------------------------------------
// Receive multiple byte
// buff : Pointer to data buffer
// btr : Number of bytes to receive (even number)
//--------------------------------------------------------------
static void rcvr_spi_multi(BYTE *buff, UINT btr)
{
  UB_SPI2_ReceiveMultipleBytes(buff, btr);
}


//--------------------------------------------------------------
#if _USE_WRITE
// Send multiple byte
// buf : Pointer to the data
// btx : Number of bytes to send (even number)
//--------------------------------------------------------------
static void xmit_spi_multi(const BYTE *buff, UINT btx)
{	
  UB_SPI2_SendMultipleBytes(buff, btx);
}
#endif


//--------------------------------------------------------------
// Wait for card ready    
// wt :  Timeout [ms]
// ret_wert : 1:Ready, 0:Timeout
//--------------------------------------------------------------
static int wait_ready(UINT wt)
{
  BYTE d;

  Timer2 = wt;
  do {
    d = xchg_spi(0xFF);
    // This loop takes a time. Insert rot_rdq() here for multitask envilonment.
  } while (d != 0xFF && Timer2); // Wait for card goes ready or timeout

  return (d == 0xFF) ? 1 : 0;
}


//--------------------------------------------------------------
// Deselect card and release SPI           
//--------------------------------------------------------------
static void deselect(void)
{
  CS_HIGH(); // CS = H
  xchg_spi(0xFF); // Dummy clock (force DO hi-z for multiple slave SPI)
}


//--------------------------------------------------------------
// Select card and wait for ready  
// ret_wert :  1:OK, 0:Timeout 
//--------------------------------------------------------------
static int select(void)
{
  CS_LOW();
  xchg_spi(0xFF); // Dummy clock (force DO enabled)

  if (wait_ready(500)) return 1; // OK
  deselect();
  return 0; // Timeout
}


//--------------------------------------------------------------
// Receive a data packet from the MMC  
// buf : Data buffer
// btr : Data block length (byte)
// ret_wert : 1:OK, 0:Error 
//--------------------------------------------------------------
static int rcvr_datablock (BYTE *buff, UINT btr)
{
  BYTE token;

  Timer1 = 200;
  do { // Wait for DataStart token in timeout of 200ms
    token = xchg_spi(0xFF);
    // This loop will take a time. Insert rot_rdq() here for multitask envilonment.
  } while ((token == 0xFF) && Timer1);
  if(token != 0xFE) 
    return 0; // Function fails if invalid DataStart token or timeout

  rcvr_spi_multi(buff, btr); // Store trailing data to the buffer
  xchg_spi(0xFF); xchg_spi(0xFF); // Discard CRC

  return 1; // Function succeeded
}


//--------------------------------------------------------------
// Send a data packet to the MMC  
// buf : Ponter to 512 byte data to be sent
// token : Token
// ret_wert : 1:OK, 0:Failed
//--------------------------------------------------------------
#if _USE_WRITE
static int xmit_datablock(const BYTE *buff,BYTE token)
{
  BYTE resp;

  if (!wait_ready(500)) return 0; // Wait for card ready
  xchg_spi(token); // Send token
  if (token != 0xFD) { // Send data if token is other than StopTran
    xmit_spi_multi(buff, 512); // Data
    xchg_spi(0xFF); xchg_spi(0xFF); // Dummy CRC

    int n = 10; // Wait for response (10 bytes max)
    do {
      resp = xchg_spi(0xFF);
    } while ((resp == 0xFF) && --n);

    // Function fails if the data packet was not accepted
    if ((resp & 0x1F) != 0x05) return 0;
  }
  return 1;
}
#endif


//--------------------------------------------------------------
// Send a command packet to the MMC 
// cmd : Command index
// arg : Argument
// ret_wert : R1 resp (bit7==1:Failed to send)
//--------------------------------------------------------------
static BYTE send_cmd (BYTE cmd, DWORD arg)
{
  BYTE n, res;

  if (cmd & 0x80) { // Send a CMD55 prior to ACMD<n>
    cmd &= 0x7F;
    res = send_cmd(CMD55, 0);
    if (res > 1) return res;
  }

  // Select the card and wait for ready except to stop multiple block read
  if (cmd != CMD12) {
    deselect();
    if (!select()) return 0xFF;
  }

  // Send command packet
  xchg_spi(0x40 | cmd); // Start + command index
  xchg_spi((BYTE)(arg >> 24)); // Argument[31..24]
  xchg_spi((BYTE)(arg >> 16)); // Argument[23..16]
  xchg_spi((BYTE)(arg >> 8)); // Argument[15..8]
  xchg_spi((BYTE)arg); // Argument[7..0]
  n = 0x01; // Dummy CRC + Stop
  if (cmd == CMD0) n = 0x95; // Valid CRC for CMD0(0)
  if (cmd == CMD8) n = 0x87; // Valid CRC for CMD8(0x1AA)
  xchg_spi(n);

  // Receive command resp
  if (cmd == CMD12) xchg_spi(0xFF); // Diacard following one byte when CMD12
  n = 10; // Wait for response (10 bytes max)
  do {
    res = xchg_spi(0xFF);
  }while ((res & 0x80) && --n);

  return res; // Return received response
}

//--------------------------------------------------------------
// Send a command packet to the MMC, don't wait for the card to be ready (used for initialization)
// cmd : Command index
// arg : Argument
// ret_wert : R1 resp (bit7==1:Failed to send)
//--------------------------------------------------------------
static BYTE send_cmd_no_wait_ready (BYTE cmd, DWORD arg)
{
  BYTE n, res;

  // Send command packet
  xchg_spi(0x40 | cmd); // Start + command index
  xchg_spi((BYTE)(arg >> 24)); // Argument[31..24]
  xchg_spi((BYTE)(arg >> 16)); // Argument[23..16]
  xchg_spi((BYTE)(arg >> 8)); // Argument[15..8]
  xchg_spi((BYTE)arg); // Argument[7..0]
  n = 0x01; // Dummy CRC + Stop
  if (cmd == CMD0) n = 0x95; // Valid CRC for CMD0(0)
  if (cmd == CMD8) n = 0x87; // Valid CRC for CMD8(0x1AA)
  xchg_spi(n);

  // Receive command resp
  if (cmd == CMD12) xchg_spi(0xFF); // Diacard following one byte when CMD12
  n = 10; // Wait for response (10 bytes max)
  do {
    res = xchg_spi(0xFF);
  }while ((res & 0x80) && --n);

  return res; // Return received response
}


//--------------------------------------------------------------
// Device timer function     
// This function must be called from timer interrupt routine in period
// of 1 ms to generate card control timing.
//--------------------------------------------------------------
void disk_timerproc (void)
{
  WORD n;
  BYTE s;

  n = Timer1;
  if (n) Timer1 = --n;
  n = Timer2;
  if (n) Timer2 = --n;

  s = Stat;
  if (USE_WRITE_PROTECTION)
    s |= STA_PROTECT;
  else
    s &= ~STA_PROTECT;
  if (SD_Detect()!=0)
    s &= ~STA_NODISK;
  else
    s |= (STA_NODISK | STA_NOINIT);
  Stat = s;
}


//--------------------------------------------------------------
// ISR vom Timer
//--------------------------------------------------------------
void SD_1MS_TIM_ISR_HANDLER(void)
{
  // es gibt hier nur eine Interrupt Quelle
  TIM_ClearITPendingBit(SD_1MS_TIM, TIM_IT_Update);

  // funktion aufrufen
  disk_timerproc();
}
