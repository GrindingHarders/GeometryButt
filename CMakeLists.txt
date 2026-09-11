cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(buttplug-mod VERSION 1.0.0)

# ---------------------------------------------------------
# Geode
# ---------------------------------------------------------

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR
        "Unable to find Geode SDK! Please define GEODE_SDK "
        "environment variable to point to Geode"
    )
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory(
    $ENV{GEODE_SDK}
    ${CMAKE_CURRENT_BINARY_DIR}/geode
)

# ---------------------------------------------------------
# Dependencies
# ---------------------------------------------------------

include(FetchContent)

# nlohmann/json
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(nlohmann_json)

# IXWebSocket
set(BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)

set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v12.0.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(ixwebsocket)

# ---------------------------------------------------------
# Sources
# ---------------------------------------------------------

file(
    GLOB_RECURSE
    SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

# ---------------------------------------------------------
# Mod
# ---------------------------------------------------------

add_library(
    ${PROJECT_NAME}
    SHARED
    ${SOURCES}
)

target_link_libraries(
    ${PROJECT_NAME}
    nlohmann_json::nlohmann_json
    ixwebsocket::ixwebsocket
)

# ---------------------------------------------------------
# Geode setup
# ---------------------------------------------------------

setup_geode_mod(${PROJECT_NAME})
