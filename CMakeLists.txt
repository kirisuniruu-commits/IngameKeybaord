cmake_minimum_required(VERSION 3.21)
project(ingame_keyboard CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the preloader-android framework using the GitHub Action variable
add_subdirectory(${PRELOADER_ANDROID_ROOT} preloader)

# Build our mod shared library
add_library(ingame_keyboard SHARED
    src/InGameKeyboardMod.cpp
)

# Link our mod against the preloader headers and libraries
target_link_libraries(ingame_keyboard PRIVATE preloader)
