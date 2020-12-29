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

#ifndef __PLAYER_H__
#define __PLAYER_H__


#include <stdint.h>
#include <stdbool.h>

void pl_init();

/** returns true if playback is running. */
bool pl_isPlaying();

/** Starts playing all files in a directory in alphabetical order. */
bool pl_playDirectory(const char* path);
/** Starts playing the next file in the current directory */
bool pl_next(bool allowRewind);
/** Starts playing the previous file in the current directory */
bool pl_prev(bool allowRewind);
/** Restarts playback of the current file */
void pl_restart();
/** Stops playback */
void pl_stop();

/** Returns the time in ms that the current file has been playing (roughly). */
uint32_t pl_getMsPlayedInCurrentFile();

/** Returns the index of the file currently playing, or -1 if idle. */
int16_t pl_getCurrentFileIndex();

/** call regularly from the main loop */
void pl_run();

#endif // ifndef __PLAYER_H__
