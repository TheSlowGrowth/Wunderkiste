#include "AudioStreamPlayer.h"
#include <gtest/gtest.h>
#include <deque>

// ==============================================================
// A dummy audio driver
// ==============================================================

class UnitTestAudioDriver
{
public:
    UnitTestAudioDriver()
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        instances_[testName] = this;
    }
    ~UnitTestAudioDriver()
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        instances_.erase(testName);
    }
    UnitTestAudioDriver(const UnitTestAudioDriver& other) = delete;

    static void init()
    {
        getInstance()->initCalled_ = getInstance()->callCounter_++;
    }
    static bool isRunning()
    {
        return getInstance()->isRunning_;
    }
    static void start(AudioFormat audioFormat,
                      AudioStreamPlayerIsrCallbackPtr callbackWhenBuffersMustBeProvided,
                      void* callbackContext)
    {
        getInstance()->startCalled_ = getInstance()->callCounter_++;
        getInstance()->audioFormatProvided_ = audioFormat;
        getInstance()->callback_ = callbackWhenBuffersMustBeProvided;
        getInstance()->callbackContext_ = callbackContext;
        getInstance()->isRunning_ = true;
    }
    static void stop()
    {
        getInstance()->stopCalled_ = getInstance()->callCounter_++;
    }
    static AudioFormat getCurrentAudioFormat()
    {
        return getInstance()->audioFormatProvided_;
    }

    int callCounter_ = 0; // tracks all calls (except getter-functions)
    int initCalled_ = -1;
    bool isRunning_ = false;
    int startCalled_ = -1;
    AudioFormat audioFormatProvided_ = AudioFormat::invalid;
    AudioStreamPlayerIsrCallbackPtr callback_ = nullptr;
    void* callbackContext_ = nullptr;
    int stopCalled_ = -1;

    static UnitTestAudioDriver* getInstance()
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        return instances_[testName];
    }
    static std::map<std::string, UnitTestAudioDriver*> instances_;
};

std::map<std::string, UnitTestAudioDriver*> UnitTestAudioDriver::instances_;

// ==============================================================
// A dummy StereoAudioSampleStream that plays N samples with values from 0..N-1
// ==============================================================

class DummyStream : public StereoAudioSampleStream
{
public:
    DummyStream(int numSamples, int sampleRate) :
        totalNumSamplesWritten_(0),
        numSamplesLeft_(numSamples),
        sampleRate_(sampleRate),
        completedCalled_(false)
    {
    }

    int getSampleRate() const override { return sampleRate_; }

    int fillBuffer(AudioSampleType* buffer, int bufferSize) override
    {
        int numWritten = 0;
        while (numWritten < bufferSize)
        {
            buffer[numWritten] = totalNumSamplesWritten_++;
            numWritten++;
            numSamplesLeft_--;
            if (numSamplesLeft_ <= 0)
                break;
        }
        return numWritten;
    }

    void completed() override
    {
        completedCalled_ = true;
    }

    int totalNumSamplesWritten_;
    int numSamplesLeft_;
    const int sampleRate_;
    bool completedCalled_;
};

// ==============================================================
// A dummy StreamProvider that plays a queue of streams
// ==============================================================

class DummyStreamProvider : public StreamProvider
{
public:
    StereoAudioSampleStream* getNextStream() override
    {
        if (streamsToPlay_.empty())
            return nullptr;
        auto* front = streamsToPlay_.front();
        streamsToPlay_.pop_front();
        return front;
    }

    /** Called when a stream is completed */
    void streamCompleted(StereoAudioSampleStream* streamThatWasCompleted) override
    {
        streamsCompleted_.push_back(streamThatWasCompleted);
    }

    std::deque<StereoAudioSampleStream*> streamsToPlay_;
    std::deque<StereoAudioSampleStream*> streamsCompleted_;
};

// ==============================================================
// Tests
// ==============================================================

