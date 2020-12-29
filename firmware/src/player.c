#include "player.h"
#include "mp3dec.h"
#include "Audio.h"
#include <string.h>
#include <stdio.h>
#include "error.h"
#include "ff.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "statemachine.h"

#define STANDBY_MUTE_PORT     GPIOC
#define STANDBY_PIN           GPIO_Pin_8
#define MUTE_PIN              GPIO_Pin_9
#define STANDBY_MUTE_CLOCK    RCC_AHB1Periph_GPIOC

// local variables
#define FILE_READ_BUFFER_SIZE (8192*4)
MP3FrameInfo			mp3FrameInfo;
HMP3Decoder				hMP3Decoder;
FIL						file;
char					file_read_buffer[FILE_READ_BUFFER_SIZE];
volatile int			bytes_left;
char					*read_ptr;
char                    szArtist[120];
char                    szTitle[120];
uint32_t                bytesRead;
uint32_t 				numSamplesPlayed;

// directory browsing
#define PATH_BUFFER_SIZE 256
#define MAX_NUM_FILES 32
char                    dirPath[PATH_BUFFER_SIZE];
char                    fileList[MAX_NUM_FILES][PATH_BUFFER_SIZE];
char*                   sortedFileList[MAX_NUM_FILES];
int                     numFilesInList;
int                     currentFile;
DIR                     dir;

typedef enum {
    idle,
    playing,
	endOfFile,
    done
} pl_status_t;
pl_status_t status;

// Private function prototypes
static void AudioCallback(void *context,int buffer);
static uint32_t Mp3ReadId3V2Tag(FIL* pInFile, char* pszArtist,
		uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize);
void pl_playMp3(const char* filename);

// macros
#define f_tell(fp)		((fp)->fptr)

void pl_init()
{
    status = idle;

	// init clocks
	RCC_AHB1PeriphClockCmd(STANDBY_MUTE_CLOCK, ENABLE);
	// init standby and mute pins
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = STANDBY_PIN | MUTE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(STANDBY_MUTE_PORT, &GPIO_InitStructure);

	// enter standby & mute 
	GPIO_ResetBits(STANDBY_MUTE_PORT, STANDBY_PIN | MUTE_PIN);
}

