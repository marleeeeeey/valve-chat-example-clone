// Original location: https://github.com/ValveSoftware/GameNetworkingSockets/tree/master/examples
// In this example, I have added some comments to understand the code better (see `MY:` comments).

//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Example client/server chat application using SteamNetworkingSockets

#include <assert.h>
#include <atomic>
#include <cctype>
#include <chrono>
#include <map>
#include <stdio.h>
#include <string.h>
#include <string>
#include <thread>

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

#include "local_utils.h"
#include "non_blocking_console_user_input.h"

/////////////////////////////////////////////////////////////////////////////
//
// Common stuff
//
/////////////////////////////////////////////////////////////////////////////

class SteamNetworkingInitRAII
{
public:
    SteamNetworkingInitRAII()
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

    ~SteamNetworkingInitRAII()
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
};

/////////////////////////////////////////////////////////////////////////////
//
// ChatServer
//
/////////////////////////////////////////////////////////////////////////////

class ChatServer
{
    NonBlockingConsoleUserInput& nonBlockingConsoleUserInput;
    std::atomic<bool>& g_bQuit;
public:
    ChatServer(NonBlockingConsoleUserInput& nonBlockingConsoleUserInput, std::atomic<bool>& g_bQuit)
      : nonBlockingConsoleUserInput(nonBlockingConsoleUserInput), g_bQuit(g_bQuit)
    {}

    void Run(uint16 nPort)
    {
        // Select instance to use.  For now we'll always use the default.
        // But we could use SteamGameServerNetworkingSockets() on Steam.
        m_pInterface = SteamNetworkingSockets();

        // Start listening
        SteamNetworkingIPAddr serverLocalAddr;
        serverLocalAddr.Clear();
        serverLocalAddr.m_port = nPort;

        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(
            k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)SteamNetConnectionStatusChangedCallback);

        m_hListenSock = m_pInterface->CreateListenSocketIP(serverLocalAddr, 1, &opt);
        if (m_hListenSock == k_HSteamListenSocket_Invalid)
            LocalUtils::FatalError("Failed to listen on port %d", nPort);

        m_hPollGroup = m_pInterface->CreatePollGroup();
        if (m_hPollGroup == k_HSteamNetPollGroup_Invalid)
            LocalUtils::FatalError("Failed to listen on port %d", nPort);
        LocalUtils::Printf("Server listening on port %d\n", nPort);

        while (!g_bQuit)
        {
            PollIncomingMessages(); // MY: Recieve messages from clients until the ReceiveMessagesOnPollGroup is
                                    // empty.
            PollConnectionStateChanges(); // MY: Run all callbacks including OnSteamNetConnectionStatusChanged.
                                          // - Case 01: Detect problems with connections and close them localy by
                                          // API.
                                          // - Case 02: AcceptConnection, SetConnectionPollGroup, Create Nickname,
                                          // Send
                                          //   Welcome message.
            PollLocalUserInput(); // MY: Check if the user has entered `/quit` command and set the g_bQuit flag.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Close all the connections
        LocalUtils::Printf("Closing connections...\n");
        for (auto it : m_mapClients)
        {
            // Send them one more goodbye message.  Note that we also have the
            // connection close reason as a place to send final data.  However,
            // that's usually best left for more diagnostic/debug text not actual
            // protocol strings.
            SendStringToClient(it.first, "Server is shutting down. Goodbye.");

            // Close the connection.  We use "linger mode" to ask SteamNetworkingSockets
            // to flush this out and close gracefully.
            m_pInterface->CloseConnection(it.first, 0, "Server Shutdown", true);
        }
        m_mapClients.clear();

        m_pInterface->CloseListenSocket(m_hListenSock);
        m_hListenSock = k_HSteamListenSocket_Invalid;

        m_pInterface->DestroyPollGroup(m_hPollGroup);
        m_hPollGroup = k_HSteamNetPollGroup_Invalid;
    }
private:
    HSteamListenSocket m_hListenSock;
    HSteamNetPollGroup m_hPollGroup;
    ISteamNetworkingSockets* m_pInterface;

