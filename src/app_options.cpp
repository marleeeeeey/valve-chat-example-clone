#include "app_options.h"
#include <chat_client.h>
#include <chat_server.h>
#include <local_utils.h>
#include <non_blocking_console_user_input.h>
#include <stdio.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingsockets.h>
#include <steam_networking_init_RAII.h>
#include <string.h>

static void PrintUsageAndExit(int rc = 1)
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

AppOptions ReadAppOptions(int argc, const char* argv[])
{
    const uint16 DEFAULT_SERVER_PORT = 27020;

    AppOptions options;
    auto& [bServer, bClient, nPort, addrServer] = options;
    nPort = DEFAULT_SERVER_PORT;
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

    return options;
}
