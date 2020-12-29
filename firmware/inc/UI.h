#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>
#include <stdbool.h>

typedef enum 
{
    ui_state_idle,
    ui_state_playing,
    ui_state_linkingWaitingForTag,
    ui_state_linkingWaitingForTagRemove,
    ui_state_errNoCard,
} ui_state_t;

typedef enum 
{
    ui_button_next,
    ui_button_prev,
} ui_button_t;

typedef enum 
{
    ui_event_none = 0,
    ui_event_prev_pressed,
    ui_event_prev_released,
    ui_event_next_pressed,
    ui_event_next_released,
} ui_event_t;

void ui_init();
void ui_enterState(ui_state_t newState);
bool ui_isButtonDown(ui_button_t button);
void ui_processLeds();

void ui_processButtons();
ui_event_t ui_getNextEvent();

#endif // ifndef __UI_H__