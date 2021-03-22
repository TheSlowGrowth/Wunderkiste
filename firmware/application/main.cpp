/**	
 * Copyright (C) Johannes Elliesen, 2020
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

extern "C"
{
#include "stm32f4xx.h"
#include "core_cm4.h"
#include "stm32f4xx_conf.h"
}
#include "Platform.h"
#include "Library.h"
#include "File.h"
#include "Wunderkiste.h"
#include "RFID.h"
#include "UI.h"
#include "UiEventQueue.h"
#include <type_traits>
#include <memory>

using AudioStreamPlayerType = AudioStreamPlayer<WunderkisteAudioOutput>;
using Mp3DirectoryPlayerType = Mp3DirectoryPlayer<AudioStreamPlayerType>;

LateInitializedObject<AudioStreamPlayerType> streamPlayer;
LateInitializedObject<Mp3DirectoryPlayerType> mp3DirectoryPlayer;
LateInitializedObject<UiEventQueue> uiEventQueue;
LateInitializedObject<Wunderkiste> wunderkisteApp;

/**
 * Main function. Called when startup code is done with
 * copying memory and setting up clocks.
 */
int main(void)
{
    // initialize the platform
    Power::initAndLatchOn();
    WatchdogTimer::init();
    Systick::init();
    LED::init();
    const bool filesystemMounted = Filesystem::mount();

    uiEventQueue.create();

    RfidReader::init();
    ButtonScanner::init(*uiEventQueue); // debouncing executed via the systick interrupt

    streamPlayer.create();
    mp3DirectoryPlayer.create(*streamPlayer);
    wunderkisteApp.create(*uiEventQueue, *mp3DirectoryPlayer);

    while (1)
    {
        WatchdogTimer::reset();
        RfidReader::readAndGenerateEvents(*uiEventQueue);
        wunderkisteApp->handleEvents();
        streamPlayer->refillBuffers();
    }
}