bool pl_isPlaying()
{
    return (status == playing) || (status == endOfFile);
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

// reads all filesnames into the array so that they can be sorted
// sorts the filenames and starts playing the first one
// returns fals if no playable files were found in the directory
bool pl_playDirectory(const char* path)
{
	FRESULT res;

    // stop all sound
    pl_stop();

    // clear old file list
    for (int i = 0; i < MAX_NUM_FILES; i++)
        sortedFileList[i] = NULL;

    // open directory
	res = f_opendir(&dir, path);
	if (res != FR_OK)
    {
        error(eOtherDiscError);
        return false;
    }
    strncpy(dirPath, path, PATH_BUFFER_SIZE);

    // rewind to the start of the directory
    f_readdir(&dir, NULL);
    // iterate over the files
    int currFileIdx = 0;
    while (true)
    {
        FILINFO fno;
		#if _USE_LFN
		static char lfn[_MAX_LFN + 1];
		fno.lfname = lfn;
		fno.lfsize = sizeof lfn;
		#endif

        res = f_readdir(&dir, &fno);
        if (res != FR_OK)
        {
            error(eOtherDiscError);
            return false;
        }

        // on end of dir
        if (fno.fname[0] == 0)
        {
            break;
        }

        char *fn; /* This function is assuming non-Unicode cfg. */
        #if _USE_LFN
            fn = *fno.lfname ? fno.lfname : fno.fname;
        #else
            fn = fno.fname;
        #endif

        // check if this entry is a file to play
        if (fn[0] == '.')
            continue;
        else if (fno.fattrib & AM_DIR)
            continue;
        else
        {
            // Check if it is an mp3 file
            if ((strcmp("mp3", get_filename_ext(fn)) == 0) || (strcmp("MP3", get_filename_ext(fn)) == 0))
            {
                // append to the list
                strncpy(fileList[currFileIdx], fn, PATH_BUFFER_SIZE);
                // assign a pointer to the array of pointers that will later
                // be sorted
                sortedFileList[currFileIdx] = fileList[currFileIdx];
                currFileIdx++;
            }
        }
    }
    numFilesInList = currFileIdx;
    f_closedir(&dir);

	if (numFilesInList <= 0)
		return false;

    // now sort the files with the "simplesort" algorithm
    for (int targetIdx = numFilesInList-1; targetIdx >= 0; targetIdx--)
    {
        for (int probeIdx = 0; probeIdx < targetIdx; probeIdx++)
        {
            if (strcmp(sortedFileList[probeIdx], sortedFileList[targetIdx]) > 0)
            {
                char* tmp = sortedFileList[probeIdx];
                sortedFileList[probeIdx] = sortedFileList[targetIdx];
                sortedFileList[targetIdx] = tmp;
            }
        }
    }

    // start playing the first file
    currentFile = -1;
    pl_next(false);
	return true;
}

bool pl_next(bool allowRewind)
{
    if (numFilesInList <= 0)
        return false;

    // stop previously playing file
    pl_stop();

    currentFile++;
    if (currentFile >= numFilesInList)
    {
        if (allowRewind)
            currentFile = 0;
        else
            return false;
    }

    char buffer[2 * PATH_BUFFER_SIZE];
    sprintf(buffer, "%s/%s", dirPath, sortedFileList[currentFile]);
    // start playing
    pl_playMp3(buffer);
    return true;
}

bool pl_prev(bool allowRewind)
{
    if (numFilesInList <= 0)
        return false;

    // stop previously playing file
    pl_stop();

    currentFile--;
    if (currentFile < 0)
    {
        if (allowRewind)
            currentFile = numFilesInList - 1;
        else
            return false;
    }

    char buffer[2 * PATH_BUFFER_SIZE];
    sprintf(buffer, "%s/%s", dirPath, sortedFileList[currentFile]);
    // start playing
    pl_playMp3(buffer);
    return true;
}

void pl_stop()
{
    if (status == playing)
    {
        // Close currently open file
        f_close(&file);
    }

	// enter standby & mute 
	GPIO_ResetBits(STANDBY_MUTE_PORT, STANDBY_PIN | MUTE_PIN);

    status = idle;
    StopAudio();
    // Re-initialize and set volume to avoid noise
    InitializeAudio(Audio44100HzSettings);
    SetAudioVolume(0);
}

void pl_restart()
{
	if (!pl_isPlaying())
		return;

	char buffer[2 * PATH_BUFFER_SIZE];
    sprintf(buffer, "%s/%s", dirPath, sortedFileList[currentFile]);
    // start playing
    pl_playMp3(buffer);
}

uint32_t pl_getMsPlayedInCurrentFile()
{
	if (!pl_isPlaying())
		return 0;
	return (numSamplesPlayed * 10) / 441;
}

int16_t pl_getCurrentFileIndex() 
{
	if (!pl_isPlaying())
		return -1;
	return currentFile;
}


void pl_playMp3(const char* filename) {
	FRESULT res;

	numSamplesPlayed = 0;

	bytes_left = FILE_READ_BUFFER_SIZE;
	read_ptr = file_read_buffer;

	if (FR_OK == f_open(&file, filename, FA_OPEN_EXISTING | FA_READ)) {

		// Read ID3v2 Tag
		Mp3ReadId3V2Tag(&file, szArtist, sizeof(szArtist), szTitle, sizeof(szTitle));

		// Fill buffer
		res = f_read(&file, file_read_buffer, FILE_READ_BUFFER_SIZE, &bytesRead);
        if (res != FR_OK)
        {
            f_close(&file);
            pl_stop();
            return;
        }

		// unmute
		GPIO_SetBits(STANDBY_MUTE_PORT, STANDBY_PIN | MUTE_PIN);

		// Play mp3
		hMP3Decoder = MP3InitDecoder();
		InitializeAudio(Audio44100HzSettings);
		SetAudioVolume(0xAF);
		PlayAudioWithCallback(AudioCallback, 0);

        status = playing;
	}
    else
    {
        pl_stop();
    }
}

void pl_run()
{
    if (status == playing)
    {
        if (bytes_left < (FILE_READ_BUFFER_SIZE * 3 / 4))
        {
            /*
             * If past half of buffer, refill...
             *
             * When bytes_left changes, the audio callback has just been executed. This
             * means that there should be enough time to copy the end of the buffer
             * to the beginning and update the pointer before the next audio callback.
             * Getting audio callbacks while the next part of the file is read from the
             * file system should not cause problems.
             */

            // Copy rest of data to beginning of read buffer
            memcpy(file_read_buffer, read_ptr, bytes_left);

            // Update read pointer for audio sampling
            read_ptr = file_read_buffer;

            // Read next part of file
            unsigned int bytesToRead;
            bytesToRead = FILE_READ_BUFFER_SIZE - bytes_left;
			char* buffPtr = file_read_buffer + bytes_left;
            FRESULT res = f_read(&file, buffPtr, bytesToRead, &bytesRead);

            // Update the bytes left variable
            bytes_left = FILE_READ_BUFFER_SIZE;

            // error... Stop playback!
            if (res != FR_OK ) {
                pl_stop();
                return;
            }
            // out of data - end of file reached. Fill the rest of the buffer with zeros.
            if (bytesRead < bytesToRead)
            {
				for (uint32_t i = bytesRead; i < bytesToRead; i++)
					buffPtr[i] = 0;
                status = endOfFile;
            }
        }
    }
	else if (status == endOfFile)
	{
		// wait until the current buffer is all processed, then finish.
		if (bytes_left == 0)
		{
			status = done;
			// go to the next file on the folder, or finish if all files have been played.
			if (!pl_next(false))
				stateMachine_playbackFinished();
		}
	}
}

/*
 * Called by the audio driver when it is time to provide data to
 * one of the audio buffers (while the other buffer is sent to the
 * CODEC using DMA). One mp3 frame is decoded at a time and
 * provided to the audio driver.
 */
static void AudioCallback(void *context, int buffer) {
	static int16_t audio_buffer0[4096];
	static int16_t audio_buffer1[4096];

	int offset, err;
	int outOfData = 0;

	int16_t *samples;
	if (buffer) {
		samples = audio_buffer0;
	} else {
		samples = audio_buffer1;
	}

	// provide zeroes when out of data
	if (bytes_left <= 0)
	{
		const int numSamplesToProvide = sizeof(audio_buffer0);
		for (uint32_t i = 0; i < numSamplesToProvide; i++)
			samples[i] = 0;
		ProvideAudioBuffer(samples, numSamplesToProvide);
		return;
	}

	offset = MP3FindSyncWord((unsigned char*)read_ptr, bytes_left);
	bytes_left -= offset;
	read_ptr += offset;

	err = MP3Decode(hMP3Decoder, (unsigned char**)&read_ptr, (int*)&bytes_left, samples, 0);

	if (err) {
		/* error occurred */
		switch (err) {
		case ERR_MP3_INDATA_UNDERFLOW:
			outOfData = 1;
			// stop playback
			bytes_left = 0;
			break;
		case ERR_MP3_MAINDATA_UNDERFLOW:
			/* do nothing - next call to decode will provide more mainData */
			break;
		case ERR_MP3_FREE_BITRATE_SYNC:
		default:
			outOfData = 1;
			// stop playback
			bytes_left = 0;
			break;
		}
	} else {
		// no error
		MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

		// Duplicate data in case of mono to maintain playback speed
		if (mp3FrameInfo.nChans == 1) {
			for(int i = mp3FrameInfo.outputSamps;i >= 0;i--) 	{
				samples[2 * i]=samples[i];
				samples[2 * i + 1]=samples[i];
			}
			mp3FrameInfo.outputSamps *= 2;
		}
	}

	if (!outOfData) {
		numSamplesPlayed += mp3FrameInfo.outputSamps / mp3FrameInfo.nChans;
		ProvideAudioBuffer(samples, mp3FrameInfo.outputSamps);
	}
}

/*
 * Taken from
 * http://www.mikrocontroller.net/topic/252319
 */
static uint32_t Mp3ReadId3V2Text(FIL* pInFile, uint32_t unDataLen, char* pszBuffer, uint32_t unBufferSize)
{
	UINT unRead = 0;
	BYTE byEncoding = 0;
	if((f_read(pInFile, &byEncoding, 1, &unRead) == FR_OK) && (unRead == 1))
	{
		unDataLen--;
		if(unDataLen <= (unBufferSize - 1))
		{
			if((f_read(pInFile, pszBuffer, unDataLen, &unRead) == FR_OK) ||
					(unRead == unDataLen))
			{
				if(byEncoding == 0)
				{
					// ISO-8859-1 multibyte
					// just add a terminating zero
					pszBuffer[unDataLen] = 0;
				}
				else if(byEncoding == 1)
				{
					// UTF16LE unicode
					uint32_t r = 0;
					uint32_t w = 0;
					if((unDataLen > 2) && (pszBuffer[0] == 0xFF) && (pszBuffer[1] == 0xFE))
					{
						// ignore BOM, assume LE
						r = 2;
					}
					for(; r < unDataLen; r += 2, w += 1)
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
			if(f_lseek(pInFile, f_tell(pInFile) + unDataLen) != FR_OK)
			{
				return 1;
			}
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
static uint32_t Mp3ReadId3V2Tag(FIL* pInFile, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize)
{
	pszArtist[0] = 0;
	pszTitle[0] = 0;

	BYTE id3hd[10];
	UINT unRead = 0;
	if((f_read(pInFile, id3hd, 10, &unRead) != FR_OK) || (unRead != 10))
	{
		return 1;
	}
	else
	{
		uint32_t unSkip = 0;
		if((unRead == 10) &&
				(id3hd[0] == 'I') &&
				(id3hd[1] == 'D') &&
				(id3hd[2] == '3'))
		{
			unSkip += 10;
			unSkip = ((id3hd[6] & 0x7f) << 21) | ((id3hd[7] & 0x7f) << 14) | ((id3hd[8] & 0x7f) << 7) | (id3hd[9] & 0x7f);

			// try to get some information from the tag
			// skip the extended header, if present
			uint8_t unVersion = id3hd[3];
			if(id3hd[5] & 0x40)
			{
				BYTE exhd[4];
				f_read(pInFile, exhd, 4, &unRead);
				size_t unExHdrSkip = ((exhd[0] & 0x7f) << 21) | ((exhd[1] & 0x7f) << 14) | ((exhd[2] & 0x7f) << 7) | (exhd[3] & 0x7f);
				unExHdrSkip -= 4;
				if(f_lseek(pInFile, f_tell(pInFile) + unExHdrSkip) != FR_OK)
				{
					return 1;
				}
			}
			uint32_t nFramesToRead = 2;
			while(nFramesToRead > 0)
			{
				char frhd[10];
				if((f_read(pInFile, frhd, 10, &unRead) != FR_OK) || (unRead != 10))
				{
					return 1;
				}
				if((frhd[0] == 0) || (strncmp(frhd, "3DI", 3) == 0))
				{
					break;
				}
				char szFrameId[5] = {0, 0, 0, 0, 0};
				memcpy(szFrameId, frhd, 4);
				uint32_t unFrameSize = 0;
				uint32_t i = 0;
				for(; i < 4; i++)
				{
					if(unVersion == 3)
					{
						// ID3v2.3
						unFrameSize <<= 8;
						unFrameSize += frhd[i + 4];
					}
					if(unVersion == 4)
					{
						// ID3v2.4
						unFrameSize <<= 7;
						unFrameSize += frhd[i + 4] & 0x7F;
					}
				}

				if(strcmp(szFrameId, "TPE1") == 0)
				{
					// artist
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszArtist, unArtistSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else if(strcmp(szFrameId, "TIT2") == 0)
				{
					// title
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszTitle, unTitleSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else
				{
					if(f_lseek(pInFile, f_tell(pInFile) + unFrameSize) != FR_OK)
					{
						return 1;
					}
				}
			}
		}
		if(f_lseek(pInFile, unSkip) != FR_OK)
		{
			return 1;
		}
	}

	return 0;
}
