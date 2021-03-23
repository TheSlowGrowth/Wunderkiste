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

#include "FixedSizeString.h"

// for unit tests, we use a dummy version of the File class
#ifndef UNIT_TEST

// ==========================================================================
// This is the fatfs based implementation of the File class.
// Unit tests use a proxy class - see below.
// ==========================================================================

extern "C"
{
#    include "ff.h"
}

class File
{
public:
    File() :
        errorCode_(FR_OK),
        isOpened_(false)
    {
    }

    File(const char* filePath) :
        filePath_(filePath),
        errorCode_(FR_OK),
        isOpened_(false)
    {
    }

    File(const File& other) :
        filePath_(other.filePath_),
        errorCode_(FR_OK),
        isOpened_(false)
    {
    }

    ~File()
    {
        if (isOpened_)
            close();
    }

    File& operator=(const File& other)
    {
        if (isOpened_)
            close();
        filePath_ = other.filePath_;
        return *this;
    }

    File& operator=(const char* filePath)
    {
        if (isOpened_)
            close();
        filePath_ = filePath;
        return *this;
    }

    enum class AccessMode
    {
        read,
        readWrite
    };

    enum class OpenMode
    {
        openIfExists,
        createNewDontOverwrite,
        createNewAllowOverwrite,
        openOrCreate,
        openOrCreateAndSeekToEof
    };

    /** Opens the file. It will automatically be closed by the destructor,
     *  or with a call to close()
     */
    bool open(AccessMode accessMode, OpenMode openMode)
    {
        const BYTE rwMode = (accessMode == AccessMode::readWrite)
                                ? (FA_READ | FA_WRITE)
                                : (FA_READ);
        switch (openMode)
        {
            default:
            case OpenMode::openIfExists:
                errorCode_ = f_open(&fileHandle_, filePath_, rwMode | FA_OPEN_EXISTING);
                break;
            case OpenMode::createNewDontOverwrite:
                errorCode_ = f_open(&fileHandle_, filePath_, rwMode | FA_CREATE_NEW);
                break;
            case OpenMode::createNewAllowOverwrite:
                errorCode_ = f_open(&fileHandle_, filePath_, rwMode | FA_CREATE_ALWAYS);
                break;
            case OpenMode::openOrCreate:
                errorCode_ = f_open(&fileHandle_, filePath_, rwMode | FA_OPEN_ALWAYS);
                break;
            case OpenMode::openOrCreateAndSeekToEof:
                errorCode_ = f_open(&fileHandle_, filePath_, rwMode | FA_OPEN_APPEND);
                break;
        }
        if (errorCode_ != FR_OK)
        {
            f_close(&fileHandle_);
            isOpened_ = false;
            return false;
        }
        isOpened_ = true;
        return true;
    }

    /** Closes the file */
    bool close()
    {
        if (isOpened_)
        {
            errorCode_ = f_close(&fileHandle_);
            isOpened_ = false;
            return errorCode_ == FR_OK;
        }
        return false;
    }

    /** Reads up to `numBytesRequested` into `readBuffer`, stores the number
     *  of characters read in `numBytesRead`. Zero terminates the final string in
     *  `readBuffer`. `readBuffer` must be at least `numBytesRequested + 1` bytes long.
     *  Returns true, if the read operation was successful.
     */
    bool tryRead(char* readBuffer, uint32_t numBytesRequested, uint32_t& numBytesRead)
    {
        UINT numRead;
        errorCode_ = f_read(&fileHandle_, readBuffer, numBytesRequested, &numRead);
        numBytesRead = numRead;
        readBuffer[numBytesRead] = 0;
        return errorCode_ == FR_OK;
    }

    /** Reads a full line into the supplied output string. */
    template <size_t stringCapacity>
    bool readLine(FixedSizeStr<stringCapacity>& outputString)
    {
        char* ret = f_gets(outputString.data(), outputString.maxSize(), &fileHandle_);
        if (ret != outputString)
        {
            outputString = "";
            return false;
        }
        // we manipulated the string buffer directly and must update the size
        outputString.updateSize();
        return true;
    }

    size_t getSize() const
    {
        if (!isOpened_)
            return 0;
        return f_size(&fileHandle_);
    }

    bool setCursorTo(size_t position)
    {
        if (!isOpened_)
            return false;
        if (position > f_size(&fileHandle_))
            return false;
        errorCode_ = f_lseek(&fileHandle_, position);
        return errorCode_ == FR_OK;
    }

    bool advanceCursor(size_t numBytes)
    {
        if (!isOpened_)
            return false;
        const auto advancedPositon = f_tell(&fileHandle_) + numBytes;
        return setCursorTo(advancedPositon);
    }

