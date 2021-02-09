#include "DummyLibraryFile.h"
#include "DummyDirectoryIterator.h"
#include "Library.h"
#include <vector>
#include <string>
#include <gtest/gtest.h>

// ==============================================================
// Tests
// ==============================================================

class Library_Fixture : public ::testing::Test
{
protected:
    Library_Fixture()
    {
        // init the "pseudo-static" environment for the dummy implementations
        DummyLibraryFile::initTestEnv();
        DummyDirectoryIterator::initTestEnv();

        // install test implementations of File and DirectoryIterator
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        File::implFactories_[testName] = [](const char* filePath) {
            return std::make_unique<DummyLibraryFile>(filePath);
        };
        DirectoryIterator::implFactories_[testName] = [](const char* directoryPath) {
            return std::make_unique<DummyDirectoryIterator>(directoryPath);
        };
    }

    ~Library_Fixture()
    {
        // remove test implementations of File and DirectoryIterator
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        File::implFactories_.erase(testName);
        DirectoryIterator::implFactories_.erase(testName);

        DummyLibraryFile::deleteTestEnv();
        DummyDirectoryIterator::deleteTestEnv();
    }

    void commonEndOfTestChecks()
    {
        // never open library file multiple times simultaneously to avoid disk corruption.
        EXPECT_LE(DummyLibraryFile::getTestEnv().maxObservedNumSimultaneousFilesOpen_, 1);
        // never delete or overwrite any file
        EXPECT_EQ(DummyLibraryFile::getTestEnv().filesOverwritten_.size(), 0u);
        bool libraryFileWasCreated = false;
        for (const auto filename : DummyLibraryFile::getTestEnv().filesInExistance_)
            libraryFileWasCreated |= (filename == "library.txt");
        // never delete or overwrite any file
        EXPECT_TRUE(libraryFileWasCreated);
    }
};

