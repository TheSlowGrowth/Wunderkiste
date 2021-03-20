//--------------------------------------------------------------
// File     : stm32_ub_spi2.c
// Datum    : 04.03.2013
// Version  : 1.0
// Autor    : UB
// EMail    : mc-4u(@)t-online.de
// Web      : www.mikrocontroller-4u.de
// CPU      : STM32F4
// IDE      : CooCox CoIDE 1.7.0
// Module   : GPIO, SPI
// Funktion : SPI-LoLevel-Funktionen (SPI-2)
//
// Hinweis  : m�gliche Pinbelegungen
//            SPI2 : SCK :[PB10, PB13]
//                   MOSI:[PB15, PC3]
//                   MISO:[PB14, PC2]
//--------------------------------------------------------------

//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "stm32_ub_spi2.h"
#include "stm32f4xx.h"
#include "stdbool.h"

//--------------------------------------------------------------
// Definition von SPI2
//--------------------------------------------------------------
SPI2_DEV_t SPI2DEV = {
    // PORT , PIN       , Clock              , Source
    { GPIOB, GPIO_Pin_13, RCC_AHB1Periph_GPIOB, GPIO_PinSource13 }, // SCK an PB13
    { GPIOB, GPIO_Pin_15, RCC_AHB1Periph_GPIOB, GPIO_PinSource15 }, // MOSI an PB15
    { GPIOB, GPIO_Pin_14, RCC_AHB1Periph_GPIOB, GPIO_PinSource14 }, // MISO an PB14
};

void initSpi2Interrupt();

//--------------------------------------------------------------
// Init von SPI2
// Return_wert :
//  -> ERROR   , wenn SPI schon mit anderem Mode initialisiert
//  -> SUCCESS , wenn SPI init ok war
//--------------------------------------------------------------
ErrorStatus UB_SPI2_Init(SPI2_Mode_t mode)
{
    ErrorStatus ret_wert = ERROR;
    static uint8_t init_ok = 0;
    static SPI2_Mode_t init_mode;
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    // initialisierung darf nur einmal gemacht werden
    if (init_ok != 0)
    {
        if (init_mode == mode)
            ret_wert = SUCCESS;
        return (ret_wert);
    }

    // SPI-Clock enable
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    // Clock Enable der Pins
    RCC_AHB1PeriphClockCmd(SPI2DEV.SCK.CLK, ENABLE);
    RCC_AHB1PeriphClockCmd(SPI2DEV.MOSI.CLK, ENABLE);
    RCC_AHB1PeriphClockCmd(SPI2DEV.MISO.CLK, ENABLE);

    // SPI Alternative-Funktions mit den IO-Pins verbinden
    GPIO_PinAFConfig(SPI2DEV.SCK.PORT, SPI2DEV.SCK.SOURCE, GPIO_AF_SPI2);
    GPIO_PinAFConfig(SPI2DEV.MOSI.PORT, SPI2DEV.MOSI.SOURCE, GPIO_AF_SPI2);
    GPIO_PinAFConfig(SPI2DEV.MISO.PORT, SPI2DEV.MISO.SOURCE, GPIO_AF_SPI2);

    // SPI als Alternative-Funktion mit PullDown
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;

    // SCK-Pin
    GPIO_InitStructure.GPIO_Pin = SPI2DEV.SCK.PIN;
    GPIO_Init(SPI2DEV.SCK.PORT, &GPIO_InitStructure);
    // MOSI-Pin
    GPIO_InitStructure.GPIO_Pin = SPI2DEV.MOSI.PIN;
    GPIO_Init(SPI2DEV.MOSI.PORT, &GPIO_InitStructure);
    // MISO-Pin
    GPIO_InitStructure.GPIO_Pin = SPI2DEV.MISO.PIN;
    GPIO_Init(SPI2DEV.MISO.PORT, &GPIO_InitStructure);

    // SPI-Konfiguration
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    if (mode == SPI_MODE_0)
    {
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    }
    else if (mode == SPI_MODE_1)
    {
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    }
    else if (mode == SPI_MODE_2)
    {
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    }
    else
    {
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    }
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI2_VORTEILER;

    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);

    initSpi2Interrupt();

    // SPI enable
    SPI_Cmd(SPI2, ENABLE);

    // init Mode speichern
    init_ok = 1;
    init_mode = mode;
    ret_wert = SUCCESS;

    return (ret_wert);
}

