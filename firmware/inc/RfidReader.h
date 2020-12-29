#ifndef __RFID_READER_H__
#define __RFID_READER_H__

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t rfid_tagId_t[5];

void rfid_init();
bool rfid_readTag(rfid_tagId_t currentTagId);
bool rfid_tagIdsEqual(rfid_tagId_t tagId1, rfid_tagId_t tagId2);

#endif // ifndef __RFID_READER_H__