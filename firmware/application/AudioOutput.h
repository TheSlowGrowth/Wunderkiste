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

#include "AudioStreamPlayer.h"

class WunderkisteAudioOutput
{
public:
    static void init();
    static bool isRunning() { return currentFormat_ != AudioFormat::invalid; }
    static void start(AudioFormat audioFormat,
                      AudioStreamPlayerIsrCallbackPtr callbackWhenNewBufferMustBeProvided,
                      void* callbackContext);
    static void stop();
    static AudioFormat getCurrentAudioFormat() { return currentFormat_; }

private:
    static void isrCallback(void* context, int bufferNumberToUse);
    static void amplifierMute();
    static void amplifierUnmute();

    static AudioFormat currentFormat_;
    static AudioStreamPlayerIsrCallbackPtr callbackFunc_;
    static void* callbackContext_;
    static constexpr int bufferSize_ = 512;
    static AudioSampleType audioBufferA_[bufferSize_];
    static AudioSampleType audioBufferB_[bufferSize_];
    static AudioSampleType* bufferBeingTransmitted_;
    static AudioSampleType* bufferBeingPrepared_;
};