#include <gtest/gtest.h>
#include "LockFreeFifo.h"

class LockFreeFifo_Fixture : public ::testing::Test
{
protected:
    static constexpr int fifoSize_ = 100;
    LockFreeFifo<int, fifoSize_> fifo_;
};

TEST_F(LockFreeFifo_Fixture, a_getSize)
{
    EXPECT_EQ(fifo_.getSize(), fifoSize_);
}

TEST_F(LockFreeFifo_Fixture, b_emptyAfterInit)
{
    EXPECT_TRUE(fifo_.isEmpty());
    EXPECT_EQ(fifo_.getNumReady(), 0);
    EXPECT_EQ(fifo_.getNumFree(), fifoSize_);
}

TEST_F(LockFreeFifo_Fixture, c_writeAndReadNonWrapping)
{
    int* block1 = nullptr;
    int blockSize1 = 0;
    int* block2 = nullptr;
    int blockSize2 = 0;

    // prepare write
    const int numWritesToRequest = 10;
    fifo_.prepareWrite(numWritesToRequest, block1, blockSize1, block2, blockSize2);
    EXPECT_NE(block1, nullptr);
    EXPECT_EQ(blockSize1, numWritesToRequest);
    EXPECT_EQ(block2, nullptr);
    EXPECT_EQ(blockSize2, 0);
    // expect write operation not finished yet
    EXPECT_EQ(fifo_.getNumReady(), 0);

    // write a sequence of numbers
    const int numToActuallyWrite = numWritesToRequest - 2;
    for (int i = 0; i < numToActuallyWrite; i++)
        block1[i] = i;

    // finish write
    fifo_.finishWrite(numToActuallyWrite);
    EXPECT_EQ(fifo_.getNumReady(), numToActuallyWrite);

    // check results by reading
    const int numReadsToRequest = 6;
    fifo_.prepareRead(numReadsToRequest, block1, blockSize1, block2, blockSize2);
    EXPECT_NE(block1, nullptr);
    EXPECT_EQ(blockSize1, numReadsToRequest);
    EXPECT_EQ(block2, nullptr);
    EXPECT_EQ(blockSize2, 0);
    // expect read operation not finished yet
    EXPECT_EQ(fifo_.getNumReady(), numToActuallyWrite);

    // read some data
    const int numToActuallyRead = numReadsToRequest - 2;
    for (int i = 0; i < numToActuallyRead; i++)
        EXPECT_EQ(block1[i], i);

    // finish read
    fifo_.finishRead(numToActuallyRead);
    // some data is still left in the buffer
    EXPECT_EQ(fifo_.getNumReady(), numToActuallyWrite - numToActuallyRead);
    EXPECT_EQ(fifo_.getNumFree(), fifoSize_ - fifo_.getNumReady());
}

TEST_F(LockFreeFifo_Fixture, d_writeAndReadWrapping)
{
    int* block1 = nullptr;
    int blockSize1 = 0;
    int* block2 = nullptr;
    int blockSize2 = 0;

    // advance till the end of the buffer
    const int numToLeaveAtEndOfBuffer = 5;
    fifo_.prepareWrite(fifoSize_ - numToLeaveAtEndOfBuffer, block1, blockSize1, block2, blockSize2);
    fifo_.finishWrite(fifoSize_ - numToLeaveAtEndOfBuffer);
    fifo_.prepareRead(fifoSize_ - numToLeaveAtEndOfBuffer, block1, blockSize1, block2, blockSize2);
    fifo_.finishRead(fifoSize_ - numToLeaveAtEndOfBuffer);
    EXPECT_EQ(fifo_.getNumReady(), 0);
    EXPECT_TRUE(fifo_.isEmpty());

    // prepare write that extends beyond the end of the buffer
    const int numWritesToRequest = 10;
    fifo_.prepareWrite(numWritesToRequest, block1, blockSize1, block2, blockSize2);
    EXPECT_NE(block1, nullptr);
    EXPECT_NE(blockSize1, 0);
    EXPECT_NE(block2, nullptr);
    EXPECT_NE(blockSize2, 0);
    EXPECT_EQ(numWritesToRequest, blockSize1 + blockSize2);
    // expect write operation not finished yet
    EXPECT_EQ(fifo_.getNumReady(), 0);

    // write a sequence of numbers
    const int numToActuallyWrite = numWritesToRequest - 2;
    for (int i = 0; i < numToActuallyWrite; i++)
    {
        if (i < blockSize1)
            block1[i] = i;
        else
            block2[i - blockSize1] = i;
    }

    // finish write
    fifo_.finishWrite(numToActuallyWrite);
    EXPECT_EQ(fifo_.getNumReady(), numToActuallyWrite);

    // check results by reading
    const int numReadsToRequest = 7;
    fifo_.prepareRead(numReadsToRequest, block1, blockSize1, block2, blockSize2);
    EXPECT_NE(block1, nullptr);
    EXPECT_NE(blockSize1, 0);
    EXPECT_NE(block2, nullptr);
    EXPECT_NE(blockSize2, 0);
    EXPECT_EQ(blockSize1 + blockSize2, numReadsToRequest);
    // expect read operation not finished yet
    EXPECT_EQ(fifo_.getNumReady(), numToActuallyWrite);

    // read the data
    const int numToActuallyRead = numReadsToRequest;
    for (int i = 0; i < numToActuallyRead; i++)
    {
        if (i < blockSize1)
            EXPECT_EQ(block1[i], i);
        else
            EXPECT_EQ(block2[i - blockSize1], i);
    }

    // finish read
    fifo_.finishRead(numToActuallyRead);
    // some data is still left in the buffer
    EXPECT_EQ(fifo_.getNumReady(), numToActuallyWrite - numToActuallyRead);
    EXPECT_EQ(fifo_.getNumFree(), fifoSize_ - fifo_.getNumReady());
}

