#pragma once

#include "AudioOutput.h"
#include "UiEventQueue.h"
#include "Library.h"
#include "RFID.h"
#include "DirectoryPlayer.h"
#include "UI.h"
#include "Platform.h"

// used to inject "friend class" statements from a unit test fixture
#ifndef UNIT_TEST_FRIEND_CLASS
#    define UNIT_TEST_FRIEND_CLASS
#endif

class Wunderkiste
{
public:
    /** Application state */
    enum class State
    {
        /** Startup: Self-Check (start state) */
        startup,
        /** Unrecoverable error */
        unrecoverableError,
        /** Linking: Playing music from an unlinked folder and waiting for a tag to 
         *  be placed on the RFID reader. */
        linkWaitingForTag,
        /** Linking: A tag has been linked to a folder. Waiting for the tag to be 
         *  removed from the RFID reader before moving on. */
        linkSuccessfulWaitingForTagRemove,
        /** Linking: Linking the current tag to the folder failed, most likely because
         *  tha tag is already in use. Waiting for the tag to be removed from the RFID 
         *  reader before trying again. */
        linkErrorWaitingForTagRemove,
        /** Normal operation: Waiting for a tag to be placed on the RFID reader. */
        waitingForTag,
        /** Normal operation: Playing music from a folder as selected with an RFID tag. */
        playing,
        /** Normal operation: All music from the folder is played, waiting for the tag to be removed. */
        stoppedWaitingForTagRemove
    };

    Wunderkiste(UiEventQueue& eventQueue,
                DirectoryPlayer& player) :
        eventQueue_(eventQueue),
        player_(player),
        state_(State::startup)
    {
        Power::enableOrResetAutoShutdownTimer();
    }

    State getState() const { return state_; }

    void handleEvents()
    {
        const auto uiEvent = eventQueue_.popEvent();
        switch (state_)
        {
            case State::startup:
            {
                // check format of library file
                if (!library_.isLibraryFileValid())
                    transitionTo(State::unrecoverableError);
                else if (library_.getNextUnlinkedFolder(directoryToLink_))
                {
                    player_.startPlayingDirectory(directoryToLink_);
                    transitionTo(State::linkWaitingForTag);
                }
                else
                    transitionTo(State::waitingForTag);
            }
            break;
            case State::unrecoverableError:
            {
                // TODO: timeout!
            }
            break;
            case State::linkWaitingForTag:
            {
                if (uiEvent.type == UiEvent::Type::rfidTagAdded)
                {
                    const RfidTagId tagId = uiEvent.payload.asRfidTagId;
                    if (!library_.storeLink(tagId, directoryToLink_))
                        transitionTo(State::linkErrorWaitingForTagRemove);
                    else
                        transitionTo(State::linkSuccessfulWaitingForTagRemove);
                }
            }
            break;
            case State::linkErrorWaitingForTagRemove:
            {
                // there was an error while trying to link (e.g. RFID tag already in use
                // for another folder). Try again.
                if (uiEvent.type == UiEvent::Type::rfidTagRemoved)
                    transitionTo(State::linkWaitingForTag);
            }
            break;
            case State::linkSuccessfulWaitingForTagRemove:
            {
                if (uiEvent.type == UiEvent::Type::rfidTagRemoved)
                {
                    player_.stopPlaying();
                    if (library_.getNextUnlinkedFolder(directoryToLink_))
                    {
                        player_.startPlayingDirectory(directoryToLink_);
                        transitionTo(State::linkWaitingForTag);
                    }
                    else
                        transitionTo(State::waitingForTag);
                }
            }
            break;
            case State::waitingForTag:
            {
                if (uiEvent.type == UiEvent::Type::rfidTagAdded)
                {
                    Library::StringType directoryToPlay;
                    const RfidTagId tagId = uiEvent.payload.asRfidTagId;
                    if (library_.getFolderFor(tagId, directoryToPlay))
                    {
                        player_.startPlayingDirectory(directoryToPlay);
                        transitionTo(State::playing);
                    }
                }
            }
            break;
            case State::playing:
            {
                // disable the auto shutdown-timer so that Wunderkiste won't shut off
                // during playback
                Power::enableOrResetAutoShutdownTimer();

                if (uiEvent.type == UiEvent::Type::prevBttnPressed)
                    player_.goToPreviousTrack();
                else if (uiEvent.type == UiEvent::Type::nextBttnPressed)
                    player_.goToNextTrack();
                else if (uiEvent.type == UiEvent::Type::rfidTagRemoved)
                {
                    player_.stopPlaying();
                    transitionTo(State::waitingForTag);
                }
                // no more music to play
                else if (!player_.isPlaying())
                    transitionTo(State::stoppedWaitingForTagRemove);
            }
            break;
            case State::stoppedWaitingForTagRemove:
            {
                if (uiEvent.type == UiEvent::Type::rfidTagRemoved)
                    transitionTo(State::waitingForTag);
            }
            break;
        }
    }

private:
    void transitionTo(State newState)
    {
        if (state_ == newState)
            return;
        state_ = newState;
        switch (newState)
        {
            case State::startup:
                LED::setLed(LED::Pattern::off);
                break;
            case State::linkWaitingForTag:
                LED::setLed(LED::Pattern::linkWaitingForTag);
                break;
            case State::linkSuccessfulWaitingForTagRemove:
                LED::setLed(LED::Pattern::linkSuccessfulWaitingForTagRemove);
                break;
            case State::linkErrorWaitingForTagRemove:
                LED::setLed(LED::Pattern::linkErrorWaitingForTagRemove);
                break;
            case State::waitingForTag:
                LED::setLed(LED::Pattern::idle);
                break;
            case State::playing:
                LED::setLed(LED::Pattern::playing);
                break;
            case State::stoppedWaitingForTagRemove:
                LED::setLed(LED::Pattern::idle);
                break;
            default:
            case State::unrecoverableError:
                LED::setLed(LED::Pattern::errInternal);
                break;
        }
    }

