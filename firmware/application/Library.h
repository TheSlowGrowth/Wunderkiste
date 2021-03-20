#pragma once
#include <stdint.h>
#include "FixedSizeString.h"
#include "RFID.h"

class Library
{
public:
    using StringType = FixedSizeStr<128>;

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