class AudioStreamPlayer_Fixture : public ::testing::Test
{
protected:
    AudioStreamPlayer_Fixture() :
        player_()
    {
    }

    static constexpr int dacBufferSize_ = 256;
    AudioSampleType dummyDacBuffer_[dacBufferSize_];
    UnitTestAudioDriver dummyDriver_;
    DummyStreamProvider streamProvider_;
    AudioStreamPlayer<UnitTestAudioDriver> player_;
};

TEST_F(AudioStreamPlayer_Fixture, a_initialization)
{
    // init was called on the driver
    EXPECT_EQ(dummyDriver_.initCalled_, 0);
    // no other call was made into the driver
    EXPECT_EQ(dummyDriver_.callCounter_, 1);

    // all streams are invalid
    EXPECT_EQ(player_.getCurrentStream(), nullptr);
    EXPECT_FALSE(player_.isPlayingStream());
    EXPECT_FALSE(player_.isAboutToChangeAudioFormat());
}

TEST_F(AudioStreamPlayer_Fixture, b_refillBuffersWithNoStream)
{
    // call refillBuffers multiple times even though no stream is currently available.
    // Expect nothing to happen.

    // reset counter to see if calls are made into the audio driver.
    dummyDriver_.callCounter_ = 0;

    // just to be sure.
    player_.stopCurrentStream();
    EXPECT_FALSE(player_.isPlayingStream());

    player_.refillBuffers();
    player_.refillBuffers();
    player_.refillBuffers();

    // no calls were made into the driver
    EXPECT_EQ(dummyDriver_.callCounter_, 0);
}

TEST_F(AudioStreamPlayer_Fixture, c_addStreamAndStartAudioDriver)
{
    // Adds a stream and checks if the audio driver is started

    // reset counter to see if calls are made into the audio driver.
    dummyDriver_.callCounter_ = 0;

    DummyStream stream(100, 44100);
    streamProvider_.streamsToPlay_.push_back(&stream);
    player_.startPlayingNextStreamFrom(streamProvider_);

    EXPECT_EQ(player_.getCurrentStream(), &stream);
    EXPECT_TRUE(player_.isPlayingStream());

    // expect that audio driver was started
    EXPECT_EQ(dummyDriver_.startCalled_, 0); // player started driver
    EXPECT_EQ(dummyDriver_.audioFormatProvided_, AudioFormat::sr44100b16); // audio format is correct
    EXPECT_NE(dummyDriver_.callback_, nullptr); // callback was provided
    // no other calls made
    EXPECT_EQ(dummyDriver_.callCounter_, 1);
}

