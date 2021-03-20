#include <gtest/gtest.h>

#define UNIT_TEST_FRIEND_CLASS friend class Wunderkiste_Fixture
#define UNIT_TEST // enable unit test implementations of File and DirectoryIterator
#include "Wunderkiste.h"
#include <functional>

#include "DummyLibraryFile.h"
#include "DummyDirectoryIterator.h"

// ==============================================================
// A Dummy player that executes lambdas for each of its functions
// and keeps track of a few simple things by itself.
// ==============================================================

class DummyPlayer : public DirectoryPlayer
{
public:
    DummyPlayer() = default;
    ~DummyPlayer() = default;

    std::function<void(const char*)> startPlayingDirectoryFunc_;
    void startPlayingDirectory(const char* directoryPath) override
    {
        currentFolderPlayed_ = directoryPath;
        currentTrackPlayed_ = 0;
        if (startPlayingDirectoryFunc_)
            startPlayingDirectoryFunc_(directoryPath);
    }

    std::function<bool(void)> isPlayingFunc_;
    bool isPlaying() const override
    {
        if (isPlayingFunc_)
            return isPlayingFunc_();
        else
            return !currentFolderPlayed_.empty();
    }

    std::function<void(void)> goToPreviousTrackFunc_;
    void goToPreviousTrack() override
    {
        currentTrackPlayed_--;
        if (goToPreviousTrackFunc_)
            goToPreviousTrackFunc_();
    }

    std::function<void(void)> goToNextTrackFunc_;
    void goToNextTrack() override
    {
        currentTrackPlayed_++;
        if (goToNextTrackFunc_)
            goToNextTrackFunc_();
    }

    std::function<void(void)> stopPlayingFunc_;
    void stopPlaying() override
    {
        currentFolderPlayed_ = "";
        if (stopPlayingFunc_)
            stopPlayingFunc_();
    }

    std::string currentFolderPlayed_;
    int currentTrackPlayed_ = 0;
};

// ==============================================================
// Helpers for verbose printing via gtest
// ==============================================================

inline std::ostream& operator<<(std::ostream& os, const Wunderkiste::State& state)
{
    switch (state)
    {
        case Wunderkiste::State::startup:
            return os << "startup";
        case Wunderkiste::State::unrecoverableError:
            return os << "unrecoverableError";
        case Wunderkiste::State::linkWaitingForTag:
            return os << "linkWaitingForTag";
        case Wunderkiste::State::linkSuccessfulWaitingForTagRemove:
            return os << "linkSuccessfulWaitingForTagRemove";
        case Wunderkiste::State::linkErrorWaitingForTagRemove:
            return os << "linkErrorWaitingForTagRemove";
        case Wunderkiste::State::waitingForTag:
            return os << "waitingForTag";
        case Wunderkiste::State::playing:
            return os << "playing";
        case Wunderkiste::State::stoppedWaitingForTagRemove:
            return os << "stoppedWaitingForTagRemove";
        default:
            os << "UNKNOWN!";
    }
    return os;
}

// ==============================================================
// Tests
// ==============================================================

class Wunderkiste_Fixture : public ::testing::Test
{
protected:
    Wunderkiste_Fixture()
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

    ~Wunderkiste_Fixture()
    {
        // remove test implementations of File and DirectoryIterator
        const auto testName = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        File::implFactories_.erase(testName);
        DirectoryIterator::implFactories_.erase(testName);

        DummyLibraryFile::deleteTestEnv();
        DummyDirectoryIterator::deleteTestEnv();
    }

    void manuallyEnterState(Wunderkiste::State state)
    {
        app_->state_ = state; // we're a friend class, we're allowed to do that :-)
    }

    UiEventQueue uiEventQueue_;
    DummyPlayer player_;
    std::unique_ptr<Wunderkiste> app_;
};

TEST_F(Wunderkiste_Fixture, a_startupState)
{
    // expects the application to start in the startup state
    app_ = std::make_unique<Wunderkiste>(uiEventQueue_, player_);
    EXPECT_EQ(app_->getState(), Wunderkiste::State::startup);
}

