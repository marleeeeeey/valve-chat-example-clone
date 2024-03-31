#pragma once
#include <steam/steamnetworkingsockets.h>

struct AppOptions
{
    bool bServer = false;
    bool bClient = false;
    int nPort = 0;
    SteamNetworkingIPAddr addrServer;
};

AppOptions ReadAppOptions(int argc, const char* argv[]);