    struct Client_t
    {
        std::string m_sNick;
    };

    std::map<HSteamNetConnection, Client_t> m_mapClients;

    void SendStringToClient(HSteamNetConnection conn, const char* str)
    {
        m_pInterface->SendMessageToConnection(conn, str, (uint32)strlen(str), k_nSteamNetworkingSend_Reliable, nullptr);
    }

    void SendStringToAllClients(const char* str, HSteamNetConnection except = k_HSteamNetConnection_Invalid)
    {
        for (auto& c : m_mapClients)
        {
            if (c.first != except)
                SendStringToClient(c.first, str);
        }
    }

    void PollIncomingMessages()
    {
        char temp[1024];

        while (!g_bQuit)
        {
            ISteamNetworkingMessage* pIncomingMsg = nullptr;
            int numMsgs = m_pInterface->ReceiveMessagesOnPollGroup(m_hPollGroup, &pIncomingMsg, 1);
            if (numMsgs == 0)
                break;
            if (numMsgs < 0)
                LocalUtils::FatalError("Error checking for messages");
            assert(numMsgs == 1 && pIncomingMsg);
            auto itClient = m_mapClients.find(pIncomingMsg->m_conn);
            assert(itClient != m_mapClients.end());

            // '\0'-terminate it to make it easier to parse
            std::string sCmd;
            sCmd.assign((const char*)pIncomingMsg->m_pData, pIncomingMsg->m_cbSize);
            const char* cmd = sCmd.c_str();

            // We don't need this anymore.
            pIncomingMsg->Release();

            // Check for known commands.  None of this example code is secure or robust.
            // Don't write a real server like this, please.

            if (strncmp(cmd, "/nick", 5) == 0)
            {
                const char* nick = cmd + 5;
                while (isspace(*nick))
                    ++nick;

                // Let everybody else know they changed their name
                sprintf(temp, "%s shall henceforth be known as %s", itClient->second.m_sNick.c_str(), nick);
                SendStringToAllClients(temp, itClient->first);

                // Respond to client
                sprintf(temp, "Ye shall henceforth be known as %s", nick);
                SendStringToClient(itClient->first, temp);

                // Actually change their name
                SetClientNick(itClient->first, nick);
                continue;
            }

            // Assume it's just a ordinary chat message, dispatch to everybody else
            sprintf(temp, "%s: %s", itClient->second.m_sNick.c_str(), cmd);
            SendStringToAllClients(temp, itClient->first);
        }
    }

    void PollLocalUserInput()
    {
        std::string cmd;
        while (!g_bQuit && nonBlockingConsoleUserInput.LocalUserInput_GetNext(cmd))
        {
            if (strcmp(cmd.c_str(), "/quit") == 0)
            {
                g_bQuit = true;
                LocalUtils::Printf("Shutting down server");
                break;
            }

            // That's the only command we support
            LocalUtils::Printf("The server only knows one command: '/quit'");
        }
    }

