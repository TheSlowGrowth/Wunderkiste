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

#include "UI.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"

#define BUTTON_PREV_PORT        GPIOA
#define BUTTON_PREV_PIN         GPIO_Pin_2

#define BUTTON_NEXT_PORT        GPIOA
#define BUTTON_NEXT_PIN         GPIO_Pin_3

#define LED_GREEN_PORT          GPIOD
#define LED_GREEN_PIN           GPIO_Pin_1
#define LED_RED_PORT            GPIOD
#define LED_RED_PIN             GPIO_Pin_0

#define BUTTON_PREV_DOWN			(!(BUTTON_PREV_PORT->IDR & BUTTON_PREV_PIN))
#define BUTTON_NEXT_DOWN			(!(BUTTON_NEXT_PORT->IDR & BUTTON_NEXT_PIN))

ui_state_t ui_currentState;

typedef struct 
{
    uint32_t redStates;
    uint32_t greenStates;
} ui_blinkPattern_t;

const ui_blinkPattern_t ui_patternIdle =                    { 0b00000000000000000000000000000000, 
                                                              0b11111111111111111111111111111111 };
const ui_blinkPattern_t ui_patternPlaying =                 { 0b11111111111111111111111111111111, 
                                                              0b11111111111111111111111111111111 };
const ui_blinkPattern_t ui_patternLinkWaitingForTag =       { 0b11111111111111111111111111110000, 
                                                              0b11111111111111111111111111110101 };
const ui_blinkPattern_t ui_patternLinkWaitingForTagRemove = { 0b00000000000000000000000000000000, 
                                                              0b01010101010101010101010101010101 };
const ui_blinkPattern_t ui_patternErrNoCard =               { 0b00000000000011110000111100001111, 
                                                              0b00000000000000000000000000000000 };
const ui_blinkPattern_t ui_patternErrInternal =             { 0b00000000000001010000010100000101, 
                                                              0b00000000000000000000000000000000 };
const ui_blinkPattern_t* ui_currentPattern;
uint8_t ui_currentPatternProgress;

void ui_setup_led_pin(GPIO_TypeDef* port, uint32_t pin)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(port, &GPIO_InitStructure);
}
void ui_setup_button_pin(GPIO_TypeDef* port, uint32_t pin)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = pin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(port, &GPIO_InitStructure);
}

void ui_initLeds()
{
    ui_currentPattern = &ui_patternIdle;
    ui_currentPatternProgress = 0;
    ui_currentState = -1; // invalid
}

void ui_enterState(ui_state_t newState)
{
    if (newState != ui_currentState)
    {
        ui_currentState = newState;
        ui_currentPatternProgress = 0;
        switch(newState) 
        {
        case ui_state_idle:
            ui_currentPattern = &ui_patternIdle;
            break;
        case ui_state_playing:
            ui_currentPattern = &ui_patternPlaying;
            break;
        case ui_state_linkingWaitingForTag:
            ui_currentPattern = &ui_patternLinkWaitingForTag;
            break;
        case ui_state_linkingWaitingForTagRemove:
            ui_currentPattern = &ui_patternLinkWaitingForTagRemove;
            break;
        case ui_state_errNoCard:
            ui_currentPattern = &ui_patternErrNoCard;
            break;
        default:
            ui_currentPattern = &ui_patternErrInternal;
            break;
        }   
    }
}

bool ui_isButtonDown(ui_button_t button)
{
    switch(button) 
    {
        case ui_button_prev:
            return BUTTON_PREV_DOWN;
        case ui_button_next:
            return BUTTON_NEXT_DOWN;
        default:
            return false;
    }
}

void ui_processLeds()
{
    const int prescaler = 3;

    bool red = ui_currentPattern->redStates & (1 << (ui_currentPatternProgress >> prescaler));
    if (red)
        GPIO_SetBits(LED_RED_PORT, LED_RED_PIN);
    else 
        GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN);

    bool green = ui_currentPattern->greenStates & (1 << (ui_currentPatternProgress >> prescaler));
    if (green)
        GPIO_SetBits(LED_GREEN_PORT, LED_GREEN_PIN);
    else 
        GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN);

    ui_currentPatternProgress++;
    ui_currentPatternProgress &= ((0x1F + 1) << prescaler) - 1;
}

