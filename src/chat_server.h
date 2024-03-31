#pragma once
#include <map>
#include <non_blocking_console_user_input.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/steamnetworkingtypes.h>

class ChatServer
{
    NonBlockingConsoleUserInput& nonBlockingConsoleUserInput;
    std::atomic<bool>& quitFlag;
    HSteamListenSocket hListenSock;
    HSteamNetPollGroup hPollGroup;
    ISteamNetworkingSockets* pInterface;
    struct Client_t
    {
        std::string m_sNick;
    };
    std::map<HSteamNetConnection, Client_t> mapClients;
public:
    ChatServer(NonBlockingConsoleUserInput& nonBlockingConsoleUserInput, std::atomic<bool>& quitFlag);
    void Run(uint16 nPort);
private:
    void SendStringToClient(HSteamNetConnection conn, const char* str);
    void SendStringToAllClients(const char* str, HSteamNetConnection except = k_HSteamNetConnection_Invalid);
    void PollIncomingMessages();
    void PollLocalUserInput();
    void SetClientNick(HSteamNetConnection hConn, const char* nick);
private: // OnSteamNetConnectionStatusChanged stuff.
    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);
    static ChatServer* s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo);
    void PollConnectionStateChanges();
};
