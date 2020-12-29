#ifndef __PLAYER_H__
#define __PLAYER_H__


#include <stdint.h>
#include <stdbool.h>

void pl_init();
bool pl_isPlaying();
bool pl_playDirectory(const char* path);
bool pl_next(bool allowRewind);
bool pl_prev(bool allowRewind);
void pl_stop();

void pl_run();

#endif // ifndef __PLAYER_H__
