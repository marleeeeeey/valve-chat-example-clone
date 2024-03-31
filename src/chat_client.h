#pragma once
#include <non_blocking_console_user_input.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>

class ChatClient
{
    NonBlockingConsoleUserInput& nonBlockingConsoleUserInput;
    std::atomic<bool>& quitFlag;
    HSteamNetConnection m_hConnection;
    ISteamNetworkingSockets* m_pInterface;
public:
    ChatClient(NonBlockingConsoleUserInput& nonBlockingConsoleUserInput, std::atomic<bool>& quitFlag);
    void Run(const SteamNetworkingIPAddr& serverAddr);
private:
    void PollIncomingMessages();
    void PollLocalUserInput();
private: // OnSteamNetConnectionStatusChanged stuff.
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);
    static ChatClient* s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo);
    void PollConnectionStateChanges();
};