TEST_F(Wunderkiste_Fixture, b_alwaysPopSingleEventFromQueue)
{
    // every call to handleEvents() should remove a single UiEvent from the event queue

    app_ = std::make_unique<Wunderkiste>(uiEventQueue_, player_);

    std::vector<Wunderkiste::State> statesToCheck = {
        Wunderkiste::State::startup,
        Wunderkiste::State::unrecoverableError,
        Wunderkiste::State::linkWaitingForTag,
        Wunderkiste::State::linkSuccessfulWaitingForTagRemove,
        Wunderkiste::State::linkErrorWaitingForTagRemove,
        Wunderkiste::State::waitingForTag,
        Wunderkiste::State::playing,
        Wunderkiste::State::stoppedWaitingForTagRemove
    };

    for (auto state : statesToCheck)
    {
        manuallyEnterState(state);
        // push two events
        uiEventQueue_.pushEvent({ UiEvent::Type::prevBttnPressed, { 0 } });
        uiEventQueue_.pushEvent({ UiEvent::Type::prevBttnPressed, { 0 } });
        app_->handleEvents();
        // expect one to be popped
        EXPECT_EQ(uiEventQueue_.getNumEvents(), 1u) << "in Wunderkiste::State::" << state;
        // pop all remaining for the next cycle
        while (uiEventQueue_.popEvent().isValid())
            ;
    }
}
TEST_F(Wunderkiste_Fixture, c_enterErrorStateOnInvalidLibFile)
{
    // expects application to enter the error state when library file is
    // ill-formatted

    // prepare environment
    DummyLibraryFile::getTestEnv().fileContents_ = "This file is so broken, it hurts!";

    // create app
    app_ = std::make_unique<Wunderkiste>(uiEventQueue_, player_);

    // first call to handleEvents()
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::unrecoverableError);
}

TEST_F(Wunderkiste_Fixture, d_linking)
{
    // expects application to enter the linking process in first call to
    // handleEvents(), then proceed to link all unlinked folders.

    // prepare environment
    DummyLibraryFile::getTestEnv().fileContents_ = "11223344:My Folder B";
    DummyDirectoryIterator::getTestEnv().directoryEntries_ = {
        { "My Folder A",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
        { "My Folder B",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
        { "My Folder C",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
    };

    // create app
    app_ = std::make_unique<Wunderkiste>(uiEventQueue_, player_);
    EXPECT_EQ(app_->getState(), Wunderkiste::State::startup);

    // ========================================
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkWaitingForTag);
    // Expect "My Folder A" to be played for linking
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder A");
    EXPECT_EQ(player_.currentTrackPlayed_, 0); // no skipping forwards or backwards!

    // ========================================
    // no user input... expect that nothing changed
    bool playbackWasRestarted = false;
    player_.startPlayingDirectoryFunc_ = [&](const char*) {
        playbackWasRestarted = true;
    };
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkWaitingForTag);
    // Playback wasn't restarted ...
    EXPECT_FALSE(playbackWasRestarted);
    // ... or stopped ...
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder A");
    // ... or skipped around.
    EXPECT_EQ(player_.currentTrackPlayed_, 0);

    // ========================================
    // user added an RFID tag
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagAdded, { 0x22334455 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkSuccessfulWaitingForTagRemove);
    // Link was added to the library now.
    {
        Library lib;
        EXPECT_TRUE(lib.isLibraryFileValid());
        EXPECT_TRUE(lib.isLinked(0x22334455));
        EXPECT_TRUE(lib.isLinked("My Folder A"));
    }
    // Playback wasn't restarted ...
    EXPECT_FALSE(playbackWasRestarted);
    // ... or stopped ...
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder A");
    // ... or skipped around.
    EXPECT_EQ(player_.currentTrackPlayed_, 0);
    player_.startPlayingDirectoryFunc_ = nullptr; // reset

    // ========================================
    // user removed the RFID tag
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagRemoved, { 0 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkWaitingForTag);
    // Expect "My Folder C" to be played for linking
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder C");
    EXPECT_EQ(player_.currentTrackPlayed_, 0); // no skipping forwards or backwards!

    // ========================================
    // user added another RFID tag
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagAdded, { 0x33445566 } });
    playbackWasRestarted = false;
    player_.startPlayingDirectoryFunc_ = [&](const char*) {
        playbackWasRestarted = true;
    };
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkSuccessfulWaitingForTagRemove);
    // Link was added to the library now.
    {
        Library lib;
        EXPECT_TRUE(lib.isLibraryFileValid());
        EXPECT_TRUE(lib.isLinked(0x33445566));
        EXPECT_TRUE(lib.isLinked("My Folder C"));
    }
    // Playback wasn't restarted ...
    EXPECT_FALSE(playbackWasRestarted);
    // ... or stopped ...
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder C");
    // ... or skipped around.
    EXPECT_EQ(player_.currentTrackPlayed_, 0);
    player_.startPlayingDirectoryFunc_ = nullptr; // reset

    // ========================================
    // user removed the RFID tag
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagRemoved, { 0 } });
    app_->handleEvents();
    // all folders linked, waiting for playback.
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);
    // Expect no playback running
    EXPECT_FALSE(player_.isPlaying());
}

