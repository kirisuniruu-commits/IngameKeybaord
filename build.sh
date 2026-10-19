#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PRELOADER="$(cd "${1:-$HERE/preloader-android}" && pwd)"
NDK="${ANDROID_NDK_HOME:?ANDROID_NDK_HOME is not set}"
BUILD="$HERE/build/android-arm64-v8a"

cmake -S "$HERE" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
  -DCMAKE_BUILD_TYPE=Release \
  -DPRELOADER_ANDROID_ROOT="$PRELOADER"
cmake --build "$BUILD" --target ingame_keyboard

python3 "$HERE/scripts/package_levipack.py" \
  --library "$BUILD/out/arm64-v8a/libingame_keyboard.so" \
  --output "$HERE/dist/InGameKeyboard.levipack"
