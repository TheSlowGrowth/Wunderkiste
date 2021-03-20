#pragma once

#include "AudioStreamPlayer.h"
#include "File.h"

extern "C"
{
#include "mp3dec.h"
#include "string.h"
}

class Mp3FileStream : public StereoAudioSampleStream
{
public:
    Mp3FileStream() :
        isStreamInUse_(false)
    {
    }

    Mp3FileStream(const Mp3FileStream&) = delete;
    ~Mp3FileStream()
    {
        tearDownStream();
    }

    float getNumSecondsPlayed() const
    {
        if (currentSampleRate_ <= 0)
            return 0.0f;

        return float(numSamplesPlayed_) / float(currentSampleRate_);
    }

    bool isPlaying() const { return isStreamInUse_; }

    bool restartWithFile(const char* filePath)
    {
        file_ = filePath;
        setupStream();
        return isStreamInUse_;
    }

    void abortStream()
    {
        tearDownStream();
    }

    int getSampleRate() const override
    {
        return currentSampleRate_;
    }

    int fillBuffer(AudioSampleType* buffer, int bufferSize) override
    {
        // we're not actively used right now, who's calling this function?
        if (!isStreamInUse_)
            return 0;

        // fill output buffer...
        int totalNumSamplesProvided = 0;
        // ... and make sure we keep track of how many samples were played in total.
        ScopedAdder scopedAdder(numSamplesPlayed_, totalNumSamplesProvided);

        while (totalNumSamplesProvided < bufferSize)
        {
            // are there samples leftover from the last decoding frame?
            if (audioBufferTail_ < mp3FrameInfo_.outputSamps)
            {
                const auto numSamplesLeftInBuffer = mp3FrameInfo_.outputSamps - audioBufferTail_;
                const auto numSamplesLeftToTransfer = bufferSize - totalNumSamplesProvided;
                const auto numSamplesToCopyFromDecodeBuffer = std::min(numSamplesLeftToTransfer, numSamplesLeftInBuffer);
                int i = numSamplesToCopyFromDecodeBuffer;
                while (i--)
                {
                    *buffer++ = audioBuffer_[audioBufferTail_];
                    audioBufferTail_++;
                }
                totalNumSamplesProvided += numSamplesToCopyFromDecodeBuffer;
            }
            else
            {
                // No samples left from last frame; decode a new frame

                // if the end of the file was reached during the last file
                // read operation, then we're out of data and there's nothing
                // left to do.
                if (isEndOfFileReached_)
                {
                    tearDownStream();
                    // return what we have provided so far
                    return totalNumSamplesProvided;
                }

                // end of file not yet reached, read more.
                // if the file read buffer becomes too empty, read more from the file
                if (fileReadBufferNumBytesLeft_ < fileReadBufferSize_ / 2)
                {
                    if (!refillFileReadBuffer())
                    {
                        // stop on file read error
                        tearDownStream();
                        // return what we have provided so far
                        return totalNumSamplesProvided;
                    }
                }

                // decode what we've read from the file.
                int numSamplesDecoded = decodeNextFrameAndGetNumSamples();
                if (numSamplesDecoded <= 0)
                {
                    // stop on decoding error
                    tearDownStream();
                    // return what we have provided so far
                    return totalNumSamplesProvided;
                }
            }
        }

        return totalNumSamplesProvided;
    }

private:
    void setupStream()
    {
        if (isStreamInUse_)
            tearDownStream();

        // setup the decoder
        mp3Decoder_ = MP3InitDecoder();
        currentSampleRate_ = 0;
        numSamplesPlayed_ = 0;

        // open the file
        if (!file_.open(File::AccessMode::read, File::OpenMode::openIfExists))
        {
            // undo initialization and return
            isStreamInUse_ = false;
            MP3FreeDecoder(mp3Decoder_);
            return;
        }

        isEndOfFileReached_ = false;
        fileReadBufferTailPtr_ = &fileReadBuffer_[0];
        fileReadBufferNumBytesLeft_ = 0;

        // Read ID3v2 Tag
        mp3ReadId3V2Tag(file_, artist_.data(), artist_.maxSize(), title_.data(), title_.maxSize());
        artist_.updateSize(); // update after direct write access to data()
        title_.updateSize(); // update after direct write access to data()

        // fill the file read buffer
        if (!refillFileReadBuffer())
        {
            tearDownStream();
            return;
        }

        // decode the first frame so that the samplerate is accurately reported
        if (decodeNextFrameAndGetNumSamples() <= 0)
            tearDownStream();
        else
            isStreamInUse_ = true;
    }

