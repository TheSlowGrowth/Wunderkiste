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