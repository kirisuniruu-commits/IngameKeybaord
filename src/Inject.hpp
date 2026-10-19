#pragma once
// Text injection: same JNI natives Android's soft-keyboard proxy calls.
#include <jni.h>
#include <dlfcn.h>
#include <string>

namespace inject {

using SetTextFn = void (*)(JNIEnv*, jobject, jstring, jint, jint);
using VoidFn    = void (*)(JNIEnv*, jobject);

inline JavaVM*   g_vm = nullptr;
inline jclass    g_stateClass = nullptr;
inline jmethodID g_getActivity = nullptr;
inline SetTextFn g_setText = nullptr;
inline VoidFn    g_backspace = nullptr;
inline VoidFn    g_enter = nullptr;

inline void* resolve(const char* name) {
  void* lib = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
  void* sym = lib ? dlsym(lib, name) : nullptr;
  return sym ? sym : dlsym(RTLD_DEFAULT, name);
}

inline bool init(JavaVM* vm) {
  g_vm = vm;
  g_setText = reinterpret_cast<SetTextFn>(resolve(
      "Java_com_mojang_minecraftpe_MainActivity_nativeSetTextboxText"));
  g_backspace = reinterpret_cast<VoidFn>(resolve(
      "Java_com_mojang_minecraftpe_MainActivity_nativeBackSpacePressed"));
  g_enter = reinterpret_cast<VoidFn>(resolve(
      "Java_com_mojang_minecraftpe_MainActivity_nativeReturnKeyPressed"));
  return g_vm && g_setText && g_backspace && g_enter;
}

inline JNIEnv* acquireEnv(bool& attached) {
  attached = false;
  if (!g_vm) return nullptr;
  JNIEnv* env = nullptr;
  jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
  if (rc == JNI_OK) return env;
  if (rc == JNI_EDETACHED &&
      g_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
    attached = true;
    return env;
  }
  return nullptr;
}

inline jobject currentActivity(JNIEnv* env) {
  if (!g_getActivity) {
    jclass local = env->FindClass(
        "org/levimc/launcher/core/minecraft/MinecraftActivityState");
    if (!local) { env->ExceptionClear(); return nullptr; }
    g_stateClass = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    g_getActivity = env->GetStaticMethodID(
        g_stateClass, "getCurrentActivity", "()Landroid/app/Activity;");
    if (!g_getActivity) { env->ExceptionClear(); return nullptr; }
  }
  jobject activity = env->CallStaticObjectMethod(g_stateClass, g_getActivity);
  if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
  return activity;
}

inline void callVoid(VoidFn fn) {
  if (!fn) return;
  bool attached;
  JNIEnv* env = acquireEnv(attached);
  if (!env) return;
  jobject activity = currentActivity(env);
  fn(env, activity);
  if (activity) env->DeleteLocalRef(activity);
  if (attached) g_vm->DetachCurrentThread();
}

inline void setText(const std::string& utf8) {
  if (!g_setText) return;
  bool attached;
  JNIEnv* env = acquireEnv(attached);
  if (!env) return;
  jstring s = env->NewStringUTF(utf8.c_str());
  if (s) {
    const jint end = static_cast<jint>(utf8.size());
    jobject activity = currentActivity(env);
    g_setText(env, activity, s, end, end);
    if (activity) env->DeleteLocalRef(activity);
    env->DeleteLocalRef(s);
  }
  if (attached) g_vm->DetachCurrentThread();
}

inline void backspace() { callVoid(g_backspace); }
inline void enter()     { callVoid(g_enter); }

} // namespace inject
