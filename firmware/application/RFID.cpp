/**	
 * Copyright (C) Johannes Elliesen, 2021
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

#include "RFID.h"
extern "C"
{
#include "tm_stm32f4_mfrc522.h"
}
#include "Platform.h"

#define RFID_RESET_PORT_CLK RCC_AHB1Periph_GPIOD
#define RFID_RESET_PORT GPIOD
#define RFID_RESET_PIN GPIO_PIN_7

static RfidTagId currentTag;
static uint32_t lastTimeValidMs;
constexpr uint32_t tagRemovedTimeoutMs = 500;

void RfidReader::init()
{
    // init the Reset pin
    RCC_AHB1PeriphClockCmd(RFID_RESET_PORT_CLK, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_PIN_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(RFID_RESET_PORT, &GPIO_InitStructure);

    // reset the MFRC522
    GPIO_ResetBits(RFID_RESET_PORT, RFID_RESET_PIN);
    Systick::delayMs(10);
    GPIO_SetBits(RFID_RESET_PORT, RFID_RESET_PIN);
    Systick::delayMs(10);

    TM_MFRC522_Init();
    currentTag = RfidTagId::invalid();
    lastTimeValidMs = Systick::getMsCounter();
}

void RfidReader::readAndGenerateEvents(UiEventQueue& queue)
{
    union
    {
        uint32_t asUint32;
        uint8_t asCharStringBuffer[5];
    } newTagId;
    TM_MFRC522_Status_t res = TM_MFRC522_Check(newTagId.asCharStringBuffer);

    // there's currently a tag on the reader
    if (res == MI_OK)
    {
        // there was no tag on the reader before
        if (!currentTag.isValid())
        {
            queue.pushEvent(UiEvent { UiEvent::Type::rfidTagAdded, newTagId.asUint32 });
            currentTag = newTagId.asUint32;
            lastTimeValidMs = Systick::getMsCounter();
        }
        else
        // there WAS a tag on the reader before
        {
            // but it's was a different one!
            if (currentTag != newTagId.asUint32)
            {
                queue.pushEvent(UiEvent { UiEvent::Type::rfidTagRemoved, 0 });
                queue.pushEvent(UiEvent { UiEvent::Type::rfidTagAdded, newTagId.asUint32 });
                currentTag = newTagId.asUint32;
                lastTimeValidMs = Systick::getMsCounter();
            }
            else 
                // the tag has not changed.
                lastTimeValidMs = Systick::getMsCounter();
        }
    }
    // there's no tag on the reader anymore
    else
    {
        // there was a tag before so it must have been removed
        if (currentTag.isValid())
        {
            const auto now = Systick::getMsCounter();
            if (now - lastTimeValidMs > tagRemovedTimeoutMs)
            {
                queue.pushEvent(UiEvent { UiEvent::Type::rfidTagRemoved, 0 });
                currentTag = RfidTagId::invalid();
            }
        }
    }
}