    void SetClientNick(HSteamNetConnection hConn, const char* nick)
    {
        // Remember their nick
        m_mapClients[hConn].m_sNick = nick;

        // Set the connection name, too, which is useful for debugging
        m_pInterface->SetConnectionName(hConn, nick);
    }

    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo)
    {
        char temp[1024];

        // What's the state of the connection?
        switch (pInfo->m_info.m_eState)
        {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            {
                // Ignore if they were not previously connected.  (If they disconnected
                // before we accepted the connection.)
                if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
                {
                    // Locate the client.  Note that it should have been found, because this
                    // is the only codepath where we remove clients (except on shutdown),
                    // and connection change callbacks are dispatched in queue order.
                    auto itClient = m_mapClients.find(pInfo->m_hConn);
                    assert(itClient != m_mapClients.end());

                    // Select appropriate log messages
                    const char* pszDebugLogAction;
                    if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                    {
                        pszDebugLogAction = "problem detected locally";
                        sprintf(
                            temp, "Alas, %s hath fallen into shadow.  (%s)", itClient->second.m_sNick.c_str(),
                            pInfo->m_info.m_szEndDebug);
                    }
                    else
                    {
                        // Note that here we could check the reason code to see if
                        // it was a "usual" connection or an "unusual" one.
                        pszDebugLogAction = "closed by peer";
                        sprintf(temp, "%s hath departed", itClient->second.m_sNick.c_str());
                    }

                    // Spew something to our own log.  Note that because we put their nick
                    // as the connection description, it will show up, along with their
                    // transport-specific data (e.g. their IP address)
                    LocalUtils::Printf(
                        "Connection %s %s, reason %d: %s\n", pInfo->m_info.m_szConnectionDescription, pszDebugLogAction,
                        pInfo->m_info.m_eEndReason, pInfo->m_info.m_szEndDebug);

                    m_mapClients.erase(itClient);

                    // Send a message so everybody else knows what happened
                    SendStringToAllClients(temp);
                }
                else
                {
                    assert(pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
                }

                // Clean up the connection.  This is important!
                // The connection is "closed" in the network sense, but
                // it has not been destroyed.  We must close it on our end, too
                // to finish up.  The reason information do not matter in this case,
                // and we cannot linger because it's already closed on the other end,
                // so we just pass 0's.
                m_pInterface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                break;
            }

        case k_ESteamNetworkingConnectionState_Connecting:
            {
                // This must be a new connection
                assert(m_mapClients.find(pInfo->m_hConn) == m_mapClients.end());

                LocalUtils::Printf("Connection request from %s", pInfo->m_info.m_szConnectionDescription);

                // A client is attempting to connect
                // Try to accept the connection.
                if (m_pInterface->AcceptConnection(pInfo->m_hConn) != k_EResultOK)
                {
                    // This could fail.  If the remote host tried to connect, but then
                    // disconnected, the connection may already be half closed.  Just
                    // destroy whatever we have on our side.
                    m_pInterface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                    LocalUtils::Printf("Can't accept connection.  (It was already closed?)");
                    break;
                }

                // Assign the poll group
                if (!m_pInterface->SetConnectionPollGroup(pInfo->m_hConn, m_hPollGroup))
                {
                    m_pInterface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                    LocalUtils::Printf("Failed to set poll group?");
                    break;
                }

                // Generate a random nick.  A random temporary nick
                // is really dumb and not how you would write a real chat server.
                // You would want them to have some sort of signon message,
                // and you would keep their client in a state of limbo (connected,
                // but not logged on) until them.  I'm trying to keep this example
                // code really simple.
                char nick[64];
                sprintf(nick, "BraveWarrior%d", 10000 + (rand() % 100000));

                // Send them a welcome message
                sprintf(
                    temp,
                    "Welcome, stranger.  Thou art known to us for now as '%s'; upon thine command '/nick' we shall know thee otherwise.",
                    nick);
                SendStringToClient(pInfo->m_hConn, temp);

                // Also send them a list of everybody who is already connected
                if (m_mapClients.empty())
                {
                    SendStringToClient(pInfo->m_hConn, "Thou art utterly alone.");
                }
                else
                {
                    sprintf(temp, "%d companions greet you:", (int)m_mapClients.size());
                    for (auto& c : m_mapClients)
                        SendStringToClient(pInfo->m_hConn, c.second.m_sNick.c_str());
                }

                // Let everybody else know who they are for now
                sprintf(temp, "Hark!  A stranger hath joined this merry host.  For now we shall call them '%s'", nick);
                SendStringToAllClients(temp, pInfo->m_hConn);

                // Add them to the client list, using std::map wacky syntax
                m_mapClients[pInfo->m_hConn];
                SetClientNick(pInfo->m_hConn, nick);
                break;
            }

        case k_ESteamNetworkingConnectionState_Connected:
            // We will get a callback immediately after accepting the connection.
            // Since we are the server, we can ignore this, it's not news to us.
            break;

        default:
            // Silences -Wswitch
            break;
        }
    }

    static ChatServer* s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo)
    {
        s_pCallbackInstance->OnSteamNetConnectionStatusChanged(pInfo);
    }

    void PollConnectionStateChanges()
    {
        s_pCallbackInstance = this;
        m_pInterface->RunCallbacks();
    }
};

