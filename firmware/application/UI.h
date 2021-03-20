#pragma once
#include "UiEventQueue.h"

class ButtonScanner
{
public:
    static void init(UiEventQueue& eventQueueToUse);
};

class LED
{
public:
    enum class Pattern 
    {
        redContinuous, 
        yellowContinuous,
        greenContinuous,
        linkWaitingForTag,
        linkErrorWaitingForTagRemove,
        linkSuccessfulWaitingForTagRemove,
        idle,
        playing,
        errNoCard,
        errInternal,
        off
    };

    static void init();
    static void setLed(Pattern pattern);
};