    UNIT_TEST_FRIEND_CLASS;
    UiEventQueue& eventQueue_;
    DirectoryPlayer& player_;
    State state_;
    Library library_;
    Library::StringType directoryToLink_;
};

#if 0
#    include "AudioStreamPlayer.h"

class SawWaveStream : public StereoAudioSampleStream
{
public:
    SawWaveStream(float frequency, int sampleRate, float durationInS) :
        sampleRate_(sampleRate),
        phaseInc_(frequency / float(sampleRate)),
        totalNumSamples_(int(durationInS* sampleRate) * 2 /* stereo */),
        numSamplesLeft_(totalNumSamples_),
        phase_(0.0f)
    {
    }

    ~SawWaveStream() override {};

    bool isExhausted() const { return numSamplesLeft_ <= 0; }
    int getSampleRate() const override { return sampleRate_; }
    int fillBuffer(AudioSampleType* buffer, int bufferSize) override
    {
        int numSamplesProvided = 0;
        while (numSamplesProvided < bufferSize)
        {
            if (numSamplesLeft_ <= 0)
                break;

            // sample for the Left channel
            if (!(numSamplesLeft_ & 0x01))
            {
                lastSampleL_ = AudioSampleType(phase_ * 32768.0f);
                // mono to stereo interleaved
                *buffer++ = lastSampleL_;
                numSamplesProvided++;
                numSamplesLeft_--;
                // advance
                phase_ += phaseInc_;
                if (phase_ > 0.5f)
                    phase_ -= 1.0f;
            }
            else
            // sample for the Right channel
            {
                *buffer++ = lastSampleL_;
                numSamplesProvided++;
                numSamplesLeft_--;
            }
        }

        return numSamplesProvided;
    }

    void rewind()
    {
        numSamplesLeft_ = totalNumSamples_;
        phase_ = 0.0f;
    }

private:
    const int sampleRate_;
    const float phaseInc_;
    const int totalNumSamples_;
    int numSamplesLeft_;
    AudioSampleType lastSampleL_;
    float phase_;
};

class TestStreamProvider : public StreamProvider
{
public:
    TestStreamProvider() :
        counter_(0),
        stream1_(220.5f, 44100, 2.0f),
        stream2_(480.0f, 48000, 1.0f),
        nextToInsert_(&stream1_)
    {
    }

    StereoAudioSampleStream* getNextStream() override
    {
        auto* returnValue = nextToInsert_;
        counter_++;
        if (nextToInsert_ == &stream1_)
            nextToInsert_ = &stream2_;
        else
            nextToInsert_ = &stream1_;
        return returnValue;
    }

    void streamCompleted(StereoAudioSampleStream* streamThatWasCompleted) override
    {
        reinterpret_cast<SawWaveStream*>(streamThatWasCompleted)->rewind();
    }

    int counter_;

private:
    SawWaveStream stream1_, stream2_;
    SawWaveStream* nextToInsert_;
};
#endif