    bool write(const char* string)
    {
        if (!isOpened_)
            return false;
        const auto numWritten = f_puts(string, &fileHandle_);
        return string[numWritten] == 0; // did we write the full string?
    }

    /** Returns true if the file has reach its end and can no longer read more bytes. */
    bool isEndOfFile() const
    {
        return f_eof(&fileHandle_);
    }

    const char* getFilePath() const { return filePath_; }
    FRESULT getLastError() const { return errorCode_; }

private:
    FixedSizeStr<256> filePath_;
    FIL fileHandle_;
    FRESULT errorCode_;
    bool isOpened_;
};

#else // #ifndef UNIT_TEST

// ==========================================================================
// for unit tests, we use a dummy version of the File Class
// ==========================================================================
#    include <functional>
#    include "ff_unitTest.h"
#    include <gtest/gtest.h>

class File
{
public:
    enum class AccessMode
    {
        read,
        readWrite
    };

    enum class OpenMode
    {
        openIfExists,
        createNewDontOverwrite,
        createNewAllowOverwrite,
        openOrCreate,
        openOrCreateAndSeekToEof
    };

    class UnitTestImpl
    {
    public:
        virtual ~UnitTestImpl() {}
        virtual UnitTestImpl& operator=(const UnitTestImpl& other) = 0;
        virtual UnitTestImpl& operator=(const char* filePath) = 0;
        virtual bool open(AccessMode accessMode, OpenMode openMode) = 0;
        virtual bool close() = 0;
        virtual bool tryRead(char* readBuffer, uint32_t numBytesRequested, uint32_t& numBytesRead) = 0;
        virtual bool readLine(FixedSizeStr<1000>& outputString) = 0;
        virtual size_t getSize() const = 0;
        virtual bool setCursorTo(size_t position) = 0;
        virtual bool advanceCursor(size_t numBytes) = 0;
        virtual bool write(const char* string) = 0;
        virtual bool isEndOfFile() const = 0;
        virtual const char* getFilePath() const = 0;
        virtual FRESULT getLastError() const = 0;
    };

    // factory functions, used by each test individually.
    using FactoryFunc = std::function<std::unique_ptr<UnitTestImpl>(const char* filePath)>;
    // key == test name
    static std::map<std::string, FactoryFunc> implFactories_;

    File()
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        impl_ = implFactories_[testName]("");
    }

    File(const char* filePath)
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        impl_ = implFactories_[testName](filePath);
    }

    File(const File& other)
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        impl_ = implFactories_[testName]("");
        if (impl_)
            *impl_ = *other.impl_;
    }

    ~File()
    {
    }

    File& operator=(const File& other)
    {
        if (impl_)
            *impl_ = *other.impl_;
        return *this;
    }

    File& operator=(const char* filePath)
    {
        if (impl_)
            *impl_ = filePath;
        return *this;
    }

    bool open(AccessMode accessMode, OpenMode openMode)
    {
        if (impl_)
            return impl_->open(accessMode, openMode);
        return false;
    }

    bool close()
    {
        if (impl_)
            return impl_->close();
        return false;
    }

    bool tryRead(char* readBuffer, uint32_t numBytesRequested, uint32_t& numBytesRead)
    {
        if (impl_)
            return impl_->tryRead(readBuffer, numBytesRequested, numBytesRead);
        return false;
    }

    template <size_t capacity>
    bool readLine(FixedSizeStr<capacity>& outputString)
    {
        // should be large enough for unit tests
        if (impl_)
        {
            FixedSizeStr<1000> line;
            const auto result = impl_->readLine(line);
            outputString = line.data();
            return result;
        }
        outputString = "";
        return false;
    }

    size_t getSize() const
    {
        if (impl_)
            return impl_->getSize();
        return 0;
    }

    bool setCursorTo(size_t position)
    {
        if (impl_)
            return impl_->setCursorTo(position);
        return false;
    }

    bool advanceCursor(size_t numBytes)
    {
        if (impl_)
            return impl_->advanceCursor(numBytes);
        return false;
    }

    bool write(const char* string)
    {
        if (impl_)
            return impl_->write(string);
        return false;
    }

    bool isEndOfFile() const
    {
        if (impl_)
            return impl_->isEndOfFile();
        return true;
    }

    const char* getFilePath() const
    {
        if (impl_)
            return impl_->getFilePath();
        return "";
    }
    FRESULT getLastError() const
    {
        if (impl_)
            return impl_->getLastError();
        return FR_INT_ERR;
    }

    std::unique_ptr<UnitTestImpl> impl_;
};

#endif // #ifndef UNIT_TEST