    bool refillFileReadBuffer()
    {
        // Copy rest of data to the beginning of the read buffer
        memcpy(fileReadBuffer_, fileReadBufferTailPtr_, fileReadBufferNumBytesLeft_);
        // Reset read pointer to the start of the buffer
        fileReadBufferTailPtr_ = fileReadBuffer_;

        // Fill the rest of the buffer with more data from the file
        uint32_t bytesRead = 0;
        uint32_t numBytesToRead = fileReadBufferSize_ - fileReadBufferNumBytesLeft_;
        const bool fileReadResult = file_.tryRead(&fileReadBuffer_[fileReadBufferNumBytesLeft_], numBytesToRead, bytesRead);
        fileReadBufferNumBytesLeft_ += bytesRead;
        if (bytesRead < numBytesToRead)
            isEndOfFileReached_ = true;
        return fileReadResult;
    }

    int decodeNextFrameAndGetNumSamples()
    {
        // repeat until a valid frame is found and simply skip all invalid frames
        while (1)
        {
            // skip forward to next sync position
            auto offset = MP3FindSyncWord((unsigned char*) fileReadBufferTailPtr_, fileReadBufferNumBytesLeft_);
            if (offset < 0)
                // end of file maybe?
                return 0;

            fileReadBufferNumBytesLeft_ -= offset;
            if (fileReadBufferNumBytesLeft_ <= 0)
                return 0;
            fileReadBufferTailPtr_ += offset;

            // decode
            const auto err = MP3Decode(mp3Decoder_,
                                       (unsigned char**) &fileReadBufferTailPtr_,
                                       (int*) &fileReadBufferNumBytesLeft_,
                                       audioBuffer_,
                                       0);
            if (err)
            {
                // error occurred
                switch (err)
                {
                    case ERR_MP3_INDATA_UNDERFLOW:
                        //isStreamInUse_ = false;
                        break;
                    case ERR_MP3_MAINDATA_UNDERFLOW:
                        // do nothing - next call to decode will provide more mainData
                        break;
                    case ERR_MP3_FREE_BITRATE_SYNC:
                    default:
                        // stop reading.
                        //isStreamInUse_ = false;
                        break;
                }

                // advance to the next frame
                fileReadBufferNumBytesLeft_--;
                fileReadBufferTailPtr_++;
            }
            else
                // this was a valid frame, go on
                break;
        }

        // no error
        MP3GetLastFrameInfo(mp3Decoder_, &mp3FrameInfo_);
        currentSampleRate_ = mp3FrameInfo_.samprate;

        // Duplicate data in case of mono to maintain playback speed
        if (mp3FrameInfo_.nChans == 1)
        {
            for (int i = mp3FrameInfo_.outputSamps; i >= 0; i--)
            {
                audioBuffer_[2 * i] = audioBuffer_[i];
                audioBuffer_[2 * i + 1] = audioBuffer_[i];
            }
            mp3FrameInfo_.outputSamps *= 2;
        }

        audioBufferTail_ = 0;
        return mp3FrameInfo_.outputSamps;
    }

    void tearDownStream()
    {
        if (!isStreamInUse_)
            return; // nothing to do

        isStreamInUse_ = false;

        // teardown decoder
        MP3FreeDecoder(mp3Decoder_);

        // close file
        file_.close();
    }

    int mp3ReadId3V2Text(File& file, uint32_t unDataLen, char* pszBuffer, uint32_t unBufferSize);
    int mp3ReadId3V2Tag(File& file, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize);

    class ScopedAdder
    {
    public:
        ScopedAdder(int& destination, int& source) :
            destination_(destination),
            source_(source) {}
        ~ScopedAdder() { destination_ += source_; }
        int& destination_;
        int& source_;
    };

    // only one of these can actively read/decode a file, so it's fine
    // if we have these variables shared between instances
    static constexpr int fileReadBufferSize_ = 8192;
    static char fileReadBuffer_[fileReadBufferSize_];
    static char* fileReadBufferTailPtr_;
    static int fileReadBufferNumBytesLeft_;
    static MP3FrameInfo mp3FrameInfo_;
    static HMP3Decoder mp3Decoder_;
    static constexpr int audioBufferSize_ = MAX_NCHAN * MAX_NGRAN * MAX_NSAMP;
    static int16_t audioBuffer_[audioBufferSize_];
    static int audioBufferTail_;
    static int currentSampleRate_;
    static int numSamplesPlayed_;
    static FixedSizeStr<128> artist_;
    static FixedSizeStr<128> title_;

    bool isStreamInUse_;
    bool isEndOfFileReached_;
    File file_;
};