ChatServer* ChatServer::s_pCallbackInstance = nullptr;

/////////////////////////////////////////////////////////////////////////////
//
// ChatClient
//
/////////////////////////////////////////////////////////////////////////////

class ChatClient
{
    NonBlockingConsoleUserInput& nonBlockingConsoleUserInput;
    std::atomic<bool>& g_bQuit;
public:
    ChatClient(NonBlockingConsoleUserInput& nonBlockingConsoleUserInput, std::atomic<bool>& g_bQuit)
      : nonBlockingConsoleUserInput(nonBlockingConsoleUserInput), g_bQuit(g_bQuit)
    {}

    void Run(const SteamNetworkingIPAddr& serverAddr)
    {
        // Select instance to use.  For now we'll always use the default.
        m_pInterface = SteamNetworkingSockets();

        // Start connecting
        char szAddr[SteamNetworkingIPAddr::k_cchMaxString];
        serverAddr.ToString(szAddr, sizeof(szAddr), true);
        LocalUtils::Printf("Connecting to chat server at %s", szAddr);
        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(
            k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)SteamNetConnectionStatusChangedCallback);
        m_hConnection = m_pInterface->ConnectByIPAddress(serverAddr, 1, &opt);
        if (m_hConnection == k_HSteamNetConnection_Invalid)
            LocalUtils::FatalError("Failed to create connection");

        while (!g_bQuit)
        {
            PollIncomingMessages();
            PollConnectionStateChanges();
            PollLocalUserInput();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
private:
    HSteamNetConnection m_hConnection;
    ISteamNetworkingSockets* m_pInterface;

    void PollIncomingMessages()
    {
        while (!g_bQuit)
        {
            ISteamNetworkingMessage* pIncomingMsg = nullptr;
            int numMsgs = m_pInterface->ReceiveMessagesOnConnection(m_hConnection, &pIncomingMsg, 1);
            if (numMsgs == 0)
                break;
            if (numMsgs < 0)
                LocalUtils::FatalError("Error checking for messages");

            // Just echo anything we get from the server
            fwrite(pIncomingMsg->m_pData, 1, pIncomingMsg->m_cbSize, stdout);
            fputc('\n', stdout);

            // We don't need this anymore.
            pIncomingMsg->Release();
        }
    }

    void PollLocalUserInput()
    {
        std::string cmd;
        while (!g_bQuit && nonBlockingConsoleUserInput.LocalUserInput_GetNext(cmd))
        {
            // Check for known commands
            if (strcmp(cmd.c_str(), "/quit") == 0)
            {
                g_bQuit = true;
                LocalUtils::Printf("Disconnecting from chat server");

                // Close the connection gracefully.
                // We use linger mode to ask for any remaining reliable data
                // to be flushed out.  But remember this is an application
                // protocol on UDP.  See ShutdownSteamDatagramConnectionSockets
                m_pInterface->CloseConnection(m_hConnection, 0, "Goodbye", true);
                break;
            }

            // Anything else, just send it to the server and let them parse it
            m_pInterface->SendMessageToConnection(
                m_hConnection, cmd.c_str(), (uint32)cmd.length(), k_nSteamNetworkingSend_Reliable, nullptr);
        }
    }

    void OnSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo)
    {
        assert(pInfo->m_hConn == m_hConnection || m_hConnection == k_HSteamNetConnection_Invalid);

        // What's the state of the connection?
        switch (pInfo->m_info.m_eState)
        {
        case k_ESteamNetworkingConnectionState_None:
            // NOTE: We will get callbacks here when we destroy connections.  You can ignore these.
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            {
                g_bQuit = true;

                // Print an appropriate message
                if (pInfo->m_eOldState == k_ESteamNetworkingConnectionState_Connecting)
                {
                    // Note: we could distinguish between a timeout, a rejected connection,
                    // or some other transport problem.
                    LocalUtils::Printf(
                        "We sought the remote host, yet our efforts were met with defeat.  (%s)",
                        pInfo->m_info.m_szEndDebug);
                }
                else if (pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                {
                    LocalUtils::Printf(
                        "Alas, troubles beset us; we have lost contact with the host.  (%s)",
                        pInfo->m_info.m_szEndDebug);
                }
                else
                {
                    // NOTE: We could check the reason code for a normal disconnection
                    LocalUtils::Printf("The host hath bidden us farewell.  (%s)", pInfo->m_info.m_szEndDebug);
                }

                // Clean up the connection.  This is important!
                // The connection is "closed" in the network sense, but
                // it has not been destroyed.  We must close it on our end, too
                // to finish up.  The reason information do not matter in this case,
                // and we cannot linger because it's already closed on the other end,
                // so we just pass 0's.
                m_pInterface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                m_hConnection = k_HSteamNetConnection_Invalid;
                break;
            }

        case k_ESteamNetworkingConnectionState_Connecting:
            // We will get this callback when we start connecting.
            // We can ignore this.
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            LocalUtils::Printf("Connected to server OK");
            break;

        default:
            // Silences -Wswitch
            break;
        }
    }

    static ChatClient* s_pCallbackInstance;
    static void SteamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo)
    {
        s_pCallbackInstance->OnSteamNetConnectionStatusChanged(pInfo);
    }

