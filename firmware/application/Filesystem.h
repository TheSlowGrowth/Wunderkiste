#pragma once

extern "C" 
{
    #include "ff.h"
    #include "string.h"
}

class Filesystem 
{
public:
    static bool mount() 
    {
        FATFS FatFs;
	    FRESULT res = f_mount(&FatFs, "0:", 1);
	    return (res == FR_OK);
    }
};