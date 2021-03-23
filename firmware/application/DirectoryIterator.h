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

// for unit tests, we use a dummy version of the DirectoryIterator class
#ifndef UNIT_TEST

// ==========================================================================
// This is the fatfs based implementation of the DirectoryIterator class.
// Unit tests use a proxy class - see below.
// ==========================================================================

extern "C"
{
#    include "ff.h"
}
#    include "FixedSizeString.h"

class DirectoryIterator
{
public:
    DirectoryIterator(const char* directoryPath) :
        directoryPath_(directoryPath),
        errorCode_(FR_OK)
    {
        errorCode_ = f_opendir(&dirHandle_, directoryPath_);
        if (errorCode_ != FR_OK)
        {
            // marke as invalid
            fileInfo_.fname[0] = 0;
            return;
        }

        // read the first entry
        advance();
    }

    DirectoryIterator(const DirectoryIterator& other) = delete;

    ~DirectoryIterator()
    {
        f_closedir(&dirHandle_);
    }

    bool isValid() const
    {
        return (errorCode_ == FR_OK) && (fileInfo_.fname[0] != 0);
    }

    bool isDirectoryEndReached() const
    {
        return (errorCode_ == FR_OK) && (fileInfo_.fname[0] == 0);
    }

    bool hasError() const
    {
        return (errorCode_ != FR_OK);
    }

    // iterates to the next file and returns true, if it points to a valid file after
    // the operation
    bool advance()
    {
        errorCode_ = f_readdir(&dirHandle_, &fileInfo_);
        return isValid();
    }

    /** Returns the file or directory name of the current entry or an empty
     *  string if A) there was an error or B) the directory end 
     *  is reached 
     */
    const char* getName() const
    {
        if (errorCode_ != FR_OK)
            return "";
        else
            return fileInfo_.fname;
    }

    bool isDirectory() const
    {
        return isValid() && (fileInfo_.fattrib & AM_DIR);
    }

    bool isFile() const
    {
        return isValid() && !(fileInfo_.fattrib & AM_DIR);
    }

    bool isHidden() const
    {
        return isValid() && (fileInfo_.fattrib & AM_HID);
    }

    bool isSystemFileOrDirectory() const
    {
        return isValid() && (fileInfo_.fattrib & AM_SYS);
    }

    bool isArchived() const
    {
        return isValid() && (fileInfo_.fattrib & AM_ARC);
    }

    bool isReadOnly() const
    {
        return isValid() && (fileInfo_.fattrib & AM_RDO);
    }

    const char* getDirectoryPath() const { return directoryPath_; }
    FRESULT getLastError() const { return errorCode_; }

    FixedSizeStr<256> directoryPath_;
    DIR dirHandle_;
    FILINFO fileInfo_;
    FRESULT errorCode_;
    bool isOpened_;
};

#else // #ifndef UNIT_TEST

// ==========================================================================
// for unit tests, we use a dummy version of the File Class
// ==========================================================================
#    include <functional>
#    include "ff_unitTest.h"

class DirectoryIterator
{
public:
    class UnitTestImpl
    {
    public:
        virtual ~UnitTestImpl() {}
        virtual bool isValid() const = 0;
        virtual bool isDirectoryEndReached() const = 0;
        virtual bool hasError() const = 0;
        virtual bool advance() = 0;
        virtual const char* getName() const = 0;
        virtual bool isDirectory() const = 0;

        virtual bool isFile() const = 0;
        virtual bool isHidden() const = 0;
        virtual bool isSystemFileOrDirectory() const = 0;
        virtual bool isArchived() const = 0;
        virtual bool isReadOnly() const = 0;

        virtual const char* getDirectoryPath() const = 0;
        virtual FRESULT getLastError() const = 0;
    };

    // factory functions, used by each test individually.
    using FactoryFunc = std::function<std::unique_ptr<UnitTestImpl>(const char* directoryPath)>;
    // key == test name
    static std::map<std::string, FactoryFunc> implFactories_;

    DirectoryIterator(const char* directoryPath)
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        impl_ = implFactories_[testName](directoryPath);
    }

    DirectoryIterator(const DirectoryIterator& other) = delete;

    ~DirectoryIterator()
    {
    }

    bool isValid() const
    {
        if (impl_)
            return impl_->isValid();
        return false;
    }

    bool isDirectoryEndReached() const
    {
        if (impl_)
            return impl_->isDirectoryEndReached();
        return true;
    }

    bool hasError() const
    {
        if (impl_)
            return impl_->hasError();
        return true;
    }

    // iterates to the next file and returns true, if it points to a valid file after
    // the operation
    bool advance()
    {
        if (impl_)
            return impl_->advance();
        return false;
    }

    const char* getName() const
    {
        if (impl_)
            return impl_->getName();
        return "";
    }

    bool isDirectory() const
    {
        if (impl_)
            return impl_->isDirectory();
        return false;
    }

    bool isFile() const
    {
        if (impl_)
            return impl_->isFile();
        return false;
    }

    bool isHidden() const
    {
        if (impl_)
            return impl_->isHidden();
        return false;
    }

    bool isSystemFileOrDirectory() const
    {
        if (impl_)
            return impl_->isSystemFileOrDirectory();
        return false;
    }

    bool isArchived() const
    {
        if (impl_)
            return impl_->isArchived();
        return false;
    }

    bool isReadOnly() const
    {
        if (impl_)
            return impl_->isReadOnly();
        return false;
    }

    const char* getDirectoryPath() const
    {
        if (impl_)
            return impl_->getDirectoryPath();
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