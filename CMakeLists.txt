cmake_minimum_required(VERSION 3.21)
project(ingame_keyboard CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${PRELOADER_ANDROID_ROOT} preloader)

# Automatically fetch ImGui headers so #include <imgui.h> never fails
include(FetchContent)
FetchContent_Declare(
    imgui_headers
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.89.9.zip
)
FetchContent_MakeAvailable(imgui_headers)

add_library(ingame_keyboard SHARED
    src/InGameKeyboardMod.cpp
)

target_include_directories(ingame_keyboard PRIVATE 
    ${PRELOADER_ANDROID_ROOT}/include
    ${imgui_headers_SOURCE_DIR}
)

target_link_libraries(ingame_keyboard PRIVATE preloader)
