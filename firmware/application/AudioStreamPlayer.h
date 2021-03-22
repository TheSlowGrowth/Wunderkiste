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

#include "stdint.h"
#include "LockFreeFifo.h"

using AudioSampleType = int16_t;

/** A stream of stereo LR-interleaved audio samples. */
class StereoAudioSampleStream
{
public:
    virtual ~StereoAudioSampleStream() = default;

    /** Returns the samplerate of the audio samples provided by this stream */
    virtual int getSampleRate() const = 0;

    /** Fills the provided interleaved buffer with new audio samples and returns the number of samples written. 
     *  A return value smaller than "bufferSize" indicates that the stream is exhausted. After that, no 
     *  more calls to fillBuffer() will occur. */
    virtual int fillBuffer(AudioSampleType* buffer, int bufferSize) = 0;

    /** Called when the stream is removed from the playback engine and is no longer used. */
    virtual void completed() {};
};

/** Valid audio formats. */
enum class AudioFormat
{
    invalid,
    sr8000b16,
    sr16000b16,
    sr22050b16,
    sr32000b16,
    sr44100b16,
    sr48000b16,
    sr96000b16,
};

/** Provides a playlist of StereoAudioSampleStreams */
class StreamProvider
{
public:
    virtual ~StreamProvider() = default;

    /** Called to request the next stream to play.
     *  @return the next stream to play (ownership is not taken), 
     *          or nullptr if the playlist reached its end. 
     */
    virtual StereoAudioSampleStream* getNextStream() = 0;

    /** Called when a stream is completed */
    virtual void streamCompleted(StereoAudioSampleStream* streamThatWasCompleted) = 0;
};

/** A callback function called from the audio driver to request a new block of samples */
using AudioStreamPlayerIsrCallbackPtr = void (*)(void* context, AudioSampleType* bufferToFill, const int bufferSize);

/**
 *  @brief  Feeds an audio output with samples from a StereoAudioSampleStream; switches to the next
 *          stream once the current stream is exhausted. Feeds zeros to the audio output when no
 *          stream is available to provide new data. Reconfigures the audio driver when the 
 *          samplerate changes between streams.
 * 
 *  @tparam AudioDriverType provides the audio output driver and must implement:
 *          \code{.cpp}
 *              // initializes the hardware but doesnÄt start the audio output
 *              static void init();
 *              // returns true if the audio output is running
 *              static bool isRunning();
 *              // Starts the audio output in the desired format; this will start generating
 *              // callbacks to the provided function whenever a new block of samples
 *              // is required. Restarting in a different format while audio is running
 *              // must be supported.
 *              static void start(AudioFormat audioFormat, 
 *                                AudioStreamPlayerIsrCallbackPtr callbackWhenNewBufferMustBeProvided, 
 *                                void* callbackContext);
 *              // stops the audio output so that no more callbacks are triggered.
 *              static void stop();
 *              // returns the AudioFormat in use, or AudioFormat::invalid if audio is stopped.
 *              static AudioFormat getCurrentAudioFormat();
 *          \endcode
 *          None of these functions will ever be called from inside a callback to the 
 *          function provided in start().
 */
template <typename AudioDriverType>
class AudioStreamPlayer
{
public:
    AudioStreamPlayer() :
        streamProvider_(nullptr),
        currentStream_(nullptr),
        numBlocksOfSilenceProvided_(0),
        clearBufferForFormatChange_(false)
    {
        AudioDriverType::init();
    }

    bool isPlayingStream() const { return currentStream_ != nullptr; }
    const StereoAudioSampleStream* getCurrentStream() const { return currentStream_; }

    /** Requests the next stream from the StreamProvider and starts playing it back.
     *  This operation will cancel any stream that was playing before or start the
     *  audio driver if it was turned off. */
    void startPlayingNextStreamFrom(StreamProvider& streamProvider)
    {
        stopCurrentStream();
        streamProvider_ = &streamProvider;
        auto* nextStream = streamProvider_->getNextStream();
        if (nextStream)
            setStream(nextStream);
    }

    /** When the samplerate of the audio output must be changed (e.g. when switching
     *  from one stream to another one) the AudioStreamPlayer will wait until the remaining
     *  samples have been playback back on the audio device. During this time, this function
     *  returns true. Afterwards, the new audio settings are applied.
     */
    bool isAboutToChangeAudioFormat() const { return clearBufferForFormatChange_; }

    /** Stops playing the current stream.
     *  This will finish playing all samples that were already requested 
     *  from the current stream and eventually shut down the audio driver.
     */
    void stopCurrentStream()
    {
        if (currentStream_)
        {
            currentStream_->completed();
            if (streamProvider_)
                streamProvider_->streamCompleted(currentStream_);
        }
        currentStream_ = nullptr;
    }

    /** Stops playing the current stream and shuts off the audio
     *  driver immediately 
     */
    void stopCurrentStreamAndTurnOffImmediately()
    {
        stopCurrentStream();
        if (AudioDriverType::isRunning())
            AudioDriverType::stop();
    }

