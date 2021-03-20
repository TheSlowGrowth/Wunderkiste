//--------------------------------------------------------------
// File     : stm32_ub_spi2.h
//--------------------------------------------------------------

//--------------------------------------------------------------
#ifndef __STM32F4_UB_SPI2_H
#define __STM32F4_UB_SPI2_H

//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_spi.h"



//--------------------------------------------------------------
// SPI-Mode
//--------------------------------------------------------------
typedef enum {
  SPI_MODE_0 = 0,  // CPOL=0, CPHA=0
  SPI_MODE_1,      // CPOL=0, CPHA=1
  SPI_MODE_2,      // CPOL=1, CPHA=0
  SPI_MODE_3       // CPOL=1, CPHA=1 
}SPI2_Mode_t;


//--------------------------------------------------------------
// SPI-Clock
// Grundfrequenz (SPI2)= APB1 (APB1=42MHz)
// M�gliche Vorteiler = 2,4,8,16,32,64,128,256
//--------------------------------------------------------------

//#define SPI2_VORTEILER     SPI_BaudRatePrescaler_2   // Frq = 21 MHz
//#define SPI2_VORTEILER     SPI_BaudRatePrescaler_4   // Frq = 10,5 MHz
//#define SPI2_VORTEILER     SPI_BaudRatePrescaler_8   // Frq = 5.25 MHz
//#define SPI2_VORTEILER     SPI_BaudRatePrescaler_16  // Frq = 2.625 MHz
//#define SPI2_VORTEILER     SPI_BaudRatePrescaler_32  // Frq = 1.3125 MHz
#define SPI2_VORTEILER     SPI_BaudRatePrescaler_64  // Frq = 656.2 kHz
//#define SPI2_VORTEILER     SPI_BaudRatePrescaler_128 // Frq = 328.1 kHz
//#define SPI2_VORTEILER     SPI_BaudRatePrescaler_256 // Frq = 164.0 kHz




//--------------------------------------------------------------
// Struktur eines SPI-Pins
//--------------------------------------------------------------
typedef struct {
  GPIO_TypeDef* PORT;     // Port
  const uint16_t PIN;     // Pin
  const uint32_t CLK;     // Clock
  const uint8_t SOURCE;   // Source
}SPI2_PIN_t; 

//--------------------------------------------------------------
// Struktur vom SPI-Device
//--------------------------------------------------------------
typedef struct {
  SPI2_PIN_t  SCK;        // SCK-Pin
  SPI2_PIN_t  MOSI;       // MOSI-Pin
  SPI2_PIN_t  MISO;       // MISO-Pin
}SPI2_DEV_t;


//--------------------------------------------------------------
// Globale Funktionen
//--------------------------------------------------------------
ErrorStatus UB_SPI2_Init(SPI2_Mode_t mode);
void UB_SPI2_SendMultipleBytes(const uint8_t* data, uint32_t size);
void UB_SPI2_ReceiveMultipleBytes(uint8_t* data, uint32_t size);
void UB_SPI2_SendAndReceiveMultipleBytes(uint8_t* txData, uint32_t txSize, uint8_t* rxData, uint32_t rxSize);




//--------------------------------------------------------------
#endif // __STM32F4_UB_SPI2_H
