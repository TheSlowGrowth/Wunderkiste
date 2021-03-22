#include "UI.h"

extern "C"
{
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
}
#include "Platform.h"

#define BUTTON_PREV_PORT GPIOA
#define BUTTON_PREV_PIN GPIO_Pin_2
#define BUTTON_NEXT_PORT GPIOA
#define BUTTON_NEXT_PIN GPIO_Pin_3

#define LED_GREEN_PORT GPIOD
#define LED_GREEN_PIN GPIO_Pin_1
#define LED_RED_PORT GPIOD
#define LED_RED_PIN GPIO_Pin_0

#define BUTTON_PREV_DOWN (!(BUTTON_PREV_PORT->IDR & BUTTON_PREV_PIN))
#define BUTTON_NEXT_DOWN (!(BUTTON_NEXT_PORT->IDR & BUTTON_NEXT_PIN))

static constexpr int8_t counterThresholdMs = 100;
static constexpr uint8_t numButtons = 2;
// The counter counts from 0 upwards to debounce the "depressed" event,
// generating a "pressed" event when it reaches counterThresholdMs.
// The counter counts from 0 downwards to debounce the "released" event,
// generating a "released" event when it reaches -counterThresholdMs.
static int8_t buttonCounters[numButtons];
static UiEventQueue* eventQueue = nullptr;

static void initButtonGPIO(GPIO_TypeDef* port, uint32_t pin)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(port, &GPIO_InitStructure);
}

class ButtonDebouncer : public Systick::Listener
{
public:
    void systickCallback() override
    {
        processSingleButton(0, BUTTON_PREV_DOWN, UiEvent::Type::prevBttnPressed, UiEvent::Type::prevBttnReleased);
        processSingleButton(1, BUTTON_NEXT_DOWN, UiEvent::Type::nextBttnPressed, UiEvent::Type::nextBttnReleased);
    }

private:
    void processSingleButton(int index,
                             bool currentState,
                             UiEvent::Type eventToGenerateWhenPressed,
                             UiEvent::Type eventToGenerateWhenReleased)
    {
        if (currentState)
        {
            // was not depressed before
            if (buttonCounters[index] <= 0)
                buttonCounters[index] = 1;
            // was depressed before
            else if (buttonCounters[index] > 0)
            {
                if (buttonCounters[index] == counterThresholdMs)
                {
                    buttonCounters[index]++;
                    if (eventQueue)
                        eventQueue->pushEvent(UiEvent { eventToGenerateWhenPressed, 0 });
                }
                else if (buttonCounters[index] < counterThresholdMs)
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
                if (buttonCounters[index] == -counterThresholdMs)
                {
                    buttonCounters[index]--;
                    if (eventQueue)
                        eventQueue->pushEvent(UiEvent { eventToGenerateWhenReleased, 0 });
                }
                else if (buttonCounters[index] > -counterThresholdMs)
                    buttonCounters[index]--;
            }
        }
    }
};
ButtonDebouncer buttonDebouncer_;

void ButtonScanner::init(UiEventQueue& eventQueueToUse)
{
    // setup GPIO clocks
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    // Button inputs
    initButtonGPIO(BUTTON_PREV_PORT, BUTTON_PREV_PIN);
    initButtonGPIO(BUTTON_NEXT_PORT, BUTTON_NEXT_PIN);

    // adjust debouncing to the initial state of the buttons
    buttonCounters[0] = (BUTTON_PREV_DOWN) ? counterThresholdMs + 1 : -counterThresholdMs - 1;
    buttonCounters[1] = (BUTTON_NEXT_DOWN) ? counterThresholdMs + 1 : -counterThresholdMs - 1;

    eventQueue = &eventQueueToUse;
    // install a Systick callback for the debouncing
    Systick::addListener(&buttonDebouncer_);
}

namespace LedPatterns
{
    struct BlinkPattern
    {
        uint32_t redStates;
        uint32_t greenStates;
    };

