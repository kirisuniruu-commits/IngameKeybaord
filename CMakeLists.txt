cmake_minimum_required(VERSION 3.22)
project(in_game_keyboard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Path to a checkout of https://github.com/LiteLDev/preloader-android
# (the LeviLaunchroid zip ships this folder empty).
set(PRELOADER_ANDROID_ROOT "" CACHE PATH "Path to LiteLDev/preloader-android")
if(NOT EXISTS "${PRELOADER_ANDROID_ROOT}/include/pl/Mod.hpp")
    message(FATAL_ERROR
        "Set -DPRELOADER_ANDROID_ROOT=<preloader-android checkout>. "
        "Missing: ${PRELOADER_ANDROID_ROOT}/include/pl/Mod.hpp")
endif()

include(FetchContent)
# The preloader headers pull these in (same set the official example uses).
if(NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
    list(APPEND IK_DEPS nlohmann_json)
endif()
if(NOT TARGET Boost::pfr)
    FetchContent_Declare(boost_pfr
        GIT_REPOSITORY https://github.com/boostorg/pfr.git GIT_TAG boost-1.88.0)
    list(APPEND IK_DEPS boost_pfr)
endif()
if(NOT TARGET magic_enum::magic_enum)
    FetchContent_Declare(magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum.git GIT_TAG v0.9.7)
    list(APPEND IK_DEPS magic_enum)
endif()
if(NOT TARGET fmt::fmt)
    FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 11.2.0)
    set(FMT_DOC OFF CACHE BOOL "" FORCE)
    set(FMT_TEST OFF CACHE BOOL "" FORCE)
    set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
    list(APPEND IK_DEPS fmt)
endif()
if(IK_DEPS)
    FetchContent_MakeAvailable(${IK_DEPS})
endif()

if(ANDROID)
    if(NOT TARGET preloader)
        add_subdirectory("${PRELOADER_ANDROID_ROOT}"
                         "${CMAKE_BINARY_DIR}/_preloader_android")
    endif()

    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/${ANDROID_ABI}")
    find_library(IK_LOG_LIB log)

    add_library(ingame_keyboard SHARED
        src/InGameKeyboardMod.cpp
        src/Keyboard.cpp
        src/TextInject.cpp)
    set_target_properties(ingame_keyboard PROPERTIES OUTPUT_NAME "ingame_keyboard")
    target_include_directories(ingame_keyboard PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src"
        "${PRELOADER_ANDROID_ROOT}/include")
    target_link_libraries(ingame_keyboard PRIVATE
        preloader nlohmann_json::nlohmann_json Boost::pfr
        magic_enum::magic_enum fmt::fmt)
    if(IK_LOG_LIB)
        target_link_libraries(ingame_keyboard PRIVATE "${IK_LOG_LIB}")
    endif()
    # Small + fast on low-end devices.
    target_compile_options(ingame_keyboard PRIVATE
        -Os -fvisibility=hidden -fvisibility-inlines-hidden
        -ffunction-sections -fdata-sections -Wall -Wextra)
    target_link_options(ingame_keyboard PRIVATE -Wl,--gc-sections -Wl,-z,max-page-size=16384)
endif()