TEST_F(Wunderkiste_Fixture, e_linking_retryOnError)
{
    // expects application to retry linking

    // prepare environment
    DummyLibraryFile::getTestEnv().fileContents_ = "11223344:My Folder B";
    DummyDirectoryIterator::getTestEnv().directoryEntries_ = {
        { "My Folder A",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
        { "My Folder B",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no },
    };

    // create app
    app_ = std::make_unique<Wunderkiste>(uiEventQueue_, player_);
    EXPECT_EQ(app_->getState(), Wunderkiste::State::startup);

    // ========================================
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkWaitingForTag);
    // Expect "My Folder A" to be played for linking
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder A");
    EXPECT_EQ(player_.currentTrackPlayed_, 0); // no skipping forwards or backwards!

    // ========================================
    // user added an RFID tag that was already in use.
    bool playbackWasRestarted = false;
    player_.startPlayingDirectoryFunc_ = [&](const char*) {
        playbackWasRestarted = true;
    };
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagAdded, { 0x11223344 } }); // tag is in use!
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkErrorWaitingForTagRemove);
    // Playback wasn't restarted ...
    EXPECT_FALSE(playbackWasRestarted);
    // ... or stopped ...
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder A");
    // ... or skipped around.
    EXPECT_EQ(player_.currentTrackPlayed_, 0);

    // ========================================
    // user removed the RFID tag
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagRemoved, { 0 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkWaitingForTag);
    // Playback wasn't restarted ...
    EXPECT_FALSE(playbackWasRestarted);
    // ... or stopped ...
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder A");
    // ... or skipped around.
    EXPECT_EQ(player_.currentTrackPlayed_, 0);

    // ========================================
    // user added another RFID tag
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagAdded, { 0x22334455 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::linkSuccessfulWaitingForTagRemove);
    // Link was added to the library now.
    {
        Library lib;
        EXPECT_TRUE(lib.isLibraryFileValid());
        EXPECT_TRUE(lib.isLinked(0x22334455));
        EXPECT_TRUE(lib.isLinked("My Folder A"));
    }
    // Playback wasn't restarted ...
    EXPECT_FALSE(playbackWasRestarted);
    // ... or stopped ...
    EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "My Folder A");
    // ... or skipped around.
    EXPECT_EQ(player_.currentTrackPlayed_, 0);
}

TEST_F(Wunderkiste_Fixture, f_linking_ignoreButtonPresses)
{
    // while linking, button presses do nothing.

    app_ = std::make_unique<Wunderkiste>(uiEventQueue_, player_);

    std::vector<Wunderkiste::State> statesToCheck = {
        Wunderkiste::State::linkWaitingForTag,
        Wunderkiste::State::linkSuccessfulWaitingForTagRemove,
        Wunderkiste::State::linkErrorWaitingForTagRemove,
    };

    for (auto state : statesToCheck)
    {
        manuallyEnterState(state);
        player_.currentFolderPlayed_ = "Hey, I'm still playing something!";
        player_.currentTrackPlayed_ = 0;

        // push prev button does nothing while linking
        uiEventQueue_.pushEvent({ UiEvent::Type::prevBttnPressed, { 0 } });
        app_->handleEvents();
        // expect state to be unchanged
        EXPECT_EQ(app_->getState(), state);

        // push next button does nothing while linking
        uiEventQueue_.pushEvent({ UiEvent::Type::nextBttnPressed, { 0 } });
        app_->handleEvents();
        // expect state to be unchanged
        EXPECT_EQ(app_->getState(), state);

        // Playback wasn't started or stopped ...
        EXPECT_STREQ(player_.currentFolderPlayed_.c_str(), "Hey, I'm still playing something!");
        // ... or skipped around.
        EXPECT_EQ(player_.currentTrackPlayed_, 0);
    }
}