TEST_F(LockFreeFifo_Fixture, e_fill)
{
    int* block1 = nullptr;
    int blockSize1 = 0;
    int* block2 = nullptr;
    int blockSize2 = 0;

    // fill almost entirely
    const int numToLeaveAtEndOfBuffer = 5;
    fifo_.prepareWrite(fifoSize_ - numToLeaveAtEndOfBuffer, block1, blockSize1, block2, blockSize2);
    fifo_.finishWrite(fifoSize_ - numToLeaveAtEndOfBuffer);
    EXPECT_EQ(fifo_.getNumReady(), fifoSize_ - numToLeaveAtEndOfBuffer);
    EXPECT_EQ(fifo_.getNumFree(), numToLeaveAtEndOfBuffer);
    EXPECT_FALSE(fifo_.isEmpty());
    EXPECT_FALSE(fifo_.isFull());

    // request more than is available
    const int numToRequest = numToLeaveAtEndOfBuffer + 2;
    fifo_.prepareWrite(numToRequest, block1, blockSize1, block2, blockSize2);
    EXPECT_NE(block1, nullptr);
    EXPECT_EQ(blockSize1, numToLeaveAtEndOfBuffer); // can't write more than is available!
    EXPECT_EQ(block2, nullptr);
    EXPECT_EQ(blockSize2, 0);
    fifo_.finishWrite(blockSize1);

    // now the buffer is full
    EXPECT_TRUE(fifo_.isFull());

    // This was the case where tail_ == 0 and head_ == buffersize - 1.
    // If we read and write so that tail and head are advanced by one, we can check the
    // other case where head_ == tail_ - 1
    fifo_.prepareRead(1, block1, blockSize1, block2, blockSize2);
    fifo_.finishRead(1);
    fifo_.prepareWrite(1, block1, blockSize1, block2, blockSize2);
    fifo_.finishWrite(1);
    EXPECT_TRUE(fifo_.isFull());
}

TEST_F(LockFreeFifo_Fixture, f_writeSingleReadSingle)
{
    int* block1 = nullptr;
    int blockSize1 = 0;
    int* block2 = nullptr;
    int blockSize2 = 0;

    // advance till the end of the buffer so that we can check the wrapping
    // of head and tail pointers
    const int numToLeaveAtEndOfBuffer = 1;
    fifo_.prepareWrite(fifoSize_ - numToLeaveAtEndOfBuffer, block1, blockSize1, block2, blockSize2);
    fifo_.finishWrite(fifoSize_ - numToLeaveAtEndOfBuffer);
    fifo_.prepareRead(fifoSize_ - numToLeaveAtEndOfBuffer, block1, blockSize1, block2, blockSize2);
    fifo_.finishRead(fifoSize_ - numToLeaveAtEndOfBuffer);
    EXPECT_EQ(fifo_.getNumReady(), 0);
    EXPECT_TRUE(fifo_.isEmpty());

    // write a single entries
    EXPECT_TRUE(fifo_.writeSingle(100));
    EXPECT_TRUE(fifo_.writeSingle(200));
    EXPECT_TRUE(fifo_.writeSingle(300));
    EXPECT_EQ(fifo_.getNumReady(), 3);
    // read single entries
    int result;
    EXPECT_TRUE(fifo_.readSingle(result));
    EXPECT_EQ(result, 100);
    EXPECT_TRUE(fifo_.readSingle(result));
    EXPECT_EQ(result, 200);
    EXPECT_TRUE(fifo_.readSingle(result));
    EXPECT_EQ(result, 300);
    EXPECT_FALSE(fifo_.readSingle(result));
    EXPECT_EQ(result, 300); // unchanged
    // now empty
    EXPECT_TRUE(fifo_.isEmpty());

    // fill almost entirely
    fifo_.prepareWrite(fifoSize_ - numToLeaveAtEndOfBuffer, block1, blockSize1, block2, blockSize2);
    fifo_.finishWrite(fifoSize_ - numToLeaveAtEndOfBuffer);
    // write until writing fails
    EXPECT_TRUE(fifo_.writeSingle(0));
    EXPECT_FALSE(fifo_.writeSingle(1)); // is now full
    EXPECT_TRUE(fifo_.isFull());
}