    /** Must be called in regular intervals to request new data from 
     *  the current stream and refill its buffers.
     */
    void refillBuffers()
    {
        // Due to the double-buffering in the audio driver, we must have provided TWO
        // full buffers of zeros before the last non-zero audio sample has been played
        // on the audio output. (We provide the first buffer of zeros while the audio
        // is still playing from the previous buffer. When we provide the second buffer
        // of zeros we know for sure that our first bffer of zeros is now played on the
        // audio output.)
        bool bufferClearedAndContentsFullyPlayedBack = (numBlocksOfSilenceProvided_ >= 2);

        // nothing to fill the fifo with && fifo has been cleared?
        // => We can shutdown the driver.
        if (!currentStream_ && bufferClearedAndContentsFullyPlayedBack)
            stopCurrentStreamAndTurnOffImmediately();

        // we're waiting for the buffer to clear so that we can change the audio settings.
        if (clearBufferForFormatChange_)
        {
            if (!bufferClearedAndContentsFullyPlayedBack)
                return;
            else
            {
                const auto formatToUse = getFormatRequiredForStream(currentStream_);
                AudioDriverType::start(formatToUse, isrCallback, this);
                numBlocksOfSilenceProvided_ = 0;
                clearBufferForFormatChange_ = false;
            }
        }

        // fill the fifo
        while (!fifo_.isFull())
        {
            if (!currentStream_)
                break;

            int maxNumToRefill = fifo_.getNumFree();

            AudioSampleType* block1 = nullptr;
            int blockSize1 = 0;
            AudioSampleType* block2 = nullptr;
            int blockSize2 = 0;

            // fill as much as we can from the fifo ...
            fifo_.prepareWrite(maxNumToRefill, block1, blockSize1, block2, blockSize2);
            int numWritten = 0;
            bool streamExhausted = false;
            if (blockSize1 > 0)
            {
                const int numWrittenToBlock1 = currentStream_->fillBuffer(block1, blockSize1);
                if (numWrittenToBlock1 != blockSize1)
                    streamExhausted = true;
                numWritten = numWrittenToBlock1;
            }
            if ((blockSize2 > 0) && !streamExhausted)
            {
                const int numWrittenToBlock2 = currentStream_->fillBuffer(block2, blockSize2);
                if (numWrittenToBlock2 != blockSize2)
                    streamExhausted = true;
                numWritten += numWrittenToBlock2;
            }
            fifo_.finishWrite(numWritten);

            // if the current stream is now exhausted, try filling the rest with the next stream
            if (streamExhausted)
            {
                // start the next stream, if there's one available
                if (streamProvider_)
                    startPlayingNextStreamFrom(*streamProvider_);

                // avoid continuing to fill the fifo when
                // a sample rate change is queued
                if (clearBufferForFormatChange_)
                    return;
            }
        }
    }

private:
    void setStream(StereoAudioSampleStream* streamToPlay)
    {
        if (currentStream_)
            stopCurrentStream();

        currentStream_ = streamToPlay;
        clearBufferForFormatChange_ = false;

        const auto formatToUse = getFormatRequiredForStream(currentStream_);
        if (formatToUse == AudioFormat::invalid)
        {
            stopCurrentStreamAndTurnOffImmediately();
            return;
        }

        if (!AudioDriverType::isRunning())
            AudioDriverType::start(formatToUse, isrCallback, this);
        else if (AudioDriverType::getCurrentAudioFormat() != formatToUse)
            // Audio is already running but we need to change the audio settings.
            // Clear the Fifo, then reconfigure the audio driver before we start playing the stream.
            clearBufferForFormatChange_ = true;
    }

    AudioFormat getFormatRequiredForStream(StereoAudioSampleStream* stream)
    {
        if (stream->getSampleRate() == 8000)
            return AudioFormat::sr8000b16;
        else if (stream->getSampleRate() == 16000)
            return AudioFormat::sr16000b16;
        else if (stream->getSampleRate() == 22050)
            return AudioFormat::sr22050b16;
        else if (stream->getSampleRate() == 32000)
            return AudioFormat::sr32000b16;
        else if (stream->getSampleRate() == 44100)
            return AudioFormat::sr44100b16;
        else if (stream->getSampleRate() == 48000)
            return AudioFormat::sr48000b16;
        else if (stream->getSampleRate() == 96000)
            return AudioFormat::sr96000b16;
        else
            return AudioFormat::invalid;
    }

    static void isrCallback(void* context, AudioSampleType* bufferToFill, const int bufferSize)
    {
        AudioStreamPlayer* player = (AudioStreamPlayer*) context;

        auto numSamplesToWrite = bufferSize;

        AudioSampleType* block1 = nullptr;
        int blockSize1 = 0;
        AudioSampleType* block2 = nullptr;
        int blockSize2 = 0;

        // fill as much as we can from the fifo ...
        player->fifo_.prepareRead(numSamplesToWrite, block1, blockSize1, block2, blockSize2);
        int numSamplesTakenFromFifo = blockSize1 + blockSize2;
        while (blockSize1-- > 0)
        {
            *bufferToFill = *block1;
            bufferToFill++;
            block1++;
        }
        while (blockSize2-- > 0)
        {
            *bufferToFill = *block2;
            bufferToFill++;
            block2++;
        }
        numSamplesToWrite -= numSamplesTakenFromFifo;
        player->fifo_.finishRead(numSamplesTakenFromFifo);

        // ... fill the rest with zeros
        while (numSamplesToWrite-- > 0)
            *(bufferToFill++) = 0;

        // update flag that allows us to safely shutdown the DAC after all audio has been played
        if (numSamplesTakenFromFifo == 0)
            player->numBlocksOfSilenceProvided_++;
        else
            player->numBlocksOfSilenceProvided_ = 0;
    }

    StreamProvider* streamProvider_;
    StereoAudioSampleStream* currentStream_;
    int numBlocksOfSilenceProvided_;
    bool clearBufferForFormatChange_;
    static constexpr int fifoSize_ = 0x3FFF;
    LockFreeFifo<AudioSampleType, fifoSize_> fifo_;
};