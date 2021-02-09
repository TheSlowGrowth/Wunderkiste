#pragma once
#include "Containers.h"

// =============================================================================
// Filesystem / SD card
// =============================================================================

class Filesystem
{
public:
    static bool mount();
};

// =============================================================================
// Systick
// =============================================================================

class Systick
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void systickCallback() = 0;
    };

    static void init();
    static uint32_t getMsCounter();
    static void delayMs(uint32_t milliseconds);
    static void addListener(Listener* l);
    static void removeListener(Listener* l);

    static StaticVector<Listener*, 10> listeners_;
};

// =============================================================================
// Power
// =============================================================================

class Power
{
public:
    static void initAndLatchOn();
    static void shutdownImmediately();
    static void enableOrResetAutoShutdownTimer();
    static void disableAutoShutdownTimer();
};

// =============================================================================
// Watchdog
// =============================================================================

class WatchdogTimer
{
public:
    enum class InitResult
    {
        resumedAfterWatchdogReset,
        startedAfterManualReset
    };

    static InitResult init();
    static void reset();
};