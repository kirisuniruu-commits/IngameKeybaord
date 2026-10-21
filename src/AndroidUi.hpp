#pragma once
// Android-side helpers (JNI): a real ImageView drawn on top of the game window
// (visible on every Minecraft screen) and real KeyEvent dispatch.
// Every function here must run on the Android UI thread.
#include <jni.h>
#include <cstdint>
#include <string>

namespace ui {

// One on-screen layer = an ImageView showing a bitmap we draw into.
struct Layer {
  jobject view = nullptr, bitmap = nullptr, canvas = nullptr, paint = nullptr;  // global refs
  int bw = 0, bh = 0;
};

inline JavaVM*   g_vm = nullptr;
inline Layer     g_kbLayer, g_btnLayer;   // keyboard panel and the KB button
inline jclass    g_stateCls = nullptr;
inline jmethodID g_getAct = nullptr;
inline jmethodID g_mSetColor = nullptr, g_mRound = nullptr, g_mText = nullptr,
                 g_mTextSize = nullptr, g_mErase = nullptr, g_mInvalidate = nullptr,
                 g_mAlpha = nullptr, g_mVis = nullptr;

inline JNIEnv* env() {   // only for threads already attached (the UI thread)
  JNIEnv* e = nullptr;
  if (g_vm && g_vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) == JNI_OK) return e;
  return nullptr;
}
inline void clearEx(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }

inline jmethodID mid(JNIEnv* e, jobject o, const char* n, const char* s) {
  jclass c = e->GetObjectClass(o);
  jmethodID m = e->GetMethodID(c, n, s);
  e->DeleteLocalRef(c);
  clearEx(e);
  return m;
}
inline jmethodID cmid(JNIEnv* e, jclass c, const char* n, const char* s) {
  jmethodID m = e->GetMethodID(c, n, s); clearEx(e); return m;
}
inline jmethodID smid(JNIEnv* e, jclass c, const char* n, const char* s) {
  jmethodID m = e->GetStaticMethodID(c, n, s); clearEx(e); return m;
}

inline bool onMainThread(JNIEnv* e) {
  jclass lc = e->FindClass("android/os/Looper");
  if (!lc) { clearEx(e); return false; }
  jmethodID my = smid(e, lc, "myLooper", "()Landroid/os/Looper;");
  jmethodID mn = smid(e, lc, "getMainLooper", "()Landroid/os/Looper;");
  bool same = false;
  if (my && mn) {
    jobject a = e->CallStaticObjectMethod(lc, my);
    jobject b = e->CallStaticObjectMethod(lc, mn);
    same = a && b && e->IsSameObject(a, b);
    if (a) e->DeleteLocalRef(a);
    if (b) e->DeleteLocalRef(b);
  }
  e->DeleteLocalRef(lc);
  clearEx(e);
  return same;
}

inline jobject currentActivity(JNIEnv* e) {   // local ref or nullptr
  if (!g_getAct) {
    jclass local = e->FindClass("org/levimc/launcher/core/minecraft/MinecraftActivityState");
    if (!local) { clearEx(e); return nullptr; }
    g_stateCls = static_cast<jclass>(e->NewGlobalRef(local));
    e->DeleteLocalRef(local);
    g_getAct = smid(e, g_stateCls, "getCurrentActivity", "()Landroid/app/Activity;");
    if (!g_getAct) return nullptr;
  }
  jobject a = e->CallStaticObjectMethod(g_stateCls, g_getAct);
  clearEx(e);
  return a;
}

// ---- key events -----------------------------------------------------------
inline void dispatchKey(JNIEnv* e, jobject activity, int action, int keyCode, int meta) {
  jclass sc = e->FindClass("android/os/SystemClock");
  jclass kc = e->FindClass("android/view/KeyEvent");
  if (!sc || !kc) { clearEx(e); return; }
  jmethodID up = smid(e, sc, "uptimeMillis", "()J");
  jmethodID ctor = cmid(e, kc, "<init>", "(JJIIII)V");
  jmethodID disp = mid(e, activity, "dispatchKeyEvent", "(Landroid/view/KeyEvent;)Z");
  if (!up || !ctor || !disp) return;
  jlong now = e->CallStaticLongMethod(sc, up);
  jobject ev = e->NewObject(kc, ctor, now, now, static_cast<jint>(action),
                            static_cast<jint>(keyCode), static_cast<jint>(0),
                            static_cast<jint>(meta));
  if (ev) e->CallBooleanMethod(activity, disp, ev);
  clearEx(e);
}

// action: 0 = down, 1 = up. Same path a hardware keyboard takes.
inline void sendKey(int action, int keyCode, int meta) {
  JNIEnv* e = env(); if (!e) return;
  e->PushLocalFrame(24);
  if (jobject act = currentActivity(e)) dispatchKey(e, act, action, keyCode, meta);
  e->PopLocalFrame(nullptr);
}

// Types one character (symbols etc.) using Android's key map: press + release.
inline void tapChar(char c) {
  JNIEnv* e = env(); if (!e) return;
  e->PushLocalFrame(32);
  jobject act = currentActivity(e);
  jclass kcm = e->FindClass("android/view/KeyCharacterMap");
  if (act && kcm) {
    jmethodID load = smid(e, kcm, "load", "(I)Landroid/view/KeyCharacterMap;");
    jobject map = load ? e->CallStaticObjectMethod(kcm, load, static_cast<jint>(-1)) : nullptr;
    jmethodID getEvents = map ? mid(e, map, "getEvents", "([C)[Landroid/view/KeyEvent;") : nullptr;
    if (getEvents) {
      jcharArray arr = e->NewCharArray(1);
      jchar ch = static_cast<jchar>(c);
      e->SetCharArrayRegion(arr, 0, 1, &ch);
      jobjectArray evs = static_cast<jobjectArray>(e->CallObjectMethod(map, getEvents, arr));
      clearEx(e);
      jmethodID disp = mid(e, act, "dispatchKeyEvent", "(Landroid/view/KeyEvent;)Z");
      if (evs && disp) {
        const jsize n = e->GetArrayLength(evs);
        for (jsize i = 0; i < n; ++i) {
          jobject ev = e->GetObjectArrayElement(evs, i);
          e->CallBooleanMethod(act, disp, ev);
          clearEx(e);
          e->DeleteLocalRef(ev);
        }
      }
    }
  }
  clearEx(e);
  e->PopLocalFrame(nullptr);
}

// ---- view -----------------------------------------------------------------
inline void releaseGlobals(JNIEnv* e, Layer& L) {
  for (jobject* o : {&L.view, &L.bitmap, &L.canvas, &L.paint})
    if (*o) { e->DeleteGlobalRef(*o); *o = nullptr; }
  L.bw = L.bh = 0;
}

inline void setMargins(JNIEnv* e, jobject lp, int x, int y) {
  jclass m = e->FindClass("android/view/ViewGroup$MarginLayoutParams");
  if (!m) { clearEx(e); return; }
  e->SetIntField(lp, e->GetFieldID(m, "leftMargin", "I"), x);
  e->SetIntField(lp, e->GetFieldID(m, "topMargin", "I"), y);
  clearEx(e);
}

// Creates (or repositions) the keyboard view. Returns false on failure.
inline bool ensureView(JNIEnv* e, Layer& L, int w, int h, int x, int y) {
  e->PushLocalFrame(64);
  bool ok = false;
  jobject act = currentActivity(e);
  jmethodID find = act ? mid(e, act, "findViewById", "(I)Landroid/view/View;") : nullptr;
  jobject content = find ? e->CallObjectMethod(act, find, static_cast<jint>(16908290)) : nullptr;
  if (content) {
    if (L.view) {   // still attached to this activity, same size?
      jmethodID gp = mid(e, L.view, "getParent", "()Landroid/view/ViewParent;");
      jobject parent = gp ? e->CallObjectMethod(L.view, gp) : nullptr;
      const bool attached = parent && e->IsSameObject(parent, content);
      if (!attached || w != L.bw || h != L.bh) {
        if (attached) {
          jmethodID rm = mid(e, content, "removeView", "(Landroid/view/View;)V");
          if (rm) e->CallVoidMethod(content, rm, L.view);
        }
        releaseGlobals(e, L);
      }
    }
    if (!L.view) {
      jclass cfgC = e->FindClass("android/graphics/Bitmap$Config");
      jclass bmpC = e->FindClass("android/graphics/Bitmap");
      jclass canC = e->FindClass("android/graphics/Canvas");
      jclass paintC = e->FindClass("android/graphics/Paint");
      jclass alC = e->FindClass("android/graphics/Paint$Align");
      jclass ivC = e->FindClass("android/widget/ImageView");
      jclass lpC = e->FindClass("android/widget/FrameLayout$LayoutParams");
      if (cfgC && bmpC && canC && paintC && alC && ivC && lpC) {
        jobject cfg = e->GetStaticObjectField(cfgC,
            e->GetStaticFieldID(cfgC, "ARGB_8888", "Landroid/graphics/Bitmap$Config;"));
        jmethodID cb = smid(e, bmpC, "createBitmap",
            "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
        jobject bmp = cb ? e->CallStaticObjectMethod(bmpC, cb, w, h, cfg) : nullptr;
        jobject canvas = bmp ? e->NewObject(canC, cmid(e, canC, "<init>", "(Landroid/graphics/Bitmap;)V"), bmp) : nullptr;
        jobject paint = e->NewObject(paintC, cmid(e, paintC, "<init>", "()V"));
        jobject iv = canvas ? e->NewObject(ivC, cmid(e, ivC, "<init>", "(Landroid/content/Context;)V"), act) : nullptr;
        jobject lp = e->NewObject(lpC, cmid(e, lpC, "<init>", "(II)V"), w, h);
        if (bmp && canvas && paint && iv && lp) {
          jobject center = e->GetStaticObjectField(alC,
              e->GetStaticFieldID(alC, "CENTER", "Landroid/graphics/Paint$Align;"));
          e->CallVoidMethod(paint, mid(e, paint, "setAntiAlias", "(Z)V"), JNI_TRUE);
          e->CallVoidMethod(paint, mid(e, paint, "setTextAlign", "(Landroid/graphics/Paint$Align;)V"), center);
          e->SetIntField(lp, e->GetFieldID(lpC, "gravity", "I"), 51);   // TOP | LEFT
          setMargins(e, lp, x, y);
          e->CallVoidMethod(iv, mid(e, iv, "setImageBitmap", "(Landroid/graphics/Bitmap;)V"), bmp);
          e->CallVoidMethod(iv, mid(e, iv, "setLayoutParams", "(Landroid/view/ViewGroup$LayoutParams;)V"), lp);
          e->CallVoidMethod(content, mid(e, content, "addView", "(Landroid/view/View;)V"), iv);
          g_mSetColor = mid(e, paint, "setColor", "(I)V");
          g_mTextSize = mid(e, paint, "setTextSize", "(F)V");
          g_mRound = mid(e, canvas, "drawRoundRect", "(FFFFFFLandroid/graphics/Paint;)V");
          g_mText = mid(e, canvas, "drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");
          g_mErase = mid(e, bmp, "eraseColor", "(I)V");
          g_mInvalidate = mid(e, iv, "invalidate", "()V");
          g_mAlpha = mid(e, iv, "setAlpha", "(F)V");
          g_mVis = mid(e, iv, "setVisibility", "(I)V");
          L.view = e->NewGlobalRef(iv);
          L.bitmap = e->NewGlobalRef(bmp);
          L.canvas = e->NewGlobalRef(canvas);
          L.paint = e->NewGlobalRef(paint);
          L.bw = w; L.bh = h;
        }
      }
    } else {   // just move it
      jmethodID glp = mid(e, L.view, "getLayoutParams", "()Landroid/view/ViewGroup$LayoutParams;");
      jobject lp = glp ? e->CallObjectMethod(L.view, glp) : nullptr;
      if (lp) {
        setMargins(e, lp, x, y);
        e->CallVoidMethod(L.view, mid(e, L.view, "requestLayout", "()V"));
      }
    }
    ok = L.view != nullptr;
  }
  clearEx(e);
  e->PopLocalFrame(nullptr);
  return ok;
}

inline void setVisible(JNIEnv* e, Layer& L, bool on) {   // 0 = VISIBLE, 8 = GONE
  if (L.view && g_mVis) e->CallVoidMethod(L.view, g_mVis, static_cast<jint>(on ? 0 : 8));
  clearEx(e);
}
inline void setAlpha(JNIEnv* e, Layer& L, float a) {
  if (L.view && g_mAlpha) e->CallVoidMethod(L.view, g_mAlpha, a);
  clearEx(e);
}
inline void bringToFront(JNIEnv* e, Layer& L) {
  if (!L.view) return;
  jmethodID m = mid(e, L.view, "bringToFront", "()V");
  if (m) e->CallVoidMethod(L.view, m);
  clearEx(e);
}

// ---- drawing into a layer's bitmap ----------------------------------------
inline void beginDraw(JNIEnv* e, Layer& L) { e->CallVoidMethod(L.bitmap, g_mErase, static_cast<jint>(0)); }
inline void endDraw(JNIEnv* e, Layer& L) { e->CallVoidMethod(L.view, g_mInvalidate); clearEx(e); }

inline void fillRound(JNIEnv* e, Layer& L, float l, float t, float r, float b, float rad, std::uint32_t argb) {
  e->CallVoidMethod(L.paint, g_mSetColor, static_cast<jint>(argb));
  e->CallVoidMethod(L.canvas, g_mRound, l, t, r, b, rad, rad, L.paint);
}
inline void text(JNIEnv* e, Layer& L, const std::string& s, float cx, float baseline, float size, std::uint32_t argb) {
  jstring js = e->NewStringUTF(s.c_str());
  e->CallVoidMethod(L.paint, g_mSetColor, static_cast<jint>(argb));
  e->CallVoidMethod(L.paint, g_mTextSize, size);
  e->CallVoidMethod(L.canvas, g_mText, js, cx, baseline, L.paint);
  e->DeleteLocalRef(js);
}

} // namespace ui
