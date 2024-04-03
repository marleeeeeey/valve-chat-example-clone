cmake_minimum_required(VERSION 3.9)
project(example_chat_vcpkg)

# ###################################################################
# #################### Setup compiler options #######################
# ###################################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Generate compile_commands.json.

# ###################################################################
# ############### Searching some packages in system #################
# ###################################################################
find_package(GameNetworkingSockets CONFIG REQUIRED)

# ###################################################################
# ############### Adding libraries from sumodules ###################
# ###################################################################
add_subdirectory(thirdparty/my_cpp_utils)

# ###################################################################
# #################### Build library itself #########################
# ###################################################################
file(GLOB_RECURSE VALVE_CHAT_SOURCES "./src/*.cpp")
add_executable(example_chat ${VALVE_CHAT_SOURCES})

target_link_libraries(example_chat
    GameNetworkingSockets::shared
    my_cpp_utils
)

target_include_directories(example_chat
    PRIVATE
    src
)
