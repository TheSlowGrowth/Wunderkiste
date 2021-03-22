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
#include "stdint.h"
#include <algorithm>

/** A single reader / single writer lock-free FIFO */
template <typename ValueType, int size>
class LockFreeFifo
{
public:
    LockFreeFifo()
    {
        clear();
    }

    int getSize() const { return size; }
    bool isEmpty() const { return head_ == tail_; }
    bool isFull() const
    {
        if ((tail_ == 0) && (head_ == rawBufferSize_ - 1))
            return true;
        if (head_ == tail_ - 1)
            return true;
        return false;
    }
    int getNumReady() const { return head_ >= tail_ ? (head_ - tail_) : (rawBufferSize_ - (tail_ - head_)); }
    int getNumFree() const { return head_ >= tail_ ? (rawBufferSize_ - 1 - (head_ - tail_)) : (tail_ - head_ - 1); }
    void clear() { head_ = tail_ = 0; }

    void prepareWrite(int numToWrite, ValueType*& block1, int& blockSize1, ValueType*& block2, int& blockSize2)
    {
        numToWrite = std::min(numToWrite, getNumFree());

        if (numToWrite > 0)
        {
            block1 = &buffer_[head_];
            blockSize1 = std::min(rawBufferSize_ - head_, numToWrite);
            numToWrite -= blockSize1;
            if (numToWrite > 0)
            {
                block2 = &buffer_[0];
                blockSize2 = std::min(numToWrite, tail_);
            }
            else
            {
                block2 = nullptr;
                blockSize2 = 0;
            }
        }
        else
        {
            block1 = nullptr;
            block2 = nullptr;
            blockSize1 = 0;
            blockSize2 = 0;
        }
    }

    void finishWrite(int numWritten)
    {
        auto newHead = head_ + numWritten;
        if (newHead >= rawBufferSize_)
            newHead -= rawBufferSize_;
        head_ = newHead;
    }

    bool writeSingle(const ValueType& value)
    {
        if (isFull())
            return false;
        buffer_[head_] = value;
        auto newHead = head_ + 1;
        if (newHead >= rawBufferSize_)
            newHead -= rawBufferSize_;
        head_ = newHead;
        return true;
    }

    void prepareRead(int numToRead, ValueType*& block1, int& blockSize1, ValueType*& block2, int& blockSize2)
    {
        numToRead = std::min(numToRead, getNumReady());

        if (numToRead > 0)
        {
            block1 = &buffer_[tail_];
            blockSize1 = std::min(rawBufferSize_ - tail_, numToRead);
            numToRead -= blockSize1;
            if (numToRead > 0)
            {
                block2 = &buffer_[0];
                blockSize2 = std::min(numToRead, head_);
            }
            else
            {
                block2 = nullptr;
                blockSize2 = 0;
            }
        }
        else
        {
            block1 = nullptr;
            block2 = nullptr;
            blockSize1 = 0;
            blockSize2 = 0;
        }
    }

    void finishRead(int numRead)
    {
        auto newTail = tail_ + numRead;
        if (newTail >= rawBufferSize_)
            newTail -= rawBufferSize_;
        tail_ = newTail;
    }

    bool readSingle(ValueType& value)
    {
        if (isEmpty())
            return false;
        value = buffer_[tail_];
        auto newTail = tail_ + 1;
        if (newTail >= rawBufferSize_)
            newTail -= rawBufferSize_;
        tail_ = newTail;
        return true;
    }

private:
    static constexpr int rawBufferSize_ = size + 1;
    ValueType buffer_[rawBufferSize_];
    int head_;
    int tail_;
};