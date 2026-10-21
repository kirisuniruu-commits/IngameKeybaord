#pragma once
// Hold-to-repeat for Backspace. A short-lived sleeping thread exists only while
// the key is held; key events go through Instrumentation, which delivers them
// to the UI thread safely.
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "AndroidUi.hpp"

namespace extras {

inline std::mutex g_repMutex;
inline std::condition_variable g_repCv;
inline unsigned g_repGen = 0;

inline void repeatLoop(int keyCode, int delayMs, int intervalMs, unsigned gen) {
  JNIEnv* e = nullptr;
  bool attached = false;
  if (ui::g_vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) != JNI_OK) {
    if (ui::g_vm->AttachCurrentThread(&e, nullptr) != JNI_OK) return;
    attached = true;
  }
  jclass ic = e->FindClass("android/app/Instrumentation");
  jmethodID ctor = ic ? ui::cmid(e, ic, "<init>", "()V") : nullptr;
  jmethodID send = ic ? ui::cmid(e, ic, "sendKeyDownUpSync", "(I)V") : nullptr;
  jobject instr = (ctor && send) ? e->NewObject(ic, ctor) : nullptr;
  if (instr) {
    std::unique_lock<std::mutex> lk(g_repMutex);
    auto stopped = [&] { return g_repGen != gen; };
    bool stop = g_repCv.wait_for(lk, std::chrono::milliseconds(delayMs), stopped);
    while (!stop) {
      lk.unlock();
      e->CallVoidMethod(instr, send, static_cast<jint>(keyCode));
      const bool failed = e->ExceptionCheck();
      ui::clearEx(e);
      lk.lock();
      if (failed) break;
      stop = g_repCv.wait_for(lk, std::chrono::milliseconds(intervalMs), stopped);
    }
  }
  ui::clearEx(e);
  if (attached) ui::g_vm->DetachCurrentThread();
}

inline void stopRepeat() {
  { std::lock_guard<std::mutex> lk(g_repMutex); ++g_repGen; }
  g_repCv.notify_all();
}

inline void startRepeat(int keyCode, int delayMs, int intervalMs) {
  unsigned gen;
  { std::lock_guard<std::mutex> lk(g_repMutex); gen = ++g_repGen; }
  g_repCv.notify_all();
  std::thread(repeatLoop, keyCode, delayMs, intervalMs, gen).detach();
}

} // namespace extras
