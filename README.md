# In-Game Keyboard (LeviLauncher / LeviLaunchroid native mod)

Author: Chriel · Target: Minecraft Bedrock 1.26.52, Android arm64-v8a

Floating on-screen QWERTY keyboard drawn by the launcher's HUD overlay
(no Android views, no extra threads, redraws only when a key is pressed or
the layout changes). Typed text goes into Minecraft's own text box through the
same JNI natives the Android soft keyboard uses, so chat, commands, command
blocks, signs and search bars all work.

## Build (cloud, no setup needed)
Push this folder to a GitHub repository; the workflow in .github/workflows/build.yml
builds InGameKeyboard.levipack and attaches it to the run as an artifact.

## Build (on your own computer)
Requirements: Android NDK r27+ (r28c works), CMake 3.22+, Ninja, Python 3,
and a checkout of https://github.com/LiteLDev/preloader-android
(the LeviLaunchroid zip ships that folder empty).

    git clone https://github.com/LiteLDev/preloader-android
    ANDROID_NDK_HOME=/path/to/ndk ./build.sh /path/to/preloader-android

Output: dist/InGameKeyboard.levipack

## Install
1. Copy InGameKeyboard.levipack to the phone.
2. LeviLauncher -> mod manager -> import it -> enable it.
3. Launch Minecraft 1.26.52 through LeviLauncher.
4. Open the mod menu, switch on "In-Game Keyboard" (or tap the KB bubble).
5. Tune Position X/Y and Key Size in the module settings.

## What was checked against your uploaded sources
- All `pl::` calls (Mod, ModMenu, Input) compile-checked against the real
  preloader-android headers (Logger and jni.h stubbed, no NDK available).
- Touch events and HUD size both come from the game view in pixels, so the
  keyboard is placed with `getHudSurfaceSize()` (sliders are screen percentages).
- The game's Activity is fetched from the launcher's
  `MinecraftActivityState.getCurrentActivity()` and passed to the JNI natives.

## Known unverified points (test these)
1. **Overlay visibility (biggest risk).** The launcher shows HUD draw commands
   only while the HUD screen is open and no menu is showing
   (`InbuiltOverlayManager.tick`). Chat, sign and search screens may count as
   "menu showing", which would hide the keyboard exactly where it is needed.
   Test: does the keyboard appear in gameplay? In chat? On a sign?
2. The JNI natives are called by name from libminecraftpe.so. If the log says
   "Text-input natives not found", the names differ in this game build.
3. The keyboard only knows text it typed; pre-existing text in a box is
   replaced on the first keypress.
4. Key labels are centered by estimate.
