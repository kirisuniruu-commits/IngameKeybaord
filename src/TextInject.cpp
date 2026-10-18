#include "TextInject.hpp"

#include <dlfcn.h>

namespace ik::inject {
namespace {

using SetTextFn = void (*)(JNIEnv*, jobject, jstring, jint, jint);
using VoidFn    = void (*)(JNIEnv*, jobject);

JavaVM*   g_vm = nullptr;
jclass    g_stateClass = nullptr;      // MinecraftActivityState (global ref)
jmethodID g_getActivity = nullptr;     // static Activity getCurrentActivity()
SetTextFn g_setText = nullptr;
VoidFn    g_backspace = nullptr;
VoidFn    g_enter = nullptr;

void* resolve(const char* name) {
  void* lib = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
  void* sym = lib ? dlsym(lib, name) : nullptr;
  return sym ? sym : dlsym(RTLD_DEFAULT, name);
}

// Returns an env for the calling thread; `attached` is set if we attached it.
JNIEnv* acquireEnv(bool& attached) {
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

// Looks up the running game Activity through the launcher's public static
// getter. Returns a local ref (or nullptr; the game then gets a null `this`).
jobject currentActivity(JNIEnv* env) {
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

void callVoid(VoidFn fn) {
  if (!fn) return;
  bool attached;
  JNIEnv* env = acquireEnv(attached);
  if (!env) return;
  jobject activity = currentActivity(env);
  fn(env, activity);
  if (activity) env->DeleteLocalRef(activity);
  if (attached) g_vm->DetachCurrentThread();
}

} // namespace

bool init(JavaVM* vm) {
  g_vm = vm;
  g_setText = reinterpret_cast<SetTextFn>(resolve(
      "Java_com_mojang_minecraftpe_MainActivity_nativeSetTextboxText"));
  g_backspace = reinterpret_cast<VoidFn>(resolve(
      "Java_com_mojang_minecraftpe_MainActivity_nativeBackSpacePressed"));
  g_enter = reinterpret_cast<VoidFn>(resolve(
      "Java_com_mojang_minecraftpe_MainActivity_nativeReturnKeyPressed"));
  return available();
}

bool available() { return g_vm && g_setText && g_backspace && g_enter; }

void setText(const std::string& utf8) {
  if (!g_setText) return;
  bool attached;
  JNIEnv* env = acquireEnv(attached);
  if (!env) return;
  jstring s = env->NewStringUTF(utf8.c_str());
  if (s) {
    const jint end = static_cast<jint>(utf8.size());  // ASCII-only keys
    jobject activity = currentActivity(env);
    g_setText(env, activity, s, end, end);
    if (activity) env->DeleteLocalRef(activity);
    env->DeleteLocalRef(s);
  }
  if (attached) g_vm->DetachCurrentThread();
}

void backspace() { callVoid(g_backspace); }
void enter()     { callVoid(g_enter); }

} // namespace ik::inject