// Event Queue

#define UI_EVENT_QUEUE_SIZE 256
#define UI_EVENT_QUEUE_BITMASK 0xFF
uint8_t ui_eventQueue[UI_EVENT_QUEUE_SIZE];
uint8_t ui_eventQueueHead;
uint8_t ui_eventQueueTail;

#define UI_NUM_BUTTONS 2
#define UI_COUNTER_THRESH 100
// The counter counts from 0 upwards to debounce the "depressed" event,
// generating a "ui_event_XYZ_pressed" event when it reaches UI_COUNTER_THRESH.
// The counter counts from 0 downwards to debounce the "released" event,
// generating a "ui_event_XYZ_released" event when it reaches -UI_COUNTER_THRESH.
int8_t buttonCounters[UI_NUM_BUTTONS];

void ui_initButtons() 
{   
    // adjust debouncing to the initial state of the buttons
    buttonCounters[0] = (BUTTON_PREV_DOWN) ? UI_COUNTER_THRESH + 1 : -UI_COUNTER_THRESH - 1;
    buttonCounters[1] = (BUTTON_NEXT_DOWN) ? UI_COUNTER_THRESH + 1 : -UI_COUNTER_THRESH - 1;

    ui_eventQueueHead = 0;
    ui_eventQueueTail = 0;
}

void ui_addEvent(ui_event_t eventToAdd)
{
    ui_eventQueue[ui_eventQueueHead] = eventToAdd;
    ui_eventQueueHead = UI_EVENT_QUEUE_BITMASK & (ui_eventQueueHead + 1);
}

void ui_processButton(int index, bool currentState, ui_event_t eventToGenerateWhenPressed, ui_event_t eventToGenerateWhenReleased)
{
    if (currentState)
    {
        // was not depressed before
        if (buttonCounters[index] <= 0)
            buttonCounters[index] = 1;
        // was depressed before
        else if (buttonCounters[index] > 0)
        {
            if (buttonCounters[index] == UI_COUNTER_THRESH)
            {
                buttonCounters[index]++;
                ui_addEvent(eventToGenerateWhenPressed);
            }
            else if (buttonCounters[index] < UI_COUNTER_THRESH)
                buttonCounters[index]++;
        }
    }
    else 
    {
        // was depressed before
        if (buttonCounters[index] >= 0)
            buttonCounters[index] = -1;
        // was not depressed before
        else if (buttonCounters[index] > 0)
        {
            if (buttonCounters[index] == -UI_COUNTER_THRESH)
            {
                buttonCounters[index]--;
                ui_addEvent(eventToGenerateWhenReleased);
            }
            else if (buttonCounters[index] > -UI_COUNTER_THRESH)
                buttonCounters[index]--;
        }
    }
}

void ui_processButtons()
{
    ui_processButton(0, BUTTON_PREV_DOWN, ui_event_prev_pressed, ui_event_prev_released);
    ui_processButton(1, BUTTON_NEXT_DOWN, ui_event_next_pressed, ui_event_next_released);
}

ui_event_t ui_getNextEvent()
{
    if (ui_eventQueueHead == ui_eventQueueTail)
        return ui_event_none;
    else 
    {
        ui_event_t event = ui_eventQueue[ui_eventQueueTail];
        ui_eventQueueTail = UI_EVENT_QUEUE_BITMASK & (ui_eventQueueTail + 1);
        return event;
    }
}

// Init

void ui_init() 
{
    // setup GPIO clocks
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	// LED outputs
    ui_setup_led_pin(LED_GREEN_PORT, LED_GREEN_PIN);
    ui_setup_led_pin(LED_RED_PORT, LED_RED_PIN);

    // Button inputs
    ui_setup_button_pin(BUTTON_PREV_PORT, BUTTON_PREV_PIN);
    ui_setup_button_pin(BUTTON_NEXT_PORT, BUTTON_NEXT_PIN);

    ui_initLeds();
    ui_initButtons();
}