cmake_minimum_required(VERSION 3.21)
project(ingame_keyboard CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the preloader-android framework subdirectory
add_subdirectory(${PRELOADER_ANDROID_ROOT} preloader)

# Build our mod shared library
add_library(ingame_keyboard SHARED
    src/InGameKeyboardMod.cpp
)

# Explicitly add the include directory so pl/Mod.hpp is found immediately
target_include_directories(ingame_keyboard PRIVATE ${PRELOADER_ANDROID_ROOT}/include)

# Link against the preloader library
target_link_libraries(ingame_keyboard PRIVATE preloader)
