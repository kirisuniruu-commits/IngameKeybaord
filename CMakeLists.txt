cmake_minimum_required(VERSION 3.21)
project(ingame_keyboard CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add our consolidated source file
add_library(ingame_keyboard SHARED
    src/InGameKeyboardMod.cpp
)

# Link against the preloader environment
target_link_libraries(ingame_keyboard PRIVATE preloader)
