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

#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "error.h"
#include "library.h"

#define LIBRARYFILE "library.txt"
#define LINE_BUFFER_LEN 512
char lineBuffer[LINE_BUFFER_LEN];
const char hexChars[] = "0123456789ABCDEF";

void lib_init()
{
    // make sure library file exists
    FIL f;
    FRESULT res = f_open(&f, LIBRARYFILE, FA_OPEN_ALWAYS);
    if (res != FR_OK)
        error(eOtherDiscError);
    f_close(&f);
}

bool lib_getFolderForID(const uint8_t* ID, char* buffer, uint16_t bufflen)
{
    // open the library file
    FIL f;
    FRESULT res = f_open(&f, LIBRARYFILE, FA_OPEN_ALWAYS | FA_READ);
    if (res != FR_OK)
    {
        error(eOtherDiscError);
        f_close(&f);
        return false;
    }

    bool result = false;

    // read the file line by line and see if the ID is in the library
    while ((!f_eof(&f)) && (!result))
    {
        char* ret = f_gets(lineBuffer, LINE_BUFFER_LEN, &f);
        if (ret != lineBuffer)
            break;

        // compile an ID String fron the ID (32 bit hex + terminating zero)
        char IDStr[9] = { 0 };
        for (int i = 0; i < 4; i++)
        {
            IDStr[2 * i]     = hexChars[(ID[i]     ) & 0x0F];
            IDStr[2 * i + 1] = hexChars[(ID[i] >> 4) & 0x0F];
        }

        // start comparing the characters to the ID string
        char* searchStr = IDStr;
        while ((*ret) && (*ret != ':'))
        {
            if (*searchStr != *ret)
                break;
            searchStr++;
            ret++;
            // end of search string reached and they matched until here?
            // great, then copy the corresponding foldername
            if ((*searchStr == 0) && (*ret == ':'))
            {
                // skip the ':'
                ret++;
                // read the rest into the result string
                int i = 0;
                while ((*ret != '\n') && (*ret))
                {
                    // TODO: remove other unwanted characters
                    if (*ret != '\r')
                    {
                        buffer[i] = *ret;
                        i++;
                    }
                    ret++;
                    if (i >= bufflen - 1)
                        break;
                }
                // did everything fit into the result string?
                if (i <= bufflen - 1)
                {
                    buffer[i] = 0;
                    result = true;
                    break;
                }
            }
        }
    }

    f_close(&f);
    return result;
}

bool lib_isLinked(const char* folderName)
{
    // open the library file
    FIL f;
    FRESULT res = f_open(&f, LIBRARYFILE, FA_OPEN_ALWAYS | FA_READ);
    if (res != FR_OK)
    {
        error(eOtherDiscError);
        f_close(&f);
        return false;
    }

    bool result = false;

    // read the file line by line and see if the folder is in the library
    while (!f_eof(&f))
    {
        char* ret = f_gets(lineBuffer, LINE_BUFFER_LEN, &f);
        if (ret != lineBuffer)
            break;
        // find the ':' symbol (or the line end)
        while ((*ret != ':') && (*ret != 0))
            ret++;
        // skip the ':' symbol
        ret++;

        // start comparing the characters to the search string
        const char* searchStr = folderName;
        while ((*searchStr) && (*ret))
        {
            if (*searchStr != *ret)
                break;
            searchStr++;
            ret++;
            // end of search string reached and they matched until here?
            if (*searchStr == 0)
            {
                result = true;
                break;
            }
        }
    }

    f_close(&f);
    return result;
}

// returns the path of the next folder to which no RFID tag has been linked
bool lib_getNextUnlinked(char* buffer, uint16_t bufflen)
{
    // open the main directory to find folders
    DIR dir;
    FRESULT res = f_opendir(&dir, ""); /* Open the root directory */
	if (res != FR_OK)
        return false;

    bool success = false;

    // scan the folders until the first unlinked folder is found
    FILINFO fno;
    #if _USE_LFN
    static char lfn[_MAX_LFN + 1];
    fno.lfname = lfn;
    fno.lfsize = sizeof lfn;
    #endif
    for (;;) {
        res = f_readdir(&dir, &fno);                   /* Read a directory item */

        const char *filename; /* This function is assuming non-Unicode cfg. */
        #if _USE_LFN
            filename = *fno.lfname ? fno.lfname : fno.fname;
        #else
            filename = fno.fname;
        #endif

        if (res != FR_OK || filename[0] == 0) break;  /* Break on error or end of dir */
        if (fno.fattrib & AM_DIR) {                    /* It is a directory */
            // skip hidden folders
            if (filename[0] == '.')
                continue;
            // check if link is available
            if (!lib_isLinked(filename))
            {
                strncpy(buffer, filename, bufflen);
                success = true;
                break;
            }
        }
    }
    f_closedir(&dir);

    return success;
}

void lib_saveLink(const char* path, const uint8_t* ID)
{
    // open the library file
    FIL f;
    //FRESULT res = f_open(&f, LIBRARYFILE, FA_OPEN_APPEND | FA_WRITE);
    FRESULT res = f_open(&f, LIBRARYFILE, FA_OPEN_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        f_close(&f);
        error(eOtherDiscError);
        return;
    }
    // move write pointer to end of file so that we can append data
    f_lseek(&f, f_size(&f));

    int written = 0;
    // store the ID first
    for (int i = 0; i < 4; i++)
    {
        written += f_putc(hexChars[(ID[i]     ) & 0x0F], &f);
        written += f_putc(hexChars[(ID[i] >> 4) & 0x0F], &f);
    }
    // then add a ':'
    written += f_putc(':', &f);
    // then add the folder name
    written += f_puts(path, &f);
    // then add a newline character
    written += f_putc('\n', &f);

    if (written < 0)
        error(eOtherDiscError);

    f_close(&f);
}
