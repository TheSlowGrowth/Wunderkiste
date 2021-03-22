#pragma once
#define UNIT_TEST // use unit test implementation of File class
#include "File.h"

// ==============================================================
// A dummy library file for simulating various test scenarios.
// ==============================================================

class DummyLibraryFile : public File::UnitTestImpl
{
public:
    DummyLibraryFile(const char* filePath) :
        filePath_(filePath),
        readIndex_(int(getTestEnv().fileContents_.size())) // imitate file not opened
    {
    }

    ~DummyLibraryFile() override
    {
        close();
    }

    UnitTestImpl& operator=(const UnitTestImpl& other) override
    {
        filePath_ = other.getFilePath();
        readIndex_ = int(getTestEnv().fileContents_.size()); // imitate file not opened
        return *this;
    }

    DummyLibraryFile& operator=(const char* filePath) override
    {
        filePath_ = filePath;
        readIndex_ = int(getTestEnv().fileContents_.size()); // imitate file not opened
        return *this;
    }

    bool open(File::AccessMode, File::OpenMode openMode) override
    {
        getTestEnv().numSimultaneousFilesOpen_++;
        if (getTestEnv().numSimultaneousFilesOpen_
            > getTestEnv().maxObservedNumSimultaneousFilesOpen_)
            getTestEnv().maxObservedNumSimultaneousFilesOpen_ =
                getTestEnv().numSimultaneousFilesOpen_;
        readIndex_ = 0;

        // remember which the files that were overwritten or created from scratch
        switch (openMode)
        {
            case File::OpenMode::openIfExists:
                break;
            case File::OpenMode::createNewAllowOverwrite:
                getTestEnv().filesOverwritten_.push_back(filePath_.c_str());
                break;
            case File::OpenMode::openOrCreateAndSeekToEof:
                readIndex_ = getSize();
                getTestEnv().filesInExistance_.push_back(filePath_.c_str());
                break;
            case File::OpenMode::createNewDontOverwrite:
            case File::OpenMode::openOrCreate:
                getTestEnv().filesInExistance_.push_back(filePath_.c_str());
                break;
        }

        return true;
    }

    bool close() override
    {
        getTestEnv().numSimultaneousFilesOpen_--;
        return true;
    }

    bool tryRead(char* readBuffer, uint32_t numBytesRequested, uint32_t& numBytesRead) override
    {
        numBytesRead = 0;
        // this is horribly inefficient, but who cares?
        while (numBytesRequested > 0 && !isEndOfFile())
        {
            *readBuffer = getTestEnv().fileContents_[readIndex_];
            readIndex_++;
            numBytesRequested--;
            readBuffer++;
            numBytesRead++;
        }
        readBuffer[numBytesRead] = 0;
        return true;
    }

    size_t getSize() const override
    {
        return getTestEnv().fileContents_.size();
    }

    bool setCursorTo(size_t position) override
    {
        if (position <= getSize())
        {
            readIndex_ = position;
            return true;
        }
        return false;
    }

    bool advanceCursor(size_t numBytes) override
    {
        return setCursorTo(readIndex_ + numBytes);
    }

    bool readLine(FixedSizeStr<1000>& outputString) override
    {
        // this is horribly inefficient, but who cares?
        while ((getTestEnv().fileContents_[readIndex_] != '\n')
               && (outputString.size() < outputString.maxSize())
               && !isEndOfFile())
        {
            outputString.append(getTestEnv().fileContents_[readIndex_]);
            readIndex_++;
        }
        if (getTestEnv().fileContents_[readIndex_] == '\n')
        {
            outputString.append(getTestEnv().fileContents_[readIndex_]);
            readIndex_++;
        }
        return true;
    }

    bool write(const char* string) override
    {
        const auto strLen = std::char_traits<char>::length(string);
        getTestEnv().fileContents_.resetAt(string, readIndex_);
        readIndex_ += strLen;
        if (readIndex_ > int(getTestEnv().fileContents_.maxSize()))
            return false; // capacity exceeded
        return true;
    }

    bool isEndOfFile() const override
    {
        return readIndex_ >= int(getTestEnv().fileContents_.size());
    }

    const char* getFilePath() const override { return filePath_; }
    FRESULT getLastError() const override { return FR_OK; }

    /** Tests running in parallel need to be isolated from each other.
     *  However, we need some sort of "static" variable to control the 
     *  test scenario, because we can't supply a "environment"/pimpl to the
     *  DummyDirectoryIterator instances - they're constructed outside
     *  of our reach.
     *  We solve this by giving each test its own environment which sits in a static
     *  std::map and is selected via the test name.
     */
    struct TestEnvironment
    {
        FixedSizeStr<1000> fileContents_;
        int numSimultaneousFilesOpen_ = 0;
        int maxObservedNumSimultaneousFilesOpen_ = 0;
        std::vector<std::string> filesInExistance_;
        std::vector<std::string> filesOverwritten_;
    };

    static void initTestEnv()
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        testEnvironments_[testName] = TestEnvironment {};
    }

    static void deleteTestEnv()
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        testEnvironments_.erase(testName);
    }

    static TestEnvironment& getTestEnv()
    {
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        return testEnvironments_[testName];
    }

private:
    static std::map<std::string, TestEnvironment> testEnvironments_;
    FixedSizeStr<32> filePath_;
    int readIndex_;
};