    static const BlinkPattern off = { 0b00000000000000000000000000000000,
                                      0b00000000000000000000000000000000 };
    static const BlinkPattern greenContinuous = { 0b00000000000000000000000000000000,
                                                  0b11111111111111111111111111111111 };
    static const BlinkPattern yellowContinuous = { 0b11111111111111111111111111111111,
                                                   0b11111111111111111111111111111111 };
    static const BlinkPattern redContinuous = { 0b11111111111111111111111111111111,
                                                0b00000000000000000000000000000000 };
    static const BlinkPattern linkWaitingForTag = { 0b11111111111111111111111111110000,
                                                    0b11111111111111111111111111110101 };
    static const BlinkPattern linkSuccessfulWaitingForTagRemove = { 0b00000000000000000000000000000000,
                                                                    0b01010101010101010101010101010101 };
    static const BlinkPattern linkErrorWaitingForTagRemove = { 0b01010101010101010101010101010101,
                                                               0b00000000000000000000000000000000 };
    static const BlinkPattern errNoCard = { 0b00000000000011110000111100001111,
                                            0b00000000000000000000000000000000 };
    static const BlinkPattern errInternal = { 0b00000000000001010000010100000101,
                                              0b00000000000000000000000000000000 };
} // namespace LedPatterns

class LedBlinker : public Systick::Listener
{
public:
    LedBlinker() :
        currentPattern_(&LedPatterns::off),
        currentStep_(0),
        prescaler_(0)
    {
        Systick::addListener(this);
    }

    void setPattern(const LedPatterns::BlinkPattern& nextPattern)
    {
        currentStep_ = 0;
        currentPattern_ = &nextPattern;
        updateLeds();
    }

    void systickCallback() override
    {
        if (prescaler_++ >= prescalerMax_)
        {
            prescaler_ = 0;
            // advance to next step in the cycle
            currentStep_ = (currentStep_ + 1) & 0x1F;
            updateLeds();
        }
    }

private:
    void updateLeds()
    {
        const auto mask = (1 << currentStep_);
        if (currentPattern_->redStates & mask)
            GPIO_SetBits(LED_RED_PORT, LED_RED_PIN);
        else
            GPIO_ResetBits(LED_RED_PORT, LED_RED_PIN);
        if (currentPattern_->greenStates & mask)
            GPIO_SetBits(LED_GREEN_PORT, LED_GREEN_PIN);
        else
            GPIO_ResetBits(LED_GREEN_PORT, LED_GREEN_PIN);
    }

    const LedPatterns::BlinkPattern* currentPattern_;
    uint8_t currentStep_;
    uint8_t prescaler_;
    static constexpr int prescalerMax_ = 100; // 100ms intervals
};

LateInitializedObject<LedBlinker> ledBlinker;

static void initLedGPIO(GPIO_TypeDef* port, uint32_t pin)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(port, &GPIO_InitStructure);
}

void LED::init()
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    initLedGPIO(LED_GREEN_PORT, LED_GREEN_PIN);
    initLedGPIO(LED_RED_PORT, LED_RED_PIN);

    ledBlinker.create();
}

void LED::setLed(LED::Pattern pattern)
{
    switch (pattern)
    {
        case LED::Pattern::redContinuous:
            ledBlinker->setPattern(LedPatterns::redContinuous);
            break;
        case LED::Pattern::yellowContinuous:
        case LED::Pattern::playing:
            ledBlinker->setPattern(LedPatterns::yellowContinuous);
            break;
        case LED::Pattern::greenContinuous:
        case LED::Pattern::idle:
            ledBlinker->setPattern(LedPatterns::greenContinuous);
            break;
        case LED::Pattern::linkWaitingForTag:
            ledBlinker->setPattern(LedPatterns::linkWaitingForTag);
            break;
        case LED::Pattern::linkSuccessfulWaitingForTagRemove:
            ledBlinker->setPattern(LedPatterns::linkSuccessfulWaitingForTagRemove);
            break;
        case LED::Pattern::linkErrorWaitingForTagRemove:
            ledBlinker->setPattern(LedPatterns::linkErrorWaitingForTagRemove);
            break;
        case LED::Pattern::errNoCard:
            ledBlinker->setPattern(LedPatterns::errNoCard);
            break;
        case LED::Pattern::errInternal:
            ledBlinker->setPattern(LedPatterns::errInternal);
            break;
        default:
        case LED::Pattern::off:
            ledBlinker->setPattern(LedPatterns::off);
            break;
    }
}