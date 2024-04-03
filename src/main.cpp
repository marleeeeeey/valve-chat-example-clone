// Original location: https://github.com/ValveSoftware/GameNetworkingSockets/tree/master/examples
// In this example, I have added some comments to understand the code better (see `MY:` comments).
//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Example client/server chat application using SteamNetworkingSockets

#include <app_options.h>
#include <atomic>
#include <chat_client.h>
#include <chat_server.h>
#include <my_cpp_utils/logger.h>
#include <non_blocking_console_user_input.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <steam_networking_init_RAII.h>

int main(int argc, const char* argv[])
{
    {
        AppOptions options = ReadAppOptions(argc, argv);

        // Initialize the logger

        std::string logName = options.bClient ? "chat_client.log" : "chat_server.log";
        utils::Logger::Init(logName, spdlog::level::trace);
        MY_LOG(info, "Starting chat application");

        // Initialize the SteamNetworkingSockets library.
        SteamNetworkingInitRAII::Options steamNetworkingOptions;
        steamNetworkingOptions.debugSeverity = k_ESteamNetworkingSocketsDebugOutputType_Msg;
        MY_LOG_FMT(info, "[SteamNetworking] debugSeverity: {}", steamNetworkingOptions.debugSeverity);
        SteamNetworkingInitRAII steamNetworkingInitRAII(steamNetworkingOptions);
        SteamNetworkingInitRAII::SetDebugCallback(
            []([[maybe_unused]] ESteamNetworkingSocketsDebugOutputType eType, const char* pszMsg)
            { MY_LOG_FMT(info, "[DebugOutput] {}", pszMsg); });

        // Start the thread to read the user input.
        std::atomic<bool> appQuitFlag = {};
        NonBlockingConsoleUserInput nonBlockingConsoleUserInput(appQuitFlag);

        if (options.bClient)
        {
            ChatClient client(nonBlockingConsoleUserInput, appQuitFlag);
            client.Run(options.addrServer);
        }
        else
        {
            ChatServer server(nonBlockingConsoleUserInput, appQuitFlag);
            server.Run((uint16)options.nPort);
        }
    }
}
