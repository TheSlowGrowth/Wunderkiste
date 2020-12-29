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
