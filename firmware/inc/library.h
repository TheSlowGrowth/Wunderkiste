#ifndef LIBRARY_H_
#define LIBRARY_H_

#include <stdbool.h>

void lib_init();
// tries to find the folder name that is linked to an RFID tag 
// returns true, if the foldername was stored in the buffer 
// returns false if the buffer was too short or if no linked folder was found
bool lib_getFolderForID(const uint8_t* ID, char* buffer, uint16_t bufflen);
// finds the path of the next folder to which no RFID tag has been linked
// returns true, if the buffer was filled with the folder name,
// returns false if the buffer was too short or there was no unlinked folder
bool lib_getNextUnlinked(char* buffer, uint16_t bufflen);
// adds a new link to the library
void lib_saveLink(const char* path, const uint8_t* ID);

#endif // ifndef LIBRARY_H_
