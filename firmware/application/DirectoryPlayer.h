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
#include "AudioStreamPlayer.h"
#include "AudioFileStream.h"
#include "Containers.h"
#include "DirectoryIterator.h"

class DirectoryPlayer
{
public:
    DirectoryPlayer() {}
    virtual ~DirectoryPlayer() = default;
    /** starts playing back all audio files from the provided directory in alphabetical order. */
    virtual void startPlayingDirectory(const char* directoryPath) = 0;
    /** Returns true while it's playing. */
    virtual bool isPlaying() const = 0;
    /** Restarts the current track if the playtime is <= 2s or starts playing the previous track. */
    virtual void goToPreviousTrack() = 0;
    /** Jumps to the next track. */
    virtual void goToNextTrack() = 0;
    /** Stopa playback. */
    virtual void stopPlaying() = 0;

private:
    DirectoryPlayer(const DirectoryPlayer&) = delete;
    DirectoryPlayer& operator=(const DirectoryPlayer&) { return *this; };
};

template <typename StreamPlayerType>
class Mp3DirectoryPlayer : public DirectoryPlayer, public StreamProvider
{
public:
    Mp3DirectoryPlayer(StreamPlayerType& streamPlayer) :
        streamPlayer_(streamPlayer),
        nextAction_(NextAction::restartFile),
        currentFileIndex_(0)
    {
    }
    Mp3DirectoryPlayer(const Mp3DirectoryPlayer&) = delete;

    void startPlayingDirectory(const char* directoryPath) override
    {
        updateAndSortFileList(directoryPath);
        nextAction_ = NextAction::restartFile;
        streamPlayer_.startPlayingNextStreamFrom(*this);
    }

    bool isPlaying() const override { return currentFileIndex_ < files_.size(); }

    void goToPreviousTrack() override
    {
        if (isPlaying())
        {
            if ((fileStream_.getNumSecondsPlayed() <= 5.0f)
                && (currentFileIndex_ > 0))
                nextAction_ = NextAction::prevFile;
            else
                nextAction_ = NextAction::restartFile;

            // stop the stream so that it completes and getNextStream()
            // gets called.
            fileStream_.abortStream();
        }
    }

    void goToNextTrack() override
    {
        if (isPlaying())
        {
            // don't go to the next file if we're already playing the last file in the list.
            if (currentFileIndex_ + 1 == files_.size())
                return;

            nextAction_ = NextAction::nextFile;

            // stop the stream so that it completes and getNextStream()
            // gets called.
            fileStream_.abortStream();
        }
    }

    void stopPlaying() override
    {
        if (isPlaying())
        {
            nextAction_ = NextAction::stop;

            // stop the stream so that it completes and getNextStream()
            // gets called.
            fileStream_.abortStream();
        }
    }

    StereoAudioSampleStream* getNextStream() override
    {
        switch (nextAction_)
        {
            case NextAction::goOn:
                // We were called because the current file has completed playing.
                // Start the next file if there is one.
                currentFileIndex_++;
                nextAction_ = NextAction::goOn;
                break;
            case NextAction::nextFile:
                // We were called because the current file was aborted when
                // the user pressed the next button.
                currentFileIndex_++;
                nextAction_ = NextAction::goOn;
                break;
            case NextAction::restartFile:
                // We were called because the current file was restarted when
                // the user pressed the prev button
                // Nothing to do here, the file will be restarted below.
                nextAction_ = NextAction::goOn;
                break;
            case NextAction::prevFile:
                // We were called because the current file was aborted when
                // the user pressed the prev button
                if (currentFileIndex_ > 0)
                    currentFileIndex_--;
                nextAction_ = NextAction::goOn;
                break;
            case NextAction::stop:
                // We were called because the current file was aborted so that
                // playback can stop.
                currentFileIndex_ = files_.size();
                break;
        }

        if (currentFileIndex_ >= files_.size())
            return nullptr;
        else
        {
            if (fileStream_.restartWithFile(files_[currentFileIndex_]))
                return &fileStream_;
            else
                return nullptr;
        }
    }

    void streamCompleted(StereoAudioSampleStream*) override {}

private:
    void updateAndSortFileList(const char* directoryPath)
    {
        files_.clear();
        DirectoryIterator dirIt(directoryPath);

        while (dirIt.isValid())
        {
            if (!dirIt.isFile()
                || dirIt.isHidden()
                || dirIt.isSystemFileOrDirectory())
            {
                dirIt.advance();
                continue;
            }

            FixedSizeStr<128> filePath;

            // combine full file path
            filePath = directoryPath;
            filePath.append('/');
            filePath.append(dirIt.getName());

            if (!filePath.endsWithIgnoringCase(".mp3"))
            {
                dirIt.advance();
                continue;
            }

            files_.add(filePath);
            dirIt.advance();
        }

        files_.sortAscending();
        currentFileIndex_ = 0;
    }

    enum class NextAction
    {
        prevFile,
        restartFile,
        nextFile,
        stop,
        goOn
    };

    StreamPlayerType& streamPlayer_;
    Mp3FileStream fileStream_;
    NextAction nextAction_;
    size_t currentFileIndex_;
    StaticVector<FixedSizeStr<128>, 128> files_;
};