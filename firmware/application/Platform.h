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