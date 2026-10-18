cmake_minimum_required(VERSION 3.21)
project(ingame_keyboard CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${PRELOADER_ANDROID_ROOT} preloader)

add_library(ingame_keyboard SHARED
    src/InGameKeyboardMod.cpp
)

target_include_directories(ingame_keyboard PRIVATE ${PRELOADER_ANDROID_ROOT}/include)

target_link_libraries(ingame_keyboard PRIVATE preloader)
