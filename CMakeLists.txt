cmake_minimum_required(VERSION 3.21)
project(ingame_keyboard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# preloader-android
# ---------------------------------------------------------------------------
if(NOT DEFINED PRELOADER_ANDROID_ROOT OR NOT EXISTS "${PRELOADER_ANDROID_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "Set -DPRELOADER_ANDROID_ROOT=<path to a LiteLDev/preloader-android checkout>")
endif()
add_subdirectory(${PRELOADER_ANDROID_ROOT} preloader)

# ---------------------------------------------------------------------------
# ImGui HEADERS ONLY.
# The mod must use the ImGui that is already inside the host process (same
# context, same allocator), so we deliberately do NOT compile imgui*.cpp.
# IMGUI_VERSION must match the ImGui version the host was built with, otherwise
# struct layouts (ImGuiIO, ImGuiStyle) differ and the mod will misbehave.
# ---------------------------------------------------------------------------
set(IMGUI_VERSION "1.89.9" CACHE STRING "ImGui version used by the host (headers only)")
include(FetchContent)
FetchContent_Declare(
    imgui_headers
    URL https://github.com/ocornut/imgui/archive/refs/tags/v${IMGUI_VERSION}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(imgui_headers)   # imgui has no CMakeLists -> download only

add_library(ingame_keyboard SHARED src/InGameKeyboardMod.cpp)

target_include_directories(ingame_keyboard PRIVATE
    ${PRELOADER_ANDROID_ROOT}/include
    ${imgui_headers_SOURCE_DIR}
)

target_compile_options(ingame_keyboard PRIVATE -fvisibility=hidden -Wall -Wextra)

# ImGui symbols are resolved from the host at dlopen() time. The NDK toolchain
# passes --no-undefined for shared libs, which would fail the link, so relax it.
target_link_options(ingame_keyboard PRIVATE -Wl,-z,undefs)

target_link_libraries(ingame_keyboard PRIVATE preloader)

# build.sh / scripts expect: <build>/out/<abi>/libingame_keyboard.so
if(ANDROID_ABI)
    set(_abi "${ANDROID_ABI}")
else()
    set(_abi "host")
endif()
set_target_properties(ingame_keyboard PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out/${_abi}"
)
