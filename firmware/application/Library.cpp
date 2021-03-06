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

#include "Library.h"
#include "File.h"
#include "DirectoryIterator.h"

Library::Library() :
    isValid_(checkLibraryFile())
{
}

bool Library::getNextUnlinkedFolder(StringType& path)
{
    DirectoryIterator iterator(""); // open root directory

    while (iterator.isValid())
    {
        if (iterator.isFile()
            || iterator.isSystemFileOrDirectory()
            || iterator.isHidden()
            || iterator.isArchived())
        {
            iterator.advance();
            continue;
        }

        // entry is a normal directory
        path = iterator.getName();

        // skip hidden directories that don't actually have the "hidden" flag
        // but are hidden via their filename
        if (path[0] == '.')
        {
            iterator.advance();
            continue;
        }

        // check if this folder is linked
        if (!isLinked(path))
            return true;

        iterator.advance();
    }

    // all folders are linked
    path = "";
    return false;
}

bool Library::getFolderFor(const RfidTagId& tag, StringType& path) const
{
    File libFile(libraryFilePath_);

    // check if file opens at all
    if (!libFile.open(File::AccessMode::read, File::OpenMode::openOrCreate))
        return false;

    FixedSizeStr<9> prefixStr = getPrefixStr(tag);

    // read line by line and check format
    while (!libFile.isEndOfFile())
    {
        // we reuse the input string as a line buffer.
        StringType& line = path;
        if (!libFile.readLine(line))
            return false;
        // must store at least the RFID-ID, the ":" and 1+ characters for the foldername
        if (line.size() < 8 /* RFID ID */ + 1 /* : */ + 1 /* Foldername */)
            continue; // skip this line. This is weird, the file must be corrupted

        if (line.startsWith(prefixStr))
        {
            // truncate the FRID-ID and ":" from the beginning
            line.removePrefix(9);
            // path already contains the file path.
            // Just remove any remaining trailing newlines
            if (line.endsWith("\n"))
                line.removeSuffix(1);
            return true;
        }
    }

    path = "";
    return false;
}

bool Library::isLinked(const StringType& path) const
{
    File libFile(libraryFilePath_);

    // check if file opens at all
    if (!libFile.open(File::AccessMode::read, File::OpenMode::openOrCreate))
        return false;

    // read line by line
    while (!libFile.isEndOfFile())
    {
        StringType line;
        if (!libFile.readLine(line))
            return false;
        // must store at least the RFID-ID, the ":" and 1+ characters for the foldername
        if (line.size() < 8 /* RFID ID */ + 1 /* : */ + 1 /* Foldername */)
            continue; // skip this line. This is weird, the file must be corrupted

        // truncate the FRID-ID and ":" from the beginning
        line.removePrefix(9);
        // remove any trailing newlines
        if (line.endsWith("\n"))
            line.removeSuffix(1);
        if (line == path)
            return true;
    }

    return false;
}

bool Library::isLinked(const RfidTagId& tag) const
{
    File libFile(libraryFilePath_);

    // check if file opens at all
    if (!libFile.open(File::AccessMode::read, File::OpenMode::openOrCreate))
        return false;

    const FixedSizeStr<9> prefixStr = getPrefixStr(tag);

    // read line by line
    while (!libFile.isEndOfFile())
    {
        StringType line;
        if (!libFile.readLine(line))
            return false;
        if (line.startsWith(prefixStr))
            return true;
    }

    return false;
}

bool Library::storeLink(const RfidTagId& tag, const StringType& path) const
{
    if (isLinked(path))
        return false;
    if (isLinked(tag))
        return false;

    File libFile(libraryFilePath_);

    // check if file opens at all
    if (!libFile.open(File::AccessMode::readWrite, File::OpenMode::openOrCreate))
        return false;

    StringType stringToWrite = getPrefixStr(tag);
    stringToWrite.append(path);

    // check if last character is a newline already
    if (libFile.getSize() > 0)
    {
        if (!libFile.setCursorTo(libFile.getSize() - 1))
            return false;
        char lastCharacterInFile[2]; // space for terminating zero...
        uint32_t numBytesRead = 0;
        if (!libFile.tryRead(lastCharacterInFile, 1, numBytesRead))
            return false;
        // skip to file end
        if (!libFile.setCursorTo(libFile.getSize()))
            return false;
        if (lastCharacterInFile[0] != '\n')
            if (!libFile.write("\n"))
                return false;
    }

    // finally write the string
    if (!libFile.write(stringToWrite))
        return false;

    // manually close the file to catch disk errors when flusing the buffers
    return libFile.close();
}

bool Library::checkLibraryFile()
{
    File libFile(libraryFilePath_);

    // check if file opens at all
    if (!libFile.open(File::AccessMode::read, File::OpenMode::openOrCreate))
        return false;

    // read line by line and check format
    while (!libFile.isEndOfFile())
    {
        StringType line;
        if (!libFile.readLine(line))
            return false;
        // must store at least the RFID-ID, the ":" and 1+ characters for the foldername
        if (line.size() < 8 /* RFID ID */ + 1 /* : */ + 1 /* Foldername */)
            return false;
        // first 8 characters must be HEX
        for (int i = 0; i < 8; i++)
        {
            const char c = line.data()[i];
            const bool is0to9 = (c >= '0') && (c <= '9');
            const bool isAtoF = (c >= 'A') && (c <= 'F');
            if (!(is0to9 || isAtoF))
                return false;
        }
        // 9th character must be ":"
        if (line.data()[8] != ':')
            return false;
        // okay, seems good! Go on with next line
    }

    return true;
}

FixedSizeStr<9> Library::getPrefixStr(const RfidTagId& tag)
{
    FixedSizeStr<9> result = tag.asString();
    // then add a ':'
    result.append(':');
    return result;
}

const char* Library::libraryFilePath_ = "library.txt";