TEST_F(Wunderkiste_Fixture, g_playback)
{
    // normal playback

    // prepare environment
    DummyLibraryFile::getTestEnv().fileContents_ = "11223344:My Folder";
    DummyDirectoryIterator::getTestEnv().directoryEntries_ = {
        { "My Folder",
          DummyDirectoryIterator::Entry::Type::directory,
          DummyDirectoryIterator::Entry::Hidden::no,
          DummyDirectoryIterator::Entry::SystemFileOrDir::no,
          DummyDirectoryIterator::Entry::Archived::no,
          DummyDirectoryIterator::Entry::ReadOnly::no }
    };

    // create app
    app_ = std::make_unique<Wunderkiste>(uiEventQueue_, player_);
    EXPECT_EQ(app_->getState(), Wunderkiste::State::startup);

    app_->handleEvents();
    // no unlinked folder, we should enter the "waitingForTag" state
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);

    // with no tag on the RFID reader, nothing should change
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);
    uiEventQueue_.pushEvent({ UiEvent::Type::prevBttnPressed, { 0 } }); // no effect
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);
    uiEventQueue_.pushEvent({ UiEvent::Type::nextBttnPressed, { 0 } }); // no effect
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);

    // place a tag on the reader, but the tag is not linked to anything
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagAdded, { 0x22334455 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagRemoved, { 0 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);

    // user placed a tag on the reader that IS linked
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagAdded, { 0x11223344 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::playing);
    // player starts playing the folder
    EXPECT_STREQ(player_.currentFolderPlayed_.data(), "My Folder");
    EXPECT_EQ(player_.currentTrackPlayed_, 0);
    EXPECT_TRUE(player_.isPlaying()); // just to be sure...

    // without any action from the outside world, the player just keeps playing
    auto playbackWasRestarted = false;
    player_.startPlayingDirectoryFunc_ = [&](const char*) {
        playbackWasRestarted = true;
    };
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::playing);
    EXPECT_STREQ(player_.currentFolderPlayed_.data(), "My Folder");
    EXPECT_EQ(player_.currentTrackPlayed_, 0);
    EXPECT_TRUE(player_.isPlaying());
    EXPECT_FALSE(playbackWasRestarted);

    // react to button presses...
    uiEventQueue_.pushEvent({ UiEvent::Type::nextBttnPressed, { 0 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::playing);
    EXPECT_STREQ(player_.currentFolderPlayed_.data(), "My Folder");
    EXPECT_EQ(player_.currentTrackPlayed_, 1); // next track plays now
    EXPECT_TRUE(player_.isPlaying());
    EXPECT_FALSE(playbackWasRestarted);

    // react to button presses...
    uiEventQueue_.pushEvent({ UiEvent::Type::prevBttnPressed, { 0 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::playing);
    EXPECT_STREQ(player_.currentFolderPlayed_.data(), "My Folder");
    EXPECT_EQ(player_.currentTrackPlayed_, 0); // previous track plays now
    EXPECT_TRUE(player_.isPlaying());
    EXPECT_FALSE(playbackWasRestarted);

    // stop playing when tag is removed
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagRemoved, { 0 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);
    EXPECT_STREQ(player_.currentFolderPlayed_.data(), "");
    EXPECT_FALSE(player_.isPlaying());

    // user placed a tag on the reader that IS linked
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagAdded, { 0x11223344 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::playing);
    // player starts playing the folder again
    EXPECT_STREQ(player_.currentFolderPlayed_.data(), "My Folder");
    EXPECT_EQ(player_.currentTrackPlayed_, 0);
    EXPECT_TRUE(player_.isPlaying()); // just to be sure...
    EXPECT_TRUE(playbackWasRestarted);
    playbackWasRestarted = false; // reset

    // When the player has finished playing, the "stoppedWaitingForTagRemove"
    // state must be entered
    player_.isPlayingFunc_ = []() { return false; };
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::stoppedWaitingForTagRemove);

    // in this state, button presses do nothing
    uiEventQueue_.pushEvent({ UiEvent::Type::prevBttnPressed, { 0 } }); // no effect
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::stoppedWaitingForTagRemove);
    EXPECT_FALSE(player_.isPlaying());
    uiEventQueue_.pushEvent({ UiEvent::Type::nextBttnPressed, { 0 } }); // no effect
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::stoppedWaitingForTagRemove);
    EXPECT_FALSE(player_.isPlaying());

    // once tag is removed, we return to the "waitingForTag" state
    uiEventQueue_.pushEvent({ UiEvent::Type::rfidTagRemoved, { 0 } });
    app_->handleEvents();
    EXPECT_EQ(app_->getState(), Wunderkiste::State::waitingForTag);
}