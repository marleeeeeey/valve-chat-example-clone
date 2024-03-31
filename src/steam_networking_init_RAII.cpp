#include "steam_networking_init_RAII.h"

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#ifndef STEAMNETWORKINGSOCKETS_OPENSOURCE
#include <steam/steam_api.h>
#endif

#include <local_utils.h>

SteamNetworkingInitRAII::SteamNetworkingInitRAII()
{
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
    SteamDatagramErrMsg errMsg;
    if (!GameNetworkingSockets_Init(nullptr, errMsg))
        throw std::runtime_error("[SteamNetworkingInitRAII] " + std::string(errMsg));
#else
    SteamDatagram_SetAppID(570); // Just set something, doesn't matter what
    SteamDatagram_SetUniverse(false, k_EUniverseDev);

    SteamDatagramErrMsg errMsg;
    if (!SteamDatagramClient_Init(errMsg))
        throw std::runtime_error("[SteamNetworkingInitRAII] " + std::string(errMsg));

    // Disable authentication when running with Steam, for this
    // example, since we're not a real app.
    //
    // Authentication is disabled automatically in the open-source
    // version since we don't have a trusted third party to issue
    // certs.
    SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1);
#endif

    // TODO: remove it from here.
    LocalUtils::g_logTimeZero = SteamNetworkingUtils()->GetLocalTimestamp();
    SteamNetworkingUtils()->SetDebugOutputFunction(
        k_ESteamNetworkingSocketsDebugOutputType_Msg, LocalUtils::DebugOutput);
}
SteamNetworkingInitRAII::~SteamNetworkingInitRAII()
{
    // Give connections time to finish up.  This is an application layer protocol
    // here, it's not TCP.  Note that if you have an application and you need to be
    // more sure about cleanup, you won't be able to do this.  You will need to send
    // a message and then either wait for the peer to close the connection, or
    // you can pool the connection to see if any reliable data is pending.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
    GameNetworkingSockets_Kill();
#else
    SteamDatagramClient_Kill();
#endif
}
