#pragma once
#include "LockFreeFifo.h"

struct UiEvent
{
    enum class Type
    {
        invalid = 0,
        prevBttnPressed,
        prevBttnReleased,
        nextBttnPressed,
        nextBttnReleased,
        rfidTagAdded,
        rfidTagRemoved
    };
    Type type;
    union
    {
        // valid for type == Type::rfidTagAdded
        uint32_t asRfidTagId;
    } payload;

    bool isValid() const { return type != Type::invalid; }
    static UiEvent getInvalidEvent() { return { Type::invalid, { 0 } }; }
};

class UiEventQueue
{
public:
    UiEventQueue()
    {
    }

    void pushEvent(UiEvent e)
    {
        // we'll accept lost events when fifo is full
        fifo_.writeSingle(e);
    }

    UiEvent popEvent()
    {
        if (fifo_.isEmpty())
            return UiEvent::getInvalidEvent();
        UiEvent result;
        fifo_.readSingle(result);
        return result;
    }

    size_t getNumEvents() const { return fifo_.getNumReady(); }

private:
    LockFreeFifo<UiEvent, 32> fifo_;
};