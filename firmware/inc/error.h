#ifndef ERROR_H_
#define ERROR_H_

#include <stdint.h>

typedef enum 
{
    eDiscErr = 0,
    eFSIntErr,
    eDiskNotReady,
    eNoFileOrPath,
    eInvalidFilename,
    eFileAccessDenied,
    eInvalidFileOrDirectory,
    eNoFilesystem,
    eOtherDiscError
} error_t;

void error(error_t e);

#endif // ifndef ERROR_H_
