#include "AudioFileStream.h"

// only one of these can actively read/decode a file, so it's fine
// if we have these variables shared between instances
char Mp3FileStream::fileReadBuffer_[Mp3FileStream::fileReadBufferSize_];
char* Mp3FileStream::fileReadBufferTailPtr_;
int Mp3FileStream::fileReadBufferNumBytesLeft_;
MP3FrameInfo Mp3FileStream::mp3FrameInfo_;
HMP3Decoder Mp3FileStream::mp3Decoder_;
int16_t Mp3FileStream::audioBuffer_[Mp3FileStream::audioBufferSize_];
int Mp3FileStream::audioBufferTail_;
int Mp3FileStream::currentSampleRate_;
int Mp3FileStream::numSamplesPlayed_;
FixedSizeStr<128> Mp3FileStream::artist_;
FixedSizeStr<128> Mp3FileStream::title_;

/*
 * Taken from
 * http://www.mikrocontroller.net/topic/252319
 */
int Mp3FileStream::mp3ReadId3V2Text(File& file, uint32_t unDataLen, char* pszBuffer, uint32_t unBufferSize)
{
    uint32_t unRead = 0;
    char byEncoding = 0;
    if (file.tryRead(&byEncoding, 1, unRead) && (unRead == 1))
    {
        unDataLen--;
        if (unDataLen <= (unBufferSize - 1))
        {
            if (file.tryRead(pszBuffer, unDataLen, unRead) || (unRead == unDataLen))
            {
                if (byEncoding == 0)
                {
                    // ISO-8859-1 multibyte
                    // just add a terminating zero
                    pszBuffer[unDataLen] = 0;
                }
                else if (byEncoding == 1)
                {
                    // UTF16LE unicode
                    uint32_t r = 0;
                    uint32_t w = 0;
                    if ((unDataLen > 2) && (pszBuffer[0] == 0xFF) && (pszBuffer[1] == 0xFE))
                    {
                        // ignore BOM, assume LE
                        r = 2;
                    }
                    for (; r < unDataLen; r += 2, w += 1)
                    {
                        // should be acceptable for 7 bit ascii
                        pszBuffer[w] = pszBuffer[r];
                    }
                    pszBuffer[w] = 0;
                }
            }
            else
            {
                return 1;
            }
        }
        else
        {
            // we won't read a partial text
            if (!file.advanceCursor(unDataLen))
                return 1;
        }
    }
    else
    {
        return 1;
    }
    return 0;
}

/*
 * Taken from
 * http://www.mikrocontroller.net/topic/252319
 */
int Mp3FileStream::mp3ReadId3V2Tag(File& file, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize)
{
    pszArtist[0] = 0;
    pszTitle[0] = 0;

    char id3hd[10];
    uint32_t unRead = 0;
    if (!file.tryRead(id3hd, 10, unRead) || (unRead != 10))
        return 1;
    else
    {
        uint32_t unSkip = 0;
        if ((unRead == 10) && (id3hd[0] == 'I') && (id3hd[1] == 'D') && (id3hd[2] == '3'))
        {
            unSkip += 10;
            unSkip = ((id3hd[6] & 0x7f) << 21) | ((id3hd[7] & 0x7f) << 14) | ((id3hd[8] & 0x7f) << 7) | (id3hd[9] & 0x7f);

            // try to get some information from the tag
            // skip the extended header, if present
            uint8_t unVersion = id3hd[3];
            if (id3hd[5] & 0x40)
            {
                char exhd[4];
                file.tryRead(exhd, 4, unRead);
                size_t unExHdrSkip = ((exhd[0] & 0x7f) << 21) | ((exhd[1] & 0x7f) << 14) | ((exhd[2] & 0x7f) << 7) | (exhd[3] & 0x7f);
                unExHdrSkip -= 4;
                if (!file.advanceCursor(unExHdrSkip))
                    return 1;
            }
            uint32_t nFramesToRead = 2;
            while (nFramesToRead > 0)
            {
                char frhd[10];
                if (!file.tryRead(frhd, 10, unRead) || (unRead != 10))
                    return 1;
                if ((frhd[0] == 0) || (strncmp(frhd, "3DI", 3) == 0))
                {
                    break;
                }
                char szFrameId[5] = { 0, 0, 0, 0, 0 };
                memcpy(szFrameId, frhd, 4);
                uint32_t unFrameSize = 0;
                uint32_t i = 0;
                for (; i < 4; i++)
                {
                    if (unVersion == 3)
                    {
                        // ID3v2.3
                        unFrameSize <<= 8;
                        unFrameSize += frhd[i + 4];
                    }
                    if (unVersion == 4)
                    {
                        // ID3v2.4
                        unFrameSize <<= 7;
                        unFrameSize += frhd[i + 4] & 0x7F;
                    }
                }

                if (strcmp(szFrameId, "TPE1") == 0)
                {
                    // artist
                    if (mp3ReadId3V2Text(file, unFrameSize, pszArtist, unArtistSize) != 0)
                    {
                        break;
                    }
                    nFramesToRead--;
                }
                else if (strcmp(szFrameId, "TIT2") == 0)
                {
                    // title
                    if (mp3ReadId3V2Text(file, unFrameSize, pszTitle, unTitleSize) != 0)
                    {
                        break;
                    }
                    nFramesToRead--;
                }
                else
                {
                    if (!file.advanceCursor(unFrameSize))
                        return 1;
                }
            }
        }
        if (!file.setCursorTo(unSkip))
        {
            return 1;
        }
    }

    return 0;
}