volatile const uint8_t* interruptSendBuffer;
volatile int32_t numLeftToSend;
volatile uint8_t* interruptReceiveBuffer;
volatile int32_t numLeftToReceive;

void initSpi2Interrupt()
{
    // why even use interrupts at all, when we're polling
    // anyway?
    // We want the transmission to not be interrupted by any other
    // interrupt, like the audio callback. This could mess up the timing
    // for the SD card, leading to failed read/write operations.
    // By using interrupts, we can make sure that the transmission of a block
    // of bytes happens in one go without longer pauses.

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = SPI2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // receve interrupt should always be firing
    SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_RXNE, ENABLE);
}

void SPI2_IRQHandler()
{
    bool shouldKeepInterruptOn = (numLeftToSend > 1) || (numLeftToReceive > 1);

    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_TXE) == SET)
    {
        if (!shouldKeepInterruptOn)
            SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, DISABLE);
        // send next byte
        if (numLeftToSend > 0)
        {
            SPI_I2S_SendData(SPI2, *interruptSendBuffer++);
            numLeftToSend--;
        }
        // send dummy byte so that we can receive something
        else if (numLeftToReceive > 0)
            SPI_I2S_SendData(SPI2, 0xFF);
    }
    if (SPI_I2S_GetITStatus(SPI2, SPI_I2S_IT_RXNE) == SET)
    {
        if (numLeftToReceive > 0)
        {
            *interruptReceiveBuffer++ = SPI_I2S_ReceiveData(SPI2);
            numLeftToReceive--;
        }
        else
            // just clear the register but drop the data
            (void) SPI_I2S_ReceiveData(SPI2);
    }
}

void UB_SPI2_SendMultipleBytes(const uint8_t* data, uint32_t size)
{
    interruptReceiveBuffer = 0;
    numLeftToReceive = 0;
    interruptSendBuffer = data + 1;
    numLeftToSend = size - 1;
    // send the first byte
    SPI_I2S_SendData(SPI2, *data);
    // enable the transmit interrupt if more bytes are to come
    if (numLeftToSend > 0)
        SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, ENABLE);

    // wait for completion
    while (numLeftToSend > 0)
        ;
}

void UB_SPI2_ReceiveMultipleBytes(uint8_t* data, uint32_t size)
{
    interruptReceiveBuffer = data;
    numLeftToReceive = size;
    interruptSendBuffer = 0;
    numLeftToSend = 0;
    // send the first dummy byte
    SPI_I2S_SendData(SPI2, 0xFF);
    // enable the transmit interrupt to send more dummy bytes if required
    if (numLeftToReceive > 1)
        SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, ENABLE);

    // wait for completion
    while (numLeftToReceive > 0)
        ;
}

void UB_SPI2_SendAndReceiveMultipleBytes(uint8_t* txData, uint32_t txSize, uint8_t* rxData, uint32_t rxSize)
{
    interruptReceiveBuffer = rxData;
    numLeftToReceive = rxSize;
    interruptSendBuffer = txData + 1;
    numLeftToSend = txSize - 1;
    // send the first byte
    SPI_I2S_SendData(SPI2, *txData);
    // enable the transmit interrupt if more bytes are to come
    if ((numLeftToReceive > 1) || (numLeftToSend > 0))
        SPI_I2S_ITConfig(SPI2, SPI_I2S_IT_TXE, ENABLE);

    // wait for completion
    while ((numLeftToSend > 0) || (numLeftToReceive > 0))
        ;
}
