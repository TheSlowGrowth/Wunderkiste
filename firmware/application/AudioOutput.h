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