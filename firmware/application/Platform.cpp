/**	
 * Copyright (C) Johannes Elliesen, 2021
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Platform.h"
extern "C"
{
#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx_rcc.h"
#include "ff.h"
}

// =============================================================================
// Error handlers and std library functions
// =============================================================================

extern "C" void __cxa_pure_virtual()
{
    while (1)
        ;
}

void operator delete(void*, unsigned int)
{
    while (1)
    {
        // this should never be actually called, but it's required when using virtual destructors.
        // even if we never actually use them.
        // See: https://stackoverflow.com/questions/39084764/compiling-c-without-the-delete-operator
    }
}

// =============================================================================
// Filesystem / SD card
// =============================================================================

static FATFS FatFs;
bool Filesystem::mount()
{
    FRESULT res = f_mount(&FatFs, "0:", 1);
    if (res == FR_OK)
        return true;

    // if filesystem is not ready yet, attampt once more, after a short delay
    int timeoutCounter = 0;
    while (res == FR_NOT_READY)
    {
        Systick::delayMs(100);
        res = f_mount(&FatFs, "0:", 1);
        timeoutCounter++;
        // abort after 1s
        if (timeoutCounter >= 10)
            return false;
    }

    return (res == FR_OK);
}

bool Filesystem::unmount()
{
    FRESULT res = f_unmount("0:");
    return (res == FR_OK);
}

// =============================================================================
// Systick
// =============================================================================

volatile uint32_t systickTicks;

extern "C" void SysTick_Handler(void)
{
    systickTicks++;
    for (size_t i = 0; i < Systick::listeners_.size(); i++)
        Systick::listeners_[i]->systickCallback();
}

void Systick::init()
{
    // init systick to 1ms
    RCC_ClocksTypeDef RCC_Clocks;
    RCC_GetClocksFreq(&RCC_Clocks);
    if (SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000))
    {
        while (1)
            ;
    }
    systickTicks = 0;
}

void Systick::addListener(Listener* l)
{
    if (!listeners_.contains(l))
        listeners_.add(l);
}

void Systick::removeListener(Listener* l)
{
    listeners_.findAndRemove(l);
}

uint32_t Systick::getMsCounter()
{
    return systickTicks;
}

void Systick::delayMs(uint32_t milliseconds)
{
    const auto start = systickTicks;
    while (systickTicks - start < milliseconds)
        ;
}

StaticVector<Systick::Listener*, 10> Systick::listeners_;

// =============================================================================
// Power
// =============================================================================

class AutoShutdownTimer : public Systick::Listener
{
public:
    AutoShutdownTimer() :
        isActive_(false),
        timeoutCounter_(timeoutCounterMaxMs_)
    {
    }

    void enableAndReset()
    {
        timeoutCounter_ = timeoutCounterMaxMs_;
        if (!isActive_)
        {
            Systick::addListener(this);
            isActive_ = true;
        }
    }

    void disable()
    {
        if (isActive_)
        {
            Systick::removeListener(this);
            isActive_ = false;
        }
    }

    void systickCallback() override
    {
        if (timeoutCounter_-- == 0)
            Power::shutdownImmediately();
    }

private:
    bool isActive_;
    uint32_t timeoutCounter_;
    static constexpr uint32_t timeoutCounterMaxMs_ = 30000; // 30s
};

LateInitializedObject<AutoShutdownTimer> autoShutdownTimer;

void Power::initAndLatchOn()
{
    // init the power pin that control the main power mosfet
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // turn on the mosfet to keep power latched while the
    // user releases the buttons
    GPIO_ResetBits(GPIOA, GPIO_Pin_1);

    // create auto shutdown timer instance (still inactive at this point)
    autoShutdownTimer.create();
}

void Power::shutdownImmediately()
{
    //unmount the filesystem
    Filesystem::unmount();
    // turn off the mosfet to cut power
    GPIO_SetBits(GPIOA, GPIO_Pin_1);
}

void Power::enableOrResetAutoShutdownTimer()
{
    autoShutdownTimer->enableAndReset();
}

void Power::disableAutoShutdownTimer()
{
    autoShutdownTimer->disable();
}

// =============================================================================
// Watchdog
// =============================================================================

WatchdogTimer::InitResult WatchdogTimer::init()
{
    InitResult result = InitResult::startedAfterManualReset;

    // Check if the system has resumed from IWDG reset
    if (RCC->CSR & RCC_CSR_WDGRSTF)
    {
        result = InitResult::resumedAfterWatchdogReset;
        // clear flags
        RCC->CSR |= RCC_CSR_RMVF;
    }

    // Enable write access to IWDG_PR and IWDG_RLR registers
    IWDG->KR = 0x5555;

    // Set clock: LSI/32 = 1024Hz
    IWDG->PR = 0x03;

    // Set reload == watchdog timeout
    // 2s @ 1024 Hz
    IWDG->RLR = 2047;

    // Reload IWDG counter
    IWDG->KR = 0xAAAA;

    // Enable IWDG (the LSI oscillator will be enabled by hardware)
    IWDG->KR = 0xCCCC;

    return result;
}

void WatchdogTimer::reset()
{
    IWDG->KR = 0xAAAA;
}

// =============================================================================
// Various Syscalls
// =============================================================================

extern "C" void HardFault_Handler()
{
    const uint32_t hardFaultReg = SCB->HFSR;
    const bool isDebugevtError = hardFaultReg & SCB_HFSR_DEBUGEVT_Msk;
    const bool isForcedError = hardFaultReg & SCB_HFSR_FORCED_Msk;
    const bool isVecttblError = hardFaultReg & SCB_HFSR_VECTTBL_Msk;

    // configurable faults that may have been escalated to a force fault
    const uint32_t confFaultStaturReg = SCB->CFSR;
    const uint32_t usageFaultStatusReg = (confFaultStaturReg & SCB_CFSR_USGFAULTSR_Msk) >> SCB_CFSR_USGFAULTSR_Pos;
    const uint32_t busFaultStatusReg = (confFaultStaturReg & SCB_CFSR_BUSFAULTSR_Msk) >> SCB_CFSR_BUSFAULTSR_Pos;
    const uint32_t memFaultStatusReg = (confFaultStaturReg & SCB_CFSR_MEMFAULTSR_Msk) >> SCB_CFSR_MEMFAULTSR_Pos;

    // configurable faults category: usage faults
    const bool isDivByZeroError = usageFaultStatusReg & (1UL << 9);
    const bool isUnalignedError = usageFaultStatusReg & (1UL << 8);
    const bool isNoCoProcessorError = usageFaultStatusReg & (1UL << 3);
    const bool isInvpcError = usageFaultStatusReg & (1UL << 2); // integrety check on EXC_RETURN
    const bool isInvstateError = usageFaultStatusReg & (1UL << 1);
    const bool isUndefInstrError = usageFaultStatusReg & (1UL << 0); // undefined instruction

    while (1)
    {
    }
}

extern "C" void MemManage_Handler()
{
    while (1)
    {
    }
}

extern "C" void BusFault_Handler()
{
    while (1)
    {
    }
}

extern "C" void UsageFault_Handler()
{
    while (1)
    {
    }
}