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

#include "RfidReader.h"
#include "tm_stm32f4_mfrc522.h"
#include "tm_stm32f4_delay.h"

#define RFID_RESET_PORT_CLK RCC_AHB1Periph_GPIOD
#define RFID_RESET_PORT     GPIOD
#define RFID_RESET_PIN      GPIO_PIN_7


void rfid_init()
{
    // init the Reset pin 
	RCC_AHB1PeriphClockCmd(RFID_RESET_PORT_CLK, ENABLE);
    GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_PIN_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_25MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(RFID_RESET_PORT, &GPIO_InitStructure);

    // reset the MFRC522
    GPIO_ResetBits(RFID_RESET_PORT, RFID_RESET_PIN);
    Delayms(10);
    GPIO_SetBits(RFID_RESET_PORT, RFID_RESET_PIN);
    Delayms(10);

	TM_MFRC522_Init();
}

bool rfid_readTag(rfid_tagId_t currentTagId)
{
    TM_MFRC522_Status_t res = TM_MFRC522_Check(currentTagId);
    return res == MI_OK;
}

bool rfid_tagIdsEqual(rfid_tagId_t tagId1, rfid_tagId_t tagId2)
{
    return TM_MFRC522_Compare(tagId1, tagId2) == MI_OK;
}