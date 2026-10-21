#pragma once
// Full on-screen keyboard and a movable KB button. Keys act like a physical keyboard: key down on
// touch, key up on release (so W/A/S/D can be held), and the same events type
// into Minecraft's chat, command blocks, signs and search bars.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <pl/ModMenu.hpp>

#include "Extras.hpp"

inline constexpr const char* kModuleId = "in_game_keyboard.module";
inline constexpr const char* kKbButtonId = "in_game_keyboard.kb";

namespace kb {

enum class Kind { Char, Shift, Back, Space, Enter, Esc, Tab, Symbols, MoreSymbols, Hide };

struct Key {
  std::string label;
  Kind kind = Kind::Char;
  float weight = 1.0f;
  float x = 0, y = 0, w = 0, h = 0;   // window pixels
};
struct Row { std::vector<Key> keys; };

struct Settings {
  float xPercent = 50.0f;    // 0 = left, 100 = right
  float yPercent = 100.0f;   // 0 = top,  100 = bottom
  float scale = 1.0f;
  float opacity = 0.85f;     // keyboard opacity, 0.1 .. 1.0
  float buttonOpacity = 0.85f;   // KB button opacity
  float btnX = 97.0f;        // KB button position, % of free screen space
  float btnY = 45.0f;
};

struct Press {               // one finger holding one key
  bool active = false;
  int pointerId = -1, row = -1, col = -1, keyCode = 0, meta = 0;
  bool repeat = false;       // Backspace auto-repeat running
};

inline std::mutex       g_mutex;
inline Settings         g_cfg;
inline bool             g_visible = false;    // keyboard open
inline bool             g_enabled = false;    // module on: KB button is shown
inline void           (*g_saveHook)(const Settings&) = nullptr;   // called after the button is dragged
inline bool             g_dirty = false;      // UI change waiting for the UI thread
inline bool             g_shift = false, g_caps = false;
inline long long        g_lastShiftMs = 0;
inline int              g_page = 0;           // 0 letters, 1 symbols, 2 more symbols
inline std::vector<Row> g_rows;
inline float            g_effScale = 1.0f;
inline float            g_panelX = 0, g_panelY = 0, g_panelW = 0, g_panelH = 0;
inline Press            g_press[6];

// KB button state (a draggable on-screen button; tap toggles the keyboard)
inline constexpr float kBtnSize = 64.0f, kDragSlop = 18.0f;
inline float g_btnX = 0, g_btnY = 0, g_surfW = 0, g_surfH = 0;
inline int   g_btnPointer = -1;
inline bool  g_btnMoved = false;
inline float g_btnDownX = 0, g_btnDownY = 0, g_btnStartX = 0, g_btnStartY = 0;

inline constexpr float kBaseUnit = 56.0f, kGap = 4.0f, kPad = 6.0f;

inline Key mk(const std::string& label, Kind kind = Kind::Char, float weight = 1.0f) {
  Key k; k.label = label; k.kind = kind; k.weight = weight; return k;
}

inline Row charRow(const char* chars, bool shift) {
  Row r;
  for (const char* p = chars; *p; ++p) {
    char c = *p;
    if (shift && c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    r.keys.push_back(mk(std::string(1, c)));
  }
  return r;
}

inline std::vector<Row> makeLayout(int page, bool shift) {
  std::vector<Row> rows;
  const char* digits = "1234567890";
  if (page == 0) {
    rows.push_back(charRow(digits, false));
    rows.push_back(charRow("qwertyuiop", shift));
    rows.push_back(charRow("asdfghjkl", shift));
    Row r; r.keys.push_back(mk("Caps", Kind::Shift, 1.5f));
    for (auto& k : charRow("zxcvbnm", shift).keys) r.keys.push_back(k);
    r.keys.push_back(mk("<-", Kind::Back, 1.5f));
    rows.push_back(std::move(r));
  } else if (page == 1) {
    rows.push_back(charRow(digits, false));
    rows.push_back(charRow("@#$_&-+()/", false));
    Row r3 = charRow("*\"':;!?%", false);
    r3.keys.push_back(mk("<-", Kind::Back, 2.0f));
    rows.push_back(std::move(r3));
    Row r4; r4.keys.push_back(mk("=\\<", Kind::MoreSymbols, 1.5f));
    for (auto& k : charRow("~`|^", false).keys) r4.keys.push_back(k);
    r4.keys.push_back(mk("Tab", Kind::Tab, 2.5f));
    rows.push_back(std::move(r4));
  } else {
    rows.push_back(charRow("[]{}<>=^|~", false));
    rows.push_back(charRow("\\`%#@$&*_+", false));
    Row r3 = charRow(".,:;!?\"'", false);
    r3.keys.push_back(mk("<-", Kind::Back, 2.0f));
    rows.push_back(std::move(r3));
    Row r4; r4.keys.push_back(mk("?123", Kind::MoreSymbols, 1.5f));
    r4.keys.push_back(mk("Tab", Kind::Tab, 2.5f));
    rows.push_back(std::move(r4));
  }
  Row b;
  b.keys.push_back(mk(page == 0 ? "?123" : "ABC", Kind::Symbols, 1.3f));
  b.keys.push_back(mk("Esc", Kind::Esc, 1.0f));
  b.keys.push_back(mk("/", Kind::Char, 0.9f));
  b.keys.push_back(mk(",", Kind::Char, 0.9f));
  b.keys.push_back(mk(" ", Kind::Space, 2.4f));
  b.keys.push_back(mk(".", Kind::Char, 0.9f));
  b.keys.push_back(mk("Enter", Kind::Enter, 1.6f));
  b.keys.push_back(mk("Hide", Kind::Hide, 1.0f));
  rows.push_back(std::move(b));
  return rows;
}

inline void layoutRects() {   // lock held
  g_rows = makeLayout(g_page, g_shift);
  const auto surface = pl::modmenu::getHudSurfaceSize();
  const bool haveSurface = surface.width > 0.0f && surface.height > 0.0f;

  float scale = g_cfg.scale;
  if (haveSurface) scale = std::min(scale, (surface.width - kPad * 2) / (kBaseUnit * 10.0f));
  g_effScale = scale;
  const float unit = kBaseUnit * scale;
  const float rowW = unit * 10.0f;
  g_panelW = rowW + kPad * 2;
  g_panelH = (unit + kGap) * static_cast<float>(g_rows.size()) - kGap + kPad * 2;
  if (haveSurface) {
    g_panelX = std::max(0.0f, surface.width - g_panelW) * g_cfg.xPercent / 100.0f;
    g_panelY = std::max(0.0f, surface.height - g_panelH) * g_cfg.yPercent / 100.0f;
  } else {
    g_panelX = 40.0f; g_panelY = 300.0f;
  }
  float y = g_panelY + kPad;
  for (auto& row : g_rows) {
    float weightSum = 0;
    for (auto& k : row.keys) weightSum += k.weight;
    const float gaps = kGap * static_cast<float>(row.keys.size() - 1);
    const bool full = weightSum >= 9.5f;
    const float unitW = full ? (rowW - gaps) / weightSum : unit;
    const float rowPx = unitW * weightSum + gaps;
    float x = g_panelX + kPad + (rowW - rowPx) / 2.0f;
    for (auto& k : row.keys) {
      k.x = x; k.y = y; k.w = unitW * k.weight; k.h = unit;
      x += k.w + kGap;
    }
    y += unit + kGap;
  }
}

inline bool isPressed(int r, int c) {
  for (const auto& p : g_press) if (p.active && p.row == r && p.col == c) return true;
  return false;
}

inline void render(JNIEnv* e) {   // lock held, UI thread
  e->PushLocalFrame(32);
  ui::Layer& L = ui::g_kbLayer;
  ui::beginDraw(e, L);
  const float unit = kBaseUnit * g_effScale;
  ui::fillRound(e, L, 0, 0, g_panelW, g_panelH, 14.0f * g_effScale, 0xE6101010u);
  for (int r = 0; r < static_cast<int>(g_rows.size()); ++r) {
    for (int c = 0; c < static_cast<int>(g_rows[r].keys.size()); ++c) {
      const Key& k = g_rows[r].keys[c];
      const bool special = k.kind != Kind::Char && k.kind != Kind::Space;
      std::uint32_t color = isPressed(r, c) ? 0xFF7A7A7Au
          : (k.kind == Kind::Shift && g_shift) ? (g_caps ? 0xFF2E9E5Bu : 0xFF3C7BD9u)
          : special ? 0xFF2B3A52u : 0xFF3A3A3Au;
      const float l = k.x - g_panelX, t = k.y - g_panelY;
      ui::fillRound(e, L, l, t, l + k.w, t + k.h, 8.0f * g_effScale, color);
      if (k.kind != Kind::Space)
        ui::text(e, L, k.label, l + k.w / 2.0f, t + k.h / 2.0f + unit * 0.15f, unit * 0.42f, 0xFFFFFFFFu);
    }
  }
  ui::endDraw(e, L);
  e->PopLocalFrame(nullptr);
}

// ---- KB button -------------------------------------------------------------
inline void layoutButton() {   // lock held
  const auto surface = pl::modmenu::getHudSurfaceSize();
  g_surfW = surface.width; g_surfH = surface.height;
  if (g_surfW > 0 && g_surfH > 0) {
    g_btnX = std::max(0.0f, g_surfW - kBtnSize) * g_cfg.btnX / 100.0f;
    g_btnY = std::max(0.0f, g_surfH - kBtnSize) * g_cfg.btnY / 100.0f;
  } else {
    g_btnX = 20.0f; g_btnY = 300.0f;
  }
}

inline bool inButton(float px, float py) {
  return g_enabled && px >= g_btnX && px < g_btnX + kBtnSize && py >= g_btnY && py < g_btnY + kBtnSize;
}

inline void renderButton(JNIEnv* e) {   // lock held, UI thread
  e->PushLocalFrame(16);
  ui::Layer& L = ui::g_btnLayer;
  ui::beginDraw(e, L);
  const std::uint32_t color = g_btnPointer >= 0 ? 0xFF7A7A7Au : g_visible ? 0xFF3C7BD9u : 0xFF2B2B2Bu;
  ui::fillRound(e, L, 0, 0, kBtnSize, kBtnSize, 14.0f, color);
  ui::text(e, L, "KB", kBtnSize / 2.0f, kBtnSize / 2.0f + 9.0f, 26.0f, 0xFFFFFFFFu);
  ui::endDraw(e, L);
  e->PopLocalFrame(nullptr);
}

inline void applyButton(JNIEnv* e) {   // lock held, UI thread
  if (!g_enabled) { ui::setVisible(e, ui::g_btnLayer, false); return; }
  layoutButton();
  const int size = static_cast<int>(kBtnSize);
  if (!ui::ensureView(e, ui::g_btnLayer, size, size, static_cast<int>(g_btnX), static_cast<int>(g_btnY))) return;
  ui::setAlpha(e, ui::g_btnLayer, g_cfg.buttonOpacity);
  ui::setVisible(e, ui::g_btnLayer, true);
  ui::bringToFront(e, ui::g_btnLayer);   // always above the keyboard
  renderButton(e);
}

// Bring the Android view in line with the state. Lock held.
inline void apply(bool knownUiThread) {
  JNIEnv* e = ui::env();
  if (!e || (!knownUiThread && !ui::onMainThread(e))) { g_dirty = true; return; }
  g_dirty = false;
  if (!g_visible) {
    ui::setVisible(e, ui::g_kbLayer, false);
  } else {
    layoutRects();
    const int w = static_cast<int>(std::ceil(g_panelW)), h = static_cast<int>(std::ceil(g_panelH));
    if (ui::ensureView(e, ui::g_kbLayer, w, h, static_cast<int>(g_panelX), static_cast<int>(g_panelY))) {
      ui::setAlpha(e, ui::g_kbLayer, g_cfg.opacity);
      ui::setVisible(e, ui::g_kbLayer, true);
      render(e);
    }
  }
  applyButton(e);
}

inline int holdKeyCode(const Key& k) {   // Android KeyEvent codes
  switch (k.kind) {
    case Kind::Space: return 62;
    case Kind::Enter: return 66;
    case Kind::Esc:   return 111;
    case Kind::Tab:   return 61;
    case Kind::Char: {
      const char c = k.label[0];
      if (c >= 'a' && c <= 'z') return 29 + (c - 'a');
      if (c >= 'A' && c <= 'Z') return 29 + (c - 'A');
      if (c >= '0' && c <= '9') return 7 + (c - '0');
      return 0;
    }
    default: return 0;
  }
}
inline bool isLetter(const Key& k) {
  return k.kind == Kind::Char && std::isalpha(static_cast<unsigned char>(k.label[0]));
}

inline Press* freePress() {
  for (auto& p : g_press) if (!p.active) return &p;
  return nullptr;
}
inline Press* findPress(int pointerId) {
  for (auto& p : g_press) if (p.active && p.pointerId == pointerId) return &p;
  return nullptr;
}
inline void clearPresses() {
  for (auto& p : g_press) { if (p.repeat) extras::stopRepeat(); p = Press{}; }
}

inline void pressKey(const Key& k, int pointerId, int row, int col) {   // lock held
  Press* p = freePress();
  if (!p) return;
  p->active = true; p->pointerId = pointerId; p->row = row; p->col = col;
  if (k.kind == Kind::Back) {          // one delete now, then repeat while held
    ui::sendKey(0, 67, 0);
    ui::sendKey(1, 67, 0);
    p->repeat = true;
    extras::startRepeat(67, 450, 110);
    return;
  }
  const int code = holdKeyCode(k);
  if (code) {
    const int meta = isLetter(k) && (g_shift || g_caps)
        ? (0x41 | (g_caps ? 0x100 : 0)) : 0;     // SHIFT_ON | SHIFT_LEFT_ON (| CAPS_LOCK_ON)
    p->keyCode = code; p->meta = meta;
    ui::sendKey(0, code, meta);                  // key down; key up comes on release
    if (isLetter(k) && g_shift && !g_caps) { g_shift = false; layoutRects(); }
    return;
  }
  switch (k.kind) {
    case Kind::Char: ui::tapChar(k.label[0]); break;
    case Kind::Shift: {
      const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count();
      if (g_shift && !g_caps && now - g_lastShiftMs < 400) g_caps = true;      // double-tap
      else if (g_caps) { g_caps = false; g_shift = false; }
      else g_shift = !g_shift;
      g_lastShiftMs = now;
      layoutRects();
      break;
    }
    case Kind::Symbols:
      g_page = (g_page == 0) ? 1 : 0; g_shift = g_caps = false; clearPresses(); layoutRects(); break;
    case Kind::MoreSymbols:
      g_page = (g_page == 1) ? 2 : 1; clearPresses(); layoutRects(); break;
    case Kind::Hide:
      g_visible = false; clearPresses(); break;
    default: break;
  }
}

inline bool findKey(float px, float py, int& row, int& col) {
  for (int r = 0; r < static_cast<int>(g_rows.size()); ++r)
    for (int c = 0; c < static_cast<int>(g_rows[r].keys.size()); ++c) {
      const Key& k = g_rows[r].keys[c];
      if (px >= k.x && px < k.x + k.w && py >= k.y && py < k.y + k.h) { row = r; col = c; return true; }
    }
  return false;
}
inline bool inPanel(float px, float py) {
  return px >= g_panelX && px < g_panelX + g_panelW && py >= g_panelY && py < g_panelY + g_panelH;
}

inline void releasePress(Press& p) {   // lock held
  if (p.repeat) extras::stopRepeat();
  if (p.keyCode) ui::sendKey(1, p.keyCode, p.meta);   // key up, even if the finger slid away
  p = Press{};
}

inline Settings getSettings() {
  std::lock_guard lock(g_mutex);
  return g_cfg;
}

inline void configure(const Settings& s) {
  std::lock_guard lock(g_mutex);
  g_cfg = s;
  apply(false);
}

inline void setVisibleLocked(bool v) {   // lock held
  g_visible = v;
  g_page = 0; g_shift = g_caps = false;
  for (auto& p : g_press) if (p.active) releasePress(p);
  apply(false);
}

// Open or close the keyboard.
inline void setVisible(bool v) {
  std::lock_guard lock(g_mutex);
  if (g_visible == v && !g_dirty) return;
  setVisibleLocked(v);
}

// Module on/off: shows or hides the KB button. Never opens the keyboard by itself.
inline void setEnabled(bool on) {
  std::lock_guard lock(g_mutex);
  g_enabled = on;
  if (!on) g_visible = false;
  g_page = 0; g_shift = g_caps = false;
  for (auto& p : g_press) if (p.active) releasePress(p);
  apply(false);
}

inline bool buttonTouch(int action, int pointerId, float x, float y) {   // lock held, UI thread
  if (action == 0 || action == 5) {
    if (g_btnPointer >= 0 || !inButton(x, y)) return false;
    g_btnPointer = pointerId; g_btnMoved = false;
    g_btnDownX = x; g_btnDownY = y; g_btnStartX = g_btnX; g_btnStartY = g_btnY;
    apply(true);                          // pressed look
    return true;
  }
  if (pointerId != g_btnPointer) return false;
  if (action == 2) {                      // drag the button
    const float dx = x - g_btnDownX, dy = y - g_btnDownY;
    if (!g_btnMoved && std::hypot(dx, dy) > kDragSlop) g_btnMoved = true;
    if (g_btnMoved && g_surfW > 0 && g_surfH > 0) {
      g_btnX = std::clamp(g_btnStartX + dx, 0.0f, std::max(0.0f, g_surfW - kBtnSize));
      g_btnY = std::clamp(g_btnStartY + dy, 0.0f, std::max(0.0f, g_surfH - kBtnSize));
      if (JNIEnv* e = ui::env())
        ui::ensureView(e, ui::g_btnLayer, static_cast<int>(kBtnSize), static_cast<int>(kBtnSize),
                       static_cast<int>(g_btnX), static_cast<int>(g_btnY));
    }
    return true;
  }
  if (action == 1 || action == 6 || action == 3) {
    const bool moved = g_btnMoved, tap = (action != 3) && !moved;
    g_btnPointer = -1; g_btnMoved = false;
    if (moved && g_surfW > kBtnSize && g_surfH > kBtnSize) {   // remember the new spot
      g_cfg.btnX = g_btnX / (g_surfW - kBtnSize) * 100.0f;
      g_cfg.btnY = g_btnY / (g_surfH - kBtnSize) * 100.0f;
      if (g_saveHook) g_saveHook(g_cfg);
    }
    if (tap) setVisibleLocked(!g_visible); else apply(true);
    return true;
  }
  return true;
}

// Runs on the UI thread for every touch. Returns true if the keyboard used it.
inline bool onTouch(int rawAction, int pointerId, float x, float y) {
  const int action = rawAction & 0xFF;   // 0/5 down, 1/6 up, 2 move, 3 cancel
  std::lock_guard lock(g_mutex);
  if (g_dirty) apply(true);
  if (g_enabled && (g_btnPointer >= 0 || action == 0 || action == 5) &&
      buttonTouch(action, pointerId, x, y)) return true;
  if (!g_visible) return false;

  if (action == 0 || action == 5) {
    if (!inPanel(x, y)) return false;    // game gets everything outside the keyboard
    int r, c;
    if (findKey(x, y, r, c)) { pressKey(g_rows[r].keys[c], pointerId, r, c); apply(true); }
    return true;
  }
  if (action == 3) {                     // cancel: let go of everything
    bool any = false;
    for (auto& p : g_press) if (p.active) { releasePress(p); any = true; }
    if (any) apply(true);
    return any;
  }
  Press* p = findPress(pointerId);
  if (!p) return false;
  if (action == 2) return true;
  if (action == 1 || action == 6) { releasePress(*p); apply(true); return true; }
  return false;
}

} // namespace kb
