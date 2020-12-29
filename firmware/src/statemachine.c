#include "statemachine.h"
#include <stdio.h>
#include "RfidReader.h"
#include "player.h"
#include "library.h"
#include "tm_stm32f4_delay.h"
#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "ff.h"
#include "main.h"
#include "UI.h"

// Macros
#define POWER_ON			GPIO_ResetBits(GPIOA, GPIO_Pin_1)
#define POWER_OFF			GPIO_SetBits(GPIOA, GPIO_Pin_1)
#define POWER_TIMEOUT_MAX   2600
#define FAILCOUNTER_MAX     50

machineState_t mstate;
uint16_t power_timeout;
uint8_t failCounter;
rfid_tagId_t currentTagID;
#define STR_BUFFLEN 512
char unlinkedFolderBuff[STR_BUFFLEN];
char libResBuff[STR_BUFFLEN];

void stateMachine_init()
{
    mstate = stInit;
    failCounter = 0;
    for (int i = 0; i < 5; i++)
        currentTagID[i] = 0;

    // init the power pin that keeps the mosfet turned on until the timer runs
    // out
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

    power_timeout = 0;
    POWER_ON;
}

void stateMachine_StorageAdded()
{
    failCounter = 0;
    for (int i = 0; i < 5; i++)
        currentTagID[i] = 0;

    // init the library of tag-to-foldername links
    lib_init();
    // check for missing links
    mstate = stLinkSearch;
    power_timeout = 0;
}

void stateMachine_StorageRemoved()
{
    stateMachine_init();
}

void stateMachine_run()
{
    ui_event_t uiEvent = ui_getNextEvent();

    switch (mstate)
    {
        // wait for storage
        case stInit:
            Delayms(10);
            // if counter runs out, turn off
            power_timeout++;
            if (power_timeout >= POWER_TIMEOUT_MAX)
            {
                POWER_OFF;
            }
            break;
        // find next folder of mp3's that is not yet linked to a tag
        case stLinkSearch:
        {
            if (lib_getNextUnlinked(unlinkedFolderBuff, STR_BUFFLEN))
            {
                // there's unlinked folders missing
                bool playResult = pl_playDirectory(unlinkedFolderBuff);
                if (playResult) // folder contains playable files
                    mstate = stLinkWaitingForTag;
            }
            else
                // all folders linked, go to playback
                mstate = stWaitingForTag;
                power_timeout = 0;
            break;
        }
        // wait for user to add a tag to link this folder to
        case stLinkWaitingForTag:
        {
            bool res = rfid_readTag(currentTagID);
            if (res)
            {
                // check if the tag is already in the library
                if (!lib_getFolderForID(currentTagID, libResBuff, STR_BUFFLEN))
                {
                    // store the link if the tag is not yet linked with anything
                    lib_saveLink(unlinkedFolderBuff, currentTagID);
                    failCounter = 0;
                    pl_stop();
                    mstate = stLinkWaitingForTagRemove;
                }
            }
            // if counter runs out, turn off
            power_timeout++;
            if (power_timeout >= POWER_TIMEOUT_MAX)
            {
                POWER_OFF;
            }
            break;
        }
        // wait for user to remove the tag after successfully linking it
        case stLinkWaitingForTagRemove:
        {
            // read the tag and check if it still matches. Somethines there are
            // read errors, so we wait for several mismatches to occur in a row
            // before we can be sure that the tag is properly removed
            rfid_tagId_t newID = { 0 };
            bool res = rfid_readTag(newID);
            if (res)
            {
                // still matching - reset the failure counter
                if (rfid_tagIdsEqual(newID, currentTagID))
                    failCounter = 0;
                else
                    failCounter++;
            }
            else
            {
                failCounter++;
            }

            // tag removed, we can go on with the next unlinked folder
            if (failCounter > FAILCOUNTER_MAX)
            {
                mstate = stLinkSearch;
            }
            break;
        }
        // waiting for a tag to play
        case stWaitingForTag:
        {
            bool res = rfid_readTag(currentTagID);
            if (res)
            {
                // check if the tag is already in the library
                if (lib_getFolderForID(currentTagID, libResBuff, STR_BUFFLEN))
                {
                    pl_playDirectory(libResBuff);
                    failCounter = 0;
                    mstate = stPlaying;
                }
            }

            // if counter runs out, turn off
            power_timeout++;
            if (power_timeout >= POWER_TIMEOUT_MAX)
            {
                POWER_OFF;
            }
            break;
        }
        // playing with a tag on the reader
        case stPlaying:
        {
            // read the tag and check if it still matches. Somethines there are
            // read errors, so we wait for several mismatches to occur in a row
            // before we abort playback
            rfid_tagId_t newID = { 0 };
            bool res = rfid_readTag(newID);
            if (res)
            {
                // still matching - reset the failure counter
                if (rfid_tagIdsEqual(newID, currentTagID))
                    failCounter = 0;
                else
                    failCounter++;
            }
            else
            {
                failCounter++;
            }

            // abort playback
            if (failCounter > FAILCOUNTER_MAX)
            {
                mstate = stWaitingForTag;
                power_timeout = 0;
                pl_stop();
            }

    		if (uiEvent == ui_event_next_pressed)
    		{
    			if (!pl_next(false))
                    stateMachine_playbackFinished();
    		}
            if (uiEvent == ui_event_prev_pressed)
    		{
                // start previous track if current track just started and we're not on the first track
                if ((pl_getMsPlayedInCurrentFile() < 2000) && (pl_getCurrentFileIndex() > 0))
                {
                    if (!pl_prev(false))
                        stateMachine_playbackFinished();
                }
                // restart if current track played for a while
                else 
                    pl_restart();
                    
    		}
            break;
        }
        // playback is done, but we wait until the tag is removed from the reader
        case stStoppedWaitingForTagRemove:
        {
            // read the tag and check if it still matches. Somethines there are
            // read errors, so we wait for several mismatches to occur in a row
            // before we abort playback
            rfid_tagId_t newID = { 0 };
            bool res = rfid_readTag(newID);
            if (res)
            {
                // still matching - reset the failure counter
                if (rfid_tagIdsEqual(newID, currentTagID))
                    failCounter = 0;
                else
                    failCounter++;
            }
            else
            {
                failCounter++;
            }

            // ready to read next tag
            if (failCounter > FAILCOUNTER_MAX)
            {
                mstate = stWaitingForTag;
                power_timeout = 0;
                pl_stop();
            }
            // if counter runs out, turn off
            power_timeout++;
            if (power_timeout >= POWER_TIMEOUT_MAX)
            {
                // unmount the filesystem
                if (filesystemMounted)
                    f_mount(0, "0:", 1);

                POWER_OFF;
            }
            break;
        }
    }
}

machineState_t stateMachine_getState()
{
    return mstate;
}

void stateMachine_playbackFinished()
{
    power_timeout = 0;
    mstate = stStoppedWaitingForTagRemove;
}
