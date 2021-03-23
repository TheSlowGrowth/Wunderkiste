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
#include <stdint.h>
#include "FixedSizeString.h"
#include "RFID.h"

class Library
{
public:
    using StringType = FixedSizeStr<256>;

    Library();
    Library(const Library&) = delete;

    /** Returns true if the library file has a valid format. */
    bool isLibraryFileValid() { return isValid_; }

    /** Searches the filesystem root for directories that don't yet
     *  Have an entry in the library. When all directories are linked,
     *  this returns false and path == "".
     *  When an unlicked directory is found, this returns true and
     *  path contains the name of the unlicked directory.
     */
    bool getNextUnlinkedFolder(StringType& path);

    /** Searches the library for an entry for the given RFID tag.
     *  When an entry is found, path contains the linked folder name
     *  and the return value is true;
     *  When no entry is found, path is empty and the return valie is false;
     */
    bool getFolderFor(const RfidTagId& tag, StringType& path) const;

    /** Returns true, if the given path has an entry in the library */
    bool isLinked(const StringType& path) const;

    /** Returns true, if the given tag has an entry in the library */
    bool isLinked(const RfidTagId& tag) const;

    /** Saves a link in the library and returns true if successful. 
     *  If the link is already there, the operation will fail.
     */
    bool storeLink(const RfidTagId& tag, const StringType& path) const;

private:
    bool checkLibraryFile();
    static FixedSizeStr<9> getPrefixStr(const RfidTagId& tag);

    static const char* libraryFilePath_;
    bool isValid_;
};