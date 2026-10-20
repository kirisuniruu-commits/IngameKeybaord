#pragma once
// Clipboard (copy / cut / paste for the game's text box) and hold-to-repeat
// for Backspace. UI-thread calls except where noted.
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "AndroidUi.hpp"

namespace extras {

inline std::string toUtf8(JNIEnv* e, jstring js) {
  if (!js) return {};
  const char* c = e->GetStringUTFChars(js, nullptr);
  std::string s = c ? c : "";
  if (c) e->ReleaseStringUTFChars(js, c);
  return s;
}

// The game's real text box (an EditText), or nullptr. Returns a local ref.
inline jobject textBox(JNIEnv* e, jobject act) {
  jclass ac = e->GetObjectClass(act);
  jfieldID f = e->GetFieldID(ac, "textInputWidget",
                             "Lcom/mojang/minecraftpe/TextInputProxyEditTextbox;");
  e->DeleteLocalRef(ac);
  if (!f) { ui::clearEx(e); return nullptr; }
  return e->GetObjectField(act, f);
}

inline bool textBoxActive(JNIEnv* e, jobject tb) {   // VISIBLE = a text field is in use
  jmethodID gv = ui::mid(e, tb, "getVisibility", "()I");
  return gv && e->CallIntMethod(tb, gv) == 0;
}

inline std::string textBoxText(JNIEnv* e, jobject tb) {
  jmethodID gt = ui::mid(e, tb, "getText", "()Landroid/text/Editable;");
  jobject ed = gt ? e->CallObjectMethod(tb, gt) : nullptr;
  if (!ed) return {};
  jmethodID ts = ui::mid(e, ed, "toString", "()Ljava/lang/String;");
  return ts ? toUtf8(e, static_cast<jstring>(e->CallObjectMethod(ed, ts))) : std::string();
}

inline jobject clipboardManager(JNIEnv* e, jobject act) {
  jmethodID gs = ui::mid(e, act, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
  if (!gs) return nullptr;
  jstring name = e->NewStringUTF("clipboard");
  return e->CallObjectMethod(act, gs, name);
}

inline std::string clipboardText(JNIEnv* e, jobject act) {
  jobject cm = clipboardManager(e, act);
  if (!cm) return {};
  jmethodID gp = ui::mid(e, cm, "getPrimaryClip", "()Landroid/content/ClipData;");
  jobject clip = gp ? e->CallObjectMethod(cm, gp) : nullptr;
  ui::clearEx(e);
  if (!clip) return {};
  jmethodID cnt = ui::mid(e, clip, "getItemCount", "()I");
  if (!cnt || e->CallIntMethod(clip, cnt) < 1) return {};
  jmethodID at = ui::mid(e, clip, "getItemAt", "(I)Landroid/content/ClipData$Item;");
  jobject item = at ? e->CallObjectMethod(clip, at, static_cast<jint>(0)) : nullptr;
  if (!item) return {};
  jmethodID co = ui::mid(e, item, "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;");
  jobject cs = co ? e->CallObjectMethod(item, co, act) : nullptr;
  if (!cs) return {};
  jmethodID ts = ui::mid(e, cs, "toString", "()Ljava/lang/String;");
  ui::clearEx(e);
  return ts ? toUtf8(e, static_cast<jstring>(e->CallObjectMethod(cs, ts))) : std::string();
}

inline void setClipboardText(JNIEnv* e, jobject act, const std::string& text) {
  jobject cm = clipboardManager(e, act);
  jclass cd = e->FindClass("android/content/ClipData");
  if (!cm || !cd) { ui::clearEx(e); return; }
  jmethodID np = ui::smid(e, cd, "newPlainText",
      "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
  jmethodID sp = ui::mid(e, cm, "setPrimaryClip", "(Landroid/content/ClipData;)V");
  if (!np || !sp) return;
  jstring label = e->NewStringUTF("Minecraft");
  jstring body = e->NewStringUTF(text.c_str());
  jobject clip = e->CallStaticObjectMethod(cd, np, label, body);
  if (clip) e->CallVoidMethod(cm, sp, clip);
  ui::clearEx(e);
}

enum class ClipOp { Copy, Cut, Paste };

// Acts on the game's text box. Does nothing unless a text field is in use.
inline void clipboard(ClipOp op) {
  JNIEnv* e = ui::env(); if (!e) return;
  e->PushLocalFrame(48);
  jobject act = ui::currentActivity(e);
  jobject tb = act ? textBox(e, act) : nullptr;
  if (act && tb && textBoxActive(e, tb)) {
    if (op == ClipOp::Paste) {
      const std::string clip = clipboardText(e, act);
      jmethodID ap = clip.empty() ? nullptr
          : ui::mid(e, tb, "append", "(Ljava/lang/CharSequence;)V");
      if (ap) e->CallVoidMethod(tb, ap, e->NewStringUTF(clip.c_str()));
    } else {
      const std::string text = textBoxText(e, tb);
      if (!text.empty()) {
        setClipboardText(e, act, text);
        if (op == ClipOp::Cut) {
          jmethodID st = ui::mid(e, tb, "setText", "(Ljava/lang/CharSequence;)V");
          if (st) e->CallVoidMethod(tb, st, e->NewStringUTF(""));
        }
      }
    }
  }
  ui::clearEx(e);
  e->PopLocalFrame(nullptr);
}

// ---- hold-to-repeat -------------------------------------------------------
// A short-lived sleeping thread exists only while the key is held. Key events
// go through Instrumentation, which delivers them to the UI thread safely.
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
