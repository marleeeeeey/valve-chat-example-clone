// Original location: https://github.com/ValveSoftware/GameNetworkingSockets/tree/master/examples
// In this example, I have added some comments to understand the code better (see `MY:` comments).
//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Example client/server chat application using SteamNetworkingSockets

#include <app_options.h>
#include <atomic>
#include <chat_client.h>
#include <chat_server.h>
#include <local_utils.h>
#include <my_cpp_utils/logger.h>
#include <non_blocking_console_user_input.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <steam_networking_init_RAII.h>

int main(int argc, const char* argv[])
{
    // Initialize the logger
    utils::Logger::Init("chat.log", spdlog::level::trace);
    MY_LOG(info, "Starting chat application");

    {
        AppOptions options = ReadAppOptions(argc, argv);

        // Initialize SteamNetworkingSockets
        SteamNetworkingInitRAII steamNetworkingInitRAII;

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

    LocalUtils::NukeProcess(0);
}