    void PollConnectionStateChanges()
    {
        s_pCallbackInstance = this;
        m_pInterface->RunCallbacks();
    }
};

ChatClient* ChatClient::s_pCallbackInstance = nullptr;

const uint16 DEFAULT_SERVER_PORT = 27020;

void PrintUsageAndExit(int rc = 1)
{
    fflush(stderr);
    printf(
        R"usage(Usage:
    example_chat client SERVER_ADDR
    example_chat server [--port PORT]
)usage");
    fflush(stdout);
    exit(rc);
}

int main(int argc, const char* argv[])
{
    bool bServer = false;
    bool bClient = false;
    int nPort = DEFAULT_SERVER_PORT;
    SteamNetworkingIPAddr addrServer;
    addrServer.Clear();

    for (int i = 1; i < argc; ++i)
    {
        if (!bClient && !bServer)
        {
            if (!strcmp(argv[i], "client"))
            {
                bClient = true;
                continue;
            }
            if (!strcmp(argv[i], "server"))
            {
                bServer = true;
                continue;
            }
        }
        if (!strcmp(argv[i], "--port"))
        {
            ++i;
            if (i >= argc)
                PrintUsageAndExit();
            nPort = atoi(argv[i]);
            if (nPort <= 0 || nPort > 65535)
                LocalUtils::FatalError("Invalid port %d", nPort);
            continue;
        }

        // Anything else, must be server address to connect to
        if (bClient && addrServer.IsIPv6AllZeros())
        {
            if (!addrServer.ParseString(argv[i]))
                LocalUtils::FatalError("Invalid server address '%s'", argv[i]);
            if (addrServer.m_port == 0)
                addrServer.m_port = DEFAULT_SERVER_PORT;
            continue;
        }

        PrintUsageAndExit();
    }

    if (bClient == bServer || (bClient && addrServer.IsIPv6AllZeros()))
        PrintUsageAndExit();
    {
        // Create client and server sockets
        SteamNetworkingInitRAII steamNetworkingInitRAII;

        // MY: Start the thread to read the user input.
        std::atomic<bool> g_bQuit = {};
        NonBlockingConsoleUserInput nonBlockingConsoleUserInput(g_bQuit);

        if (bClient)
        {
            ChatClient client(nonBlockingConsoleUserInput, g_bQuit);
            client.Run(addrServer);
        }
        else
        {
            ChatServer server(nonBlockingConsoleUserInput, g_bQuit);
            server.Run((uint16)nPort);
        }
    }

    // Ug, why is there no simple solution for portable, non-blocking console user input?
    // Just nuke the process
    // LocalUserInput_Kill();
    LocalUtils::NukeProcess(0);
}
