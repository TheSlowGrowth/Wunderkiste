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
