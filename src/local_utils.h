#pragma once
#include <steam/steamnetworkingtypes.h>
#include <string>

class LocalUtils
{
public:
    static SteamNetworkingMicroseconds g_logTimeZero;

    // You really gotta wonder what kind of pedantic garbage was
    // going through the minds of people who designed std::string
    // that they decided not to include trim.
    // https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring

    // trim from start (in place)
    static void ltrim(std::string& s);

    // trim from end (in place)
    static void rtrim(std::string& s);

    // We do this because I won't want to figure out how to cleanly shut
    // down the thread that is reading from stdin.
    static void NukeProcess(int rc);

    static void DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg);
    static void FatalError(const char* fmt, ...);
    static void Printf(const char* fmt, ...);
};
