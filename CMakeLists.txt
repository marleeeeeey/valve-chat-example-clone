#
# Example porject that uses GameNetworkingSockets, obtaining
# dependencies, etc entirely through vcpkg.
#
cmake_minimum_required(VERSION 3.9)
project(example_chat_vcpkg)

# Ask vcpkg to fetch gamenetworkingsockets and all
# its dependencies and build it
find_package(GameNetworkingSockets CONFIG REQUIRED)

# Our program only has one cpp file
add_executable(example_chat
    src/app_options.cpp
    src/chat_client.cpp
    src/chat_server.cpp
    src/local_utils.cpp
    src/main.cpp
    src/non_blocking_console_user_input.cpp
    src/steam_networking_init_RAII.cpp
)

# This example links GameNetworkingSockets as a shared lib.
target_link_libraries(example_chat GameNetworkingSockets::shared)

target_include_directories(example_chat PRIVATE
    src
)
