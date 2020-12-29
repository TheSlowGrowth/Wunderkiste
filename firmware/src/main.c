#include "main.h"
#include "core_cm4.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx_rcc.h"
#include "Audio.h"
#include "UI.h"
#include "RfidReader.h"
#include "player.h"
#include "statemachine.h"
#include "tm_stm32f4_mfrc522.h"
#include "tm_stm32f4_fatfs.h"
#include "ff.h"

// Variables
volatile uint32_t		time_var1, time_var2;
RCC_ClocksTypeDef		RCC_Clocks;

bool filesystemMounted = false;

void updateUI() 
{
	if (!filesystemMounted)
		ui_enterState(ui_state_errNoCard);
	else 
	{
		switch (stateMachine_getState())
		{
		case stPlaying:
			ui_enterState(ui_state_playing);
			break;
		case stLinkWaitingForTag:
			ui_enterState(ui_state_linkingWaitingForTag);
			break;
		case stLinkWaitingForTagRemove:
			ui_enterState(ui_state_linkingWaitingForTagRemove);
			break;
		case stWaitingForTag:
		case stStoppedWaitingForTagRemove:
			ui_enterState(ui_state_idle);
			break;
		default:
			ui_enterState(ui_state_errNoCard);
			break;
		}
	}
	ui_process();
}

/*
 * Main function. Called when startup code is done with
 * copying memory and setting up clocks.
 */
int main(void) {
	// init state machine and latch power
	stateMachine_init();

	// init ui (leds & buttons)
	ui_init();

	// SysTick interrupt each 1ms
	RCC_GetClocksFreq(&RCC_Clocks);
	SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);

	// GPIO Peripheral clock enable
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI4, ENABLE);

	// init filesystem
	FATFS FatFs;
	FRESULT res = f_mount(&FatFs, "0:", 1);
	if (res == FR_OK)
	{
			filesystemMounted = true;
			stateMachine_StorageAdded();
	}
	else
	{
			filesystemMounted = false;
			ui_enterState(ui_state_errNoCard);
	}

	// init the player
	pl_init();

	// init the RFID reader
	rfid_init();

	uint16_t cycleCounter = 0;

	for(;;) {
		pl_run();

		// check RFID only every now and then
		cycleCounter++;
		if (cycleCounter >= 1500)
		{
			cycleCounter = 0;
			stateMachine_run();
			updateUI();
		}
	}
}