TEST_F(Library_Fixture, a_libraryFileValidity_emptyFile)
{
    // an empty library file is valid
    DummyLibraryFile::getTestEnv().fileContents_ = "";
    Library library;
    EXPECT_TRUE(library.isLibraryFileValid());
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, b_libraryFileValidity_twoEntryFileNoNewline)
{
    // two entries in the file, no newline at the end
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name";
    Library library;
    EXPECT_TRUE(library.isLibraryFileValid());
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, c_libraryFileValidity_twoEntryFileNewline)
{
    // two entries in the file, newline at the end
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;
    EXPECT_TRUE(library.isLibraryFileValid());
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, d_libraryFileValidity_tagHexStrInvalidCharacters)
{
    // hex string for the tag ID is has invalid character
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4X:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;
    EXPECT_FALSE(library.isLibraryFileValid());
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, e_libraryFileValidity_tagHexStrTooShort)
{
    // hex string for the tag ID is too short
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;
    EXPECT_FALSE(library.isLibraryFileValid());
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, f_libraryFileValidity_tagHexStrTooLong)
{
    // hex string for the tag ID is too long
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D1:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;
    EXPECT_FALSE(library.isLibraryFileValid());
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, g_getFolderFor_entryFound)
{
    // getFolderFor() - entry found for the requested tag id
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;

    RfidTagId tag = 0x1A2B3C4D;
    Library::StringType result;
    EXPECT_TRUE(library.getFolderFor(tag, result));
    EXPECT_EQ(result, "A Test Directory Name");
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, h_getFolderFor_pathNotFound)
{
    // getFolderFor() - no entry found for the requested tag id
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;

    RfidTagId tag = 0x11223344;
    Library::StringType result = "some initial value";
    EXPECT_FALSE(library.getFolderFor(tag, result));
    EXPECT_TRUE(result.empty()); // string was cleared
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, i_isLinked_pathFound)
{
    // isLinked() - an entry is found for the requested folder
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;

    FixedSizeStr<32> folderToCheck = "Another Directory Name";
    EXPECT_TRUE(library.isLinked(folderToCheck));
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, j_isLinked_pathNotFound)
{
    // isLinked() - no entry found for the requested folder
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;

    FixedSizeStr<32> folderToCheck = "Another Directory Name with some extra characters";
    EXPECT_FALSE(library.isLinked(folderToCheck));
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, k_isLinked_tagFound)
{
    // isLinked() - an entry is found for the requested tag
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;

    RfidTagId tagToCheck = 0x2A3B4C5D;
    EXPECT_TRUE(library.isLinked(tagToCheck));
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, l_isLinked_tagNotFound)
{
       // isLinked() - an entry is found for the requested tag
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;

    RfidTagId tagToCheck = 0x33445566;
    EXPECT_FALSE(library.isLinked(tagToCheck));
    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, m_storeLink_newLineAlreadyThere)
{
    // link folder when library file ends on a newline
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n";
    Library library;

    RfidTagId tag = 0x11223344;
    FixedSizeStr<32> folderToStore = "My Third Folder";
    EXPECT_TRUE(library.storeLink(tag, folderToStore));

    EXPECT_STREQ(DummyLibraryFile::getTestEnv().fileContents_, "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n11223344:My Third Folder");
    EXPECT_TRUE(library.isLinked(folderToStore));

    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, n_storeLink_newLineNotThere)
{
    // link folder when library file ends without a newline
    DummyLibraryFile::getTestEnv().fileContents_ = "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name";
    Library library;

    RfidTagId tag = 0x11223344;
    FixedSizeStr<32> folderToStore = "My Third Folder";
    EXPECT_TRUE(library.storeLink(tag, folderToStore));

    EXPECT_STREQ(DummyLibraryFile::getTestEnv().fileContents_, "1A2B3C4D:A Test Directory Name\n2A3B4C5D:Another Directory Name\n11223344:My Third Folder");
    EXPECT_TRUE(library.isLinked(folderToStore));

    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, o_storeLink_emptyFile)
{
    // link folder when library is empty
    DummyLibraryFile::getTestEnv().fileContents_ = "";
    Library library;

    RfidTagId tag = 0x11223344;
    FixedSizeStr<32> folderToStore = "My Folder";
    EXPECT_TRUE(library.storeLink(tag, folderToStore));

    // No newline inserted at the beginning of the file
    EXPECT_STREQ(DummyLibraryFile::getTestEnv().fileContents_, "11223344:My Folder");
    EXPECT_TRUE(library.isLinked(folderToStore));

    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, p_storeLink_tagAlreadyInUse)
{
    // link folder when tag is already in use for another folder
    DummyLibraryFile::getTestEnv().fileContents_ = "11223344:My Folder A";
    Library library;

    RfidTagId tag = 0x11223344;
    FixedSizeStr<32> folderToStore = "My Folder B";
    EXPECT_FALSE(library.storeLink(tag, folderToStore)); // fails!

    // no new antry added
    EXPECT_STREQ(DummyLibraryFile::getTestEnv().fileContents_, "11223344:My Folder A");
    EXPECT_FALSE(library.isLinked(folderToStore));

    commonEndOfTestChecks();
}

TEST_F(Library_Fixture, q_getNextUnlinked)
{
    // getNextUnlinked() returns the first unlinked folder

    DummyLibraryFile::getTestEnv().fileContents_ = "11223344:My Folder A";
    DummyDirectoryIterator::getTestEnv().directoryEntries_ = {
        { "My Folder A",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
        { "A file to ignore",
          DummyDirectoryIterator::Entry::Type::file,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
        { "A hidden directory to ignore",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::yes,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
        { "My readonly directory B",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::yes },
        { "A system directory to ignore",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::yes,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
        { "An archived directory to ignore",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::yes,
          DummyDirectoryIterator::Entry::ReadOnly::no },
    };
    Library library;

    Library::StringType path;
    EXPECT_TRUE(library.getNextUnlinkedFolder(path));
    // the first folder is already linked, the following two must be ignored
    EXPECT_STREQ(path, "My readonly directory B");

    // link this directory
    RfidTagId tag = 0x22334455;
    FixedSizeStr<32> folderToStore = "My readonly directory B";
    EXPECT_TRUE(library.storeLink(tag, folderToStore));

    // search again. All other entries must be ignored.
    EXPECT_FALSE(library.getNextUnlinkedFolder(path)) << DummyLibraryFile::getTestEnv().fileContents_.c_str();
    EXPECT_STREQ(path, "");

    commonEndOfTestChecks();
}
