#pragma once
#include <jni.h>
#include <string>

// Feeds text into Minecraft's own text box by calling the same JNI natives
// that the Android soft-keyboard proxy (MainActivity.textInputWidget) calls.
namespace ik::inject {

bool init(JavaVM* vm);                  // resolves symbols; false if any missing
bool available();
void setText(const std::string& utf8);  // nativeSetTextboxText(text, end, end)
void backspace();                       // nativeBackSpacePressed()
void enter();                           // nativeReturnKeyPressed()

} // namespace ik::inject
