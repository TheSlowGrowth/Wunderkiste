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

#define BUTTON_PREV			(!(BUTTON_PREV_PORT->IDR & BUTTON_PREV_PIN))
#define BUTTON_NEXT			(!(BUTTON_NEXT_PORT->IDR & BUTTON_NEXT_PIN))

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
            return BUTTON_PREV;
        case ui_button_next:
            return BUTTON_NEXT;
        default:
            return false;
    }
}

void ui_process()
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