TEST_F(AudioStreamPlayer_Fixture, d_manuallyReplaceStreams)
{
    // Replaces a running stream with another one of the same
    // AudioFormat and checks if audio driver is kept running
    // seamlessly.
    // Replaces the stream with another stream of a different
    // AudioFormat and checks if the remaining samples are
    // playback back before the audio driver is reconfigured.

    // start a stream
    DummyStream stream1(100, 44100);
    streamProvider_.streamsToPlay_.push_back(&stream1);
    player_.startPlayingNextStreamFrom(streamProvider_);
    EXPECT_EQ(player_.getCurrentStream(), &stream1);

    // reset counter to see if calls are made into the audio driver.
    dummyDriver_.callCounter_ = 0;
    dummyDriver_.startCalled_ = -1;
    dummyDriver_.stopCalled_ = -1;

    // start a second stream with the same samplerate
    DummyStream stream2(100, 44100);
    streamProvider_.streamsToPlay_.push_back(&stream2);
    player_.startPlayingNextStreamFrom(streamProvider_);
    EXPECT_EQ(player_.getCurrentStream(), &stream2);
    // expect first stream to be unloaded immediately
    EXPECT_TRUE(stream1.completedCalled_);
    EXPECT_EQ(streamProvider_.streamsCompleted_.size(), size_t(1));
    EXPECT_EQ(streamProvider_.streamsCompleted_[0], &stream1);
    // expect audio driver to be left unchanged
    EXPECT_FALSE(player_.isAboutToChangeAudioFormat());
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);

    // start a third stream with a different samplerate
    DummyStream stream3(100, 48000);
    streamProvider_.streamsToPlay_.push_back(&stream3);
    player_.startPlayingNextStreamFrom(streamProvider_);
    EXPECT_EQ(player_.getCurrentStream(), &stream3);
    // expect second stream to be unloaded immediately
    EXPECT_TRUE(stream2.completedCalled_);
    EXPECT_EQ(streamProvider_.streamsCompleted_.size(), size_t(2));
    EXPECT_EQ(streamProvider_.streamsCompleted_[1], &stream2);

    // expect audio format change to be queued ...
    EXPECT_TRUE(player_.isAboutToChangeAudioFormat());
    // ... but not yet executed ...
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);

    // ... not even if we call refillBuffers()
    player_.refillBuffers();
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);

    // The audio format change is expected to be executed after the
    // remaining samples have been provided to the audio driver AND
    // have been fully played back to the audio output.
    // This means the player will have to provide another TWO blocks of
    // silence after the last block with the remaining audio samples
    // has been handed to the audio driver.

    // Request a block of samples by faking a callback from
    // the audio driver.
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);
    // Still, no change to the audio driver (which is still playing
    // valid samples on its output).
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);
    // Not even after we call refillBuffers()
    player_.refillBuffers();
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);

    // Request a second block of samples by faking a callback from
    // the audio driver.
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);
    // Now the player can safely change the audio settings. But
    // that should NOT happen from within the audio callback.
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);

    // Up until this point, no samples were requested from the stream.
    // Otherwise they would have already been played to the audio output
    // with the wrong samplerate.
    EXPECT_EQ(stream3.totalNumSamplesWritten_, 0);

    // It should happen during the next call to refillBuffers()
    player_.refillBuffers();
    // NOW we expect calls into the audio driver
    EXPECT_GE(dummyDriver_.startCalled_, 0); // player re-started driver with different samplerate
    EXPECT_EQ(dummyDriver_.audioFormatProvided_, AudioFormat::sr48000b16); // audio format is correct
    EXPECT_NE(dummyDriver_.callback_, nullptr); // callback was provided
    // audio driver was restarted without stopping it in-between.
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);

    // and finally, samples were requested from the third stream
    EXPECT_GT(stream3.totalNumSamplesWritten_, 0);
}

