#include "get_version.h" // IWYU pragma: associated

#if (defined(_WIN32) || defined(MINGW)) && !defined(GIT_VERSION) && !defined(CROSS_LINUX) && !defined(_MSC_VER)

#ifndef VERSION
#define VERSION "CDDA-Breeze 2.2"
#endif

#else

#include "version.h"

#endif

const char *getVersionString()
{
    return VERSION;
}
