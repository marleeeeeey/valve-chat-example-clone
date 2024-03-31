#include "local_utils.h"

#include <algorithm>
#include <cctype>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

#ifdef _WIN32
#include <windows.h> // Ug, for NukeProcess -- see below
#else
#include <signal.h>
#include <unistd.h>
#endif

SteamNetworkingMicroseconds LocalUtils::g_logTimeZero = 0;

void LocalUtils::ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

void LocalUtils::rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

void LocalUtils::NukeProcess(int rc)
{
#ifdef _WIN32
    ExitProcess(rc);
#else
    (void)rc; // Unused formal parameter
    kill(getpid(), SIGKILL);
#endif
}

void LocalUtils::DebugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg)
{
    SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - g_logTimeZero;
    printf("%10.6f %s\n", time * 1e-6, pszMsg);
    fflush(stdout);
    if (eType == k_ESteamNetworkingSocketsDebugOutputType_Bug)
    {
        fflush(stdout);
        fflush(stderr);
        NukeProcess(1);
    }
}

void LocalUtils::FatalError(const char* fmt, ...)
{
    char text[2048];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(text, fmt, ap);
    va_end(ap);
    char* nl = strchr(text, '\0') - 1;
    if (nl >= text && *nl == '\n')
        *nl = '\0';
    DebugOutput(k_ESteamNetworkingSocketsDebugOutputType_Bug, text);
}

void LocalUtils::Printf(const char* fmt, ...)
{
    char text[2048];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(text, fmt, ap);
    va_end(ap);
    char* nl = strchr(text, '\0') - 1;
    if (nl >= text && *nl == '\n')
        *nl = '\0';
    DebugOutput(k_ESteamNetworkingSocketsDebugOutputType_Msg, text);
}
