#define UNIT_TEST // switch to the unit test implementation of various helper classes

#include "File.h"
#include "DirectoryIterator.h"
#include "UI.h"
#include "Platform.h"

#include "DummyLibraryFile.h"
#include "DummyDirectoryIterator.h"

// static members of various classes that are otherwise header-only
std::map<std::string, File::FactoryFunc> File::implFactories_;
std::map<std::string, DirectoryIterator::FactoryFunc> DirectoryIterator::implFactories_;
std::map<std::string, DummyLibraryFile::TestEnvironment> DummyLibraryFile::testEnvironments_;
std::map<std::string, DummyDirectoryIterator::TestEnvironment> DummyDirectoryIterator::testEnvironments_;

// include various cpp files from the application directory
#include "Library.cpp"

// specify some functions manually to make the linker happy
// TODO: Include these int he Wunderkiste tests.
void LED::setLed(LED::Pattern) {}
void Power::shutdownImmediately() {}
void Power::enableOrResetAutoShutdownTimer() {}
void Power::disableAutoShutdownTimer() {}