TEST_F(AudioStreamPlayer_Fixture, e_provideAudioSamples)
{
    // Checks if the player provides audio samples from the streams.
    // Checks that after one stream is exhausted, the next queued stream
    // is used instead.
    // Checks that after the second stream is exhausted and no other stream
    // is available, audio is turned off.

    // start two streams
    DummyStream stream1(100, 44100);
    DummyStream stream2(120, 44100);
    streamProvider_.streamsToPlay_.push_back(&stream1);
    streamProvider_.streamsToPlay_.push_back(&stream2);
    player_.startPlayingNextStreamFrom(streamProvider_);

    EXPECT_EQ(player_.getCurrentStream(), &stream1);
    EXPECT_TRUE(player_.isPlayingStream());

    // reset counter to see if calls are made into the audio driver.
    dummyDriver_.callCounter_ = 0;

    // Let player request samples from the streams.
    // This call should try to request as many samples as the streams
    // can provide and the internal ring buffer can hold.
    player_.refillBuffers();
    // The streams provided just 220 samples in total, this should fit into the
    // ring buffer entirely. Both streams should now be exhausted.
    EXPECT_EQ(stream1.numSamplesLeft_, 0);
    EXPECT_EQ(stream2.numSamplesLeft_, 0);
    // both streams are exhausted and were "unloaded" and notified
    EXPECT_TRUE(stream1.completedCalled_);
    EXPECT_TRUE(stream2.completedCalled_);
    EXPECT_EQ(streamProvider_.streamsCompleted_.size(), size_t(2));
    EXPECT_EQ(streamProvider_.streamsCompleted_[0], &stream1);
    EXPECT_EQ(streamProvider_.streamsCompleted_[1], &stream2);
    // AudioFormat is the same for stream1 and stream2, so this should not
    // have triggered any other interaction with the audio driver.
    EXPECT_FALSE(player_.isAboutToChangeAudioFormat());

    // reset counter
    dummyDriver_.callCounter_ = 0;

    // Now lets check if audio buffers are provided to the audio driver.
    // Request them by calling the callback.
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);
    // check the contents of the provided samples
    for (int i = 0; i < dacBufferSize_; i++)
    {
        if (i < 100)
            // these samples came from the first stream
            EXPECT_EQ(dummyDacBuffer_[i], i);
        else if (i < 100 + 120)
            // these samples came from the second stream
            EXPECT_EQ(dummyDacBuffer_[i], i - 100);
        else
            // the rest was filled up with zeros
            EXPECT_EQ(dummyDacBuffer_[i], 0);
    }
    // no calls to the driver from inside the callback
    EXPECT_EQ(dummyDriver_.callCounter_, 0);

    // Now that the streams are both exhausted and all samples
    // have already been passed to the audio driver, we'll
    // request another two blocks of samples.
    // We should receive two full blocks of zeros.
    // After that, the player knows for sure that all the non-zero
    // samples have been fully played on the audio output and it should
    // shut down the audo driver on the next call to refillBuffers().
    // (shutting down after the first buffer of zeros is too early, because
    // at that point, the previous buffer with actual samples is still playing
    // on the audio output.)

    dummyDriver_.stopCalled_ = -1;
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);
    // check the contents of these samples
    for (int i = 0; i < dacBufferSize_; i++)
        EXPECT_EQ(dummyDacBuffer_[i], 0);

    // call refillBuffers() once, just to see if the player
    // waits until the two full buffers have been played back.
    player_.refillBuffers();
    // no shutdown yet!
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);

    // request the second buffer
    dummyDriver_.stopCalled_ = -1;
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);

    // now we should see another full block of zeros.
    for (int i = 0; i < dacBufferSize_; i++)
        EXPECT_EQ(dummyDacBuffer_[i], 0);

    // now that the two blocks of "all-zeros" were provided to the
    // audio driver, the next "refillBuffers()" is expected to
    // shut down the audio driver because there's no more data to play
    // and all samples we had were already fully played back (which we can
    // guarantee because we have since provided two buffer of silence).

    player_.refillBuffers();
    // driver was stopped.
    EXPECT_GE(dummyDriver_.stopCalled_, 0);
}

TEST_F(AudioStreamPlayer_Fixture, f_provideAudioSamplesAndAutoSwitchSampleRate)
{
    // Checks if the player provides audio samples from the streams.
    // Checks that after one stream is exhausted, the next queued stream
    // is used instead and audio settings are adjusted accordingly.

    // start two streams
    DummyStream stream1(100, 44100);
    DummyStream stream2(120, 48000);
    streamProvider_.streamsToPlay_.push_back(&stream1);
    streamProvider_.streamsToPlay_.push_back(&stream2);
    player_.startPlayingNextStreamFrom(streamProvider_);

    EXPECT_EQ(player_.getCurrentStream(), &stream1);
    EXPECT_TRUE(player_.isPlayingStream());

    // reset counter to see if calls are made into the audio driver.
    dummyDriver_.callCounter_ = 0;

    // Let player request samples from the streams.
    // This call should try to request as many samples as the streams
    // can provide and the internal ring buffer can hold.
    player_.refillBuffers();
    // The second stream has a different samplerate. The player is
    // expected to complete playing all samples from stream1, then
    // reinitialize the audio driver and start playing stream2.
    // Thus: stream1 must be exhausted but stream2 is not yet touched
    EXPECT_EQ(stream1.numSamplesLeft_, 0);
    EXPECT_EQ(stream2.totalNumSamplesWritten_, 0);
    // when stream1 was exhausted, it was "unloaded" and notified
    EXPECT_TRUE(stream1.completedCalled_);
    EXPECT_FALSE(stream2.completedCalled_);
    EXPECT_EQ(streamProvider_.streamsCompleted_.size(), size_t(1));
    EXPECT_EQ(streamProvider_.streamsCompleted_[0], &stream1);
    // AudioFormat is different, so the change should be queued
    EXPECT_TRUE(player_.isAboutToChangeAudioFormat());

    // reset counter
    dummyDriver_.callCounter_ = 0;

    // Now lets check if audio buffers are provided to the audio driver.
    // Request them by calling the callback.
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);
    // check the contents of the provided samples
    for (int i = 0; i < dacBufferSize_; i++)
    {
        if (i < 100)
            // these samples came from the first stream
            EXPECT_EQ(dummyDacBuffer_[i], i);
        else
            // the rest was filled up with zeros, because the second stream
            // will be played after the audio driver was reinitialized to
            // the new samplerate
            EXPECT_EQ(dummyDacBuffer_[i], 0);
    }
    // no calls to the driver from inside the callback
    EXPECT_EQ(dummyDriver_.callCounter_, 0);

    // The player should now wait until all samples from stream1
    // have been played back on the audio output before it reconfigures
    // the audio driver to the new samplerate (otherwise audio from stream1
    // would be cut off before it's finished).
    // To make sure that all samples from stream1 are played back,
    // the player will have to wait for two more callbacks. On the first callback
    // the stream1-samples will have been provided to the DMA and start playing.
    // On the second callback, the samples from stream1 have finished playing.
    // Now the player should reinitialize the driver to the new samplerate.
    // While the player waits, it should provide blocks of zeros.

    // fake the first callback from the driver
    dummyDriver_.startCalled_ = -1;
    dummyDriver_.stopCalled_ = -1;
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);
    // check the contents of these samples
    for (int i = 0; i < dacBufferSize_; i++)
        EXPECT_EQ(dummyDacBuffer_[i], 0);

    // call refillBuffers() once, just to see if the player
    // waits until the two full buffers have been played back.
    player_.refillBuffers();
    // audio driver is untouched
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);
    // stream2 is also untouched
    EXPECT_EQ(stream2.totalNumSamplesWritten_, 0);

    // request the second buffer
    dummyDriver_.startCalled_ = -1;
    dummyDriver_.stopCalled_ = -1;
    dummyDriver_.callback_(dummyDriver_.callbackContext_, dummyDacBuffer_, dacBufferSize_);
    // we should see another full block of zeros.
    for (int i = 0; i < dacBufferSize_; i++)
        EXPECT_EQ(dummyDacBuffer_[i], 0);

    // audio driver is still untouched, because reconfiguring it
    // from INSIDE the callback is not allowed
    EXPECT_EQ(dummyDriver_.startCalled_, -1);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1);
    // stream2 is also untouched (again: that's not allowed from the callback)
    EXPECT_EQ(stream2.totalNumSamplesWritten_, 0);

    // now that the two blocks of "all-zeros" were provided to the
    // audio driver, the next "refillBuffers()" is expected to
    // reconfigure the driver and request the first samples from stream2

    player_.refillBuffers();
    EXPECT_GE(dummyDriver_.startCalled_, 0);
    EXPECT_EQ(dummyDriver_.stopCalled_, -1); // restart without stop
    // stream2 was used now
    EXPECT_GT(stream2.totalNumSamplesWritten_, 0);
    EXPECT_TRUE(stream2.completedCalled_);
    EXPECT_EQ(streamProvider_.streamsCompleted_.size(), size_t(2));
    EXPECT_EQ(streamProvider_.streamsCompleted_[1], &stream2);
}