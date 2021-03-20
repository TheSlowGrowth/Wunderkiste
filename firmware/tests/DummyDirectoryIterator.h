#pragma once
#define UNIT_TEST // use unit test implementation of File class
#include "DirectoryIterator.h"

// ==============================================================
// A dummy directory iterator for simulating various test scenarios.
// ==============================================================

class DummyDirectoryIterator : public DirectoryIterator::UnitTestImpl
{
public:
    DummyDirectoryIterator(const char* directoryPath) :
        directoryPath_(directoryPath),
        currentIdx_(0)
    {
    }

    ~DummyDirectoryIterator() {}

    bool isValid() const override { return currentIdx_ < getTestEnv().directoryEntries_.size(); }
    bool isDirectoryEndReached() const override { return currentIdx_ >= getTestEnv().directoryEntries_.size(); }
    bool hasError() const override { return false; }
    bool advance() override
    {
        currentIdx_++;
        return isValid();
    }
    const char* getName() const override
    {
        if (isValid())
            return getTestEnv().directoryEntries_[currentIdx_].name.c_str();
        else
            return "";
    }
    bool isDirectory() const override
    {
        if (isValid())
            return getTestEnv().directoryEntries_[currentIdx_].type == Entry::Type::directory;
        else
            return false;
    }

    bool isFile() const override
    {
        if (isValid())
            return getTestEnv().directoryEntries_[currentIdx_].type == Entry::Type::file;
        else
            return false;
    }
    bool isHidden() const override
    {
        if (isValid())
            return getTestEnv().directoryEntries_[currentIdx_].hidden == Entry::Hidden::yes;
        else
            return false;
    }
    bool isSystemFileOrDirectory() const override
    {
        if (isValid())
            return getTestEnv().directoryEntries_[currentIdx_].systemFileOrDir == Entry::SystemFileOrDir::yes;
        else
            return false;
    }
    bool isArchived() const override
    {
        if (isValid())
            return getTestEnv().directoryEntries_[currentIdx_].archived == Entry::Archived::yes;
        else
            return false;
    }
    bool isReadOnly() const override
    {
        if (isValid())
            return getTestEnv().directoryEntries_[currentIdx_].readOnly == Entry::ReadOnly::yes;
        else
            return false;
    }

    const char* getDirectoryPath() const override { return directoryPath_.c_str(); }
    FRESULT getLastError() const override { return FRESULT::FR_INT_ERR; }

    struct Entry
    {
        enum class Hidden
        {
            yes,
            no
        };
        enum class Type
        {
            file,
            directory
        };
        enum class SystemFileOrDir
        {
            yes,
            no
        };
        enum class Archived
        {
            yes,
            no
        };
        enum class ReadOnly
        {
            yes,
            no
        };

        std::string name;
        Type type;
        Hidden hidden;
        SystemFileOrDir systemFileOrDir;
        Archived archived;
        ReadOnly readOnly;
    };

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
        std::vector<Entry> directoryEntries_;
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

private:
    const std::string directoryPath_;
    size_t currentIdx_;
};