#define STM32F4xx

#define TM_SPI1_PRESCALER   SPI_BaudRatePrescaler_8

/* Use SPI (SDIO pins used by DAC already) */
#define FATFS_USE_SDIO                0

#define USE_SPI2
#define FATFS_SPI							  SPI2
#define FATFS_SPI_PINSPACK		  TM_SPI_PinsPack_2 //PB15,14,13
#define FATFS_CS_PORT						GPIOB
#define FATFS_CS_PIN						GPIO_PIN_12

#define TM_SPI2_PRESCALER    SPI_BaudRatePrescaler_2
//Specify datasize
#define TM_SPI2_DATASIZE     SPI_DataSize_8b
//Specify which bit is first
#define TM_SPI2_FIRSTBIT     SPI_FirstBit_MSB
//Mode, master or slave
#define TM_SPI2_MASTERSLAVE SPI_Mode_Master
//Specify mode of operation, clock polarity and clock phase
//Modes 0 to 3 are possible
#define TM_SPI2_MODE        TM_SPI_Mode_0
