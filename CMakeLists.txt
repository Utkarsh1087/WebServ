cmake_minimum_required(VERSION 3.12)
project(WebServ LANGUAGES CXX)

# Ensure C++17 is used
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(src)

# Add source files
add_executable(webserv
    src/main.cpp
    src/Server.h
    src/Server.cpp
    src/HttpParser.h
    src/HttpParser.cpp
    src/Router.h
    src/Router.cpp
    src/Config.h
    src/Config.cpp
)

# Link standard libraries
if(WIN32)
    # On Windows, link the Winsock library (ws2_32.lib)
    target_link_libraries(webserv PRIVATE ws2_32)
endif()
