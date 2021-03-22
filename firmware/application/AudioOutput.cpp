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

#include "AudioOutput.h"

extern "C"
{
#include "DAC.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
}

#define STANDBY_MUTE_PORT GPIOC
#define STANDBY_PIN GPIO_Pin_8
#define MUTE_PIN GPIO_Pin_9
#define STANDBY_MUTE_CLOCK RCC_AHB1Periph_GPIOC

void WunderkisteAudioOutput::init()
{
    currentFormat_ = AudioFormat::invalid;
    callbackFunc_ = nullptr;
    callbackContext_ = nullptr;

    // init the amplifier mute / standby pins
    // init clocks
    RCC_AHB1PeriphClockCmd(STANDBY_MUTE_CLOCK, ENABLE);
    // init standby and mute pins
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = STANDBY_PIN | MUTE_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(STANDBY_MUTE_PORT, &GPIO_InitStructure);

    amplifierMute();
}

void WunderkisteAudioOutput::start(AudioFormat newAudioFormat,
                                   AudioStreamPlayerIsrCallbackPtr callbackWhenNewBufferMustBeProvided,
                                   void* callbackContext)
{
    callbackFunc_ = callbackWhenNewBufferMustBeProvided;
    callbackContext_ = callbackContext;

    if (newAudioFormat == currentFormat_)
        // no actual driver change is required
        return;

    currentFormat_ = newAudioFormat;
    switch (currentFormat_)
    {
        case AudioFormat::sr8000b16:
            InitializeAudio(Audio8000HzSettings);
            break;
        case AudioFormat::sr16000b16:
            InitializeAudio(Audio16000HzSettings);
            break;
        case AudioFormat::sr22050b16:
            InitializeAudio(Audio22050HzSettings);
            break;
        case AudioFormat::sr32000b16:
            InitializeAudio(Audio32000HzSettings);
            break;
        case AudioFormat::sr44100b16:
            InitializeAudio(Audio44100HzSettings);
            break;
        case AudioFormat::sr48000b16:
            InitializeAudio(Audio48000HzSettings);
            break;
        case AudioFormat::sr96000b16:
            InitializeAudio(Audio96000HzSettings);
            break;
        default:
        case AudioFormat::invalid:
            stop();
            return;
    }

    PlayAudioWithCallback(isrCallback, nullptr);
    AudioOn(); // enable DAC
    SetAudioVolume(0xAF);
    amplifierUnmute();
}

void WunderkisteAudioOutput::stop()
{
    currentFormat_ = AudioFormat::invalid;
    StopAudio();
    AudioOff();
    amplifierMute();
}

void WunderkisteAudioOutput::amplifierUnmute()
{
    // unmute
    GPIO_SetBits(STANDBY_MUTE_PORT, STANDBY_PIN | MUTE_PIN);
}

void WunderkisteAudioOutput::amplifierMute()
{
    // enter standby & mute
    GPIO_ResetBits(STANDBY_MUTE_PORT, STANDBY_PIN | MUTE_PIN);
}

void WunderkisteAudioOutput::isrCallback(void* /* context */, int bufferNumberToUse)
{
    AudioSampleType* buffer = (bufferNumberToUse == 0) ? audioBufferA_ : audioBufferB_;

    if (callbackFunc_)
        callbackFunc_(callbackContext_, buffer, bufferSize_);
    else
    {
        // fallback: provide zeros
        for (int i = 0; i < bufferSize_; i++)
            buffer[i] = 0;
    }

    ProvideAudioBuffer(buffer, bufferSize_);
}

AudioFormat WunderkisteAudioOutput::currentFormat_;
AudioStreamPlayerIsrCallbackPtr WunderkisteAudioOutput::callbackFunc_;
void* WunderkisteAudioOutput::callbackContext_;
AudioSampleType WunderkisteAudioOutput::audioBufferA_[WunderkisteAudioOutput::bufferSize_];
AudioSampleType WunderkisteAudioOutput::audioBufferB_[WunderkisteAudioOutput::bufferSize_];
