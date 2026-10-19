#pragma once
// Keyboard: layout, drawing (only on state change) and touch handling.
#include <pl/ModMenu.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "Inject.hpp"

inline constexpr const char* kModuleId = "in_game_keyboard.module";
inline constexpr const char* kKbButtonId = "in_game_keyboard.kb";

namespace kb {

enum class Kind { Char, Shift, Back, Space, Enter, Symbols, MoreSymbols, Hide };

struct Key {
  std::string label;
  Kind kind = Kind::Char;
  float weight = 1.0f;
  float x = 0, y = 0, w = 0, h = 0;
};
struct Row { std::vector<Key> keys; };

struct Settings {
  float xPercent = 50.0f;    // 0 = left, 100 = right
  float yPercent = 100.0f;   // 0 = top,  100 = bottom
  float scale = 1.0f;        // keyboard scale
  float opacity = 0.85f;     // 0.1 .. 1.0
};

inline std::mutex       g_mutex;
inline Settings         g_cfg;
inline bool             g_visible = false;
inline bool             g_shift = false;
inline bool             g_caps = false;       // caps lock (double-tap Shift)
inline long long        g_lastShiftMs = 0;
inline int              g_page = 0;           // 0 letters, 1 symbols, 2 more symbols
inline std::string      g_buffer;
inline std::vector<Row> g_rows;
inline float            g_effScale = 1.0f;
inline float            g_panelX = 0, g_panelY = 0, g_panelW = 0, g_panelH = 0;
inline int              g_activePointer = -1;
inline int              g_pressedRow = -1, g_pressedCol = -1;
inline void (*g_onHidden)() = nullptr;   // keyboard closed itself via the Hide key

inline constexpr float kBaseUnit = 56.0f;
inline constexpr float kGap = 4.0f;
inline constexpr float kPad = 6.0f;

// Multiply the alpha channel of 0xAARRGGBB by the opacity setting.
inline std::uint32_t withOpacity(std::uint32_t argb) {
  const float a = static_cast<float>(argb >> 24) * g_cfg.opacity;
  return (static_cast<std::uint32_t>(a) << 24) | (argb & 0x00FFFFFFu);
}

inline Key mk(const std::string& label, Kind kind = Kind::Char, float weight = 1.0f) {
  Key k; k.label = label; k.kind = kind; k.weight = weight; return k;
}

inline Row charRow(const char* chars, bool symbols, bool shift) {
  Row r;
  for (const char* p = chars; *p; ++p) {
    char c = *p;
    if (!symbols && shift && c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    r.keys.push_back(mk(std::string(1, c)));
  }
  return r;
}

inline std::vector<Row> makeLayout(int page, bool shift) {
  std::vector<Row> rows;
  if (page == 0) {                       // letters
    rows.push_back(charRow("qwertyuiop", false, shift));
    rows.push_back(charRow("asdfghjkl", false, shift));
    Row r3; r3.keys.push_back(mk(g_caps ? "CAPS" : "Shift", Kind::Shift, 1.5f));
    for (auto& k : charRow("zxcvbnm", false, shift).keys) r3.keys.push_back(k);
    r3.keys.push_back(mk("<-", Kind::Back, 1.5f));
    rows.push_back(std::move(r3));
  } else if (page == 1) {                // symbols, like Gboard's ?123 page
    rows.push_back(charRow("1234567890", true, false));
    rows.push_back(charRow("@#$_&-+()/", true, false));
    Row r3; r3.keys.push_back(mk("=\\<", Kind::MoreSymbols, 1.5f));
    for (auto& k : charRow("*\"':;!?", true, false).keys) r3.keys.push_back(k);
    r3.keys.push_back(mk("<-", Kind::Back, 1.5f));
    rows.push_back(std::move(r3));
  } else {                               // more symbols, like Gboard's =\< page
    rows.push_back(charRow("~`|^{}[]<>", true, false));
    rows.push_back(charRow("=%\\#$&@+_-", true, false));
    Row r3; r3.keys.push_back(mk("?123", Kind::MoreSymbols, 1.5f));
    for (auto& k : charRow("*\"':;!?", true, false).keys) r3.keys.push_back(k);
    r3.keys.push_back(mk("<-", Kind::Back, 1.5f));
    rows.push_back(std::move(r3));
  }
  Row r4;
  r4.keys.push_back(mk(page == 0 ? "?123" : "ABC", Kind::Symbols, 1.4f));
  r4.keys.push_back(mk("/"));
  r4.keys.push_back(mk(","));
  r4.keys.push_back(mk(" ", Kind::Space, 3.0f));
  r4.keys.push_back(mk("."));
  r4.keys.push_back(mk("Enter", Kind::Enter, 1.6f));
  r4.keys.push_back(mk("Hide", Kind::Hide, 1.0f));
  rows.push_back(std::move(r4));
  return rows;
}

inline void layoutRects() {   // lock held
  g_rows = makeLayout(g_page, g_shift);

  const auto surface = pl::modmenu::getHudSurfaceSize();
  const bool haveSurface = surface.width > 0.0f && surface.height > 0.0f;

  float scale = g_cfg.scale;
  if (haveSurface) {
    const float fit = (surface.width - kPad * 2) / (kBaseUnit * 10.0f);
    if (fit < scale) scale = fit;
  }
  g_effScale = scale;
  const float unit = kBaseUnit * scale;
  const float rowW = unit * 10.0f;
  g_panelW = rowW + kPad * 2;
  g_panelH = (unit + kGap) * static_cast<float>(g_rows.size()) - kGap + kPad * 2;
  if (haveSurface) {
    const float freeX = surface.width - g_panelW;
    const float freeY = surface.height - g_panelH;
    g_panelX = (freeX > 0 ? freeX : 0) * g_cfg.xPercent / 100.0f;
    g_panelY = (freeY > 0 ? freeY : 0) * g_cfg.yPercent / 100.0f;
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

inline void submit() {   // lock held; called only when something visible changed
  std::vector<pl::modmenu::DrawCommand> cmds;
  if (g_visible) {
    cmds.reserve(80);
    pl::modmenu::DrawCommand panel;
    panel.type = pl::modmenu::DrawCommandType::RectFilled;
    panel.x = g_panelX; panel.y = g_panelY; panel.w = g_panelW; panel.h = g_panelH;
    panel.color = withOpacity(0xD0101010u);
    cmds.push_back(std::move(panel));

    const float textSize = kBaseUnit * g_effScale * 0.42f;
    for (int r = 0; r < static_cast<int>(g_rows.size()); ++r) {
      for (int c = 0; c < static_cast<int>(g_rows[r].keys.size()); ++c) {
        const Key& k = g_rows[r].keys[c];
        const bool pressed = (r == g_pressedRow && c == g_pressedCol);
        const bool special = k.kind != Kind::Char && k.kind != Kind::Space;

        pl::modmenu::DrawCommand box;
        box.type = pl::modmenu::DrawCommandType::RectFilled;
        box.x = k.x; box.y = k.y; box.w = k.w; box.h = k.h;
        box.color = withOpacity(
            pressed ? 0xFF7A7A7Au
          : (k.kind == Kind::Shift && g_shift) ? (g_caps ? 0xFF2E9E5Bu : 0xFF3C7BD9u)
          : special ? 0xFF2B3A52u : 0xFF3A3A3Au);
        cmds.push_back(std::move(box));

        if (k.kind != Kind::Space) {
          pl::modmenu::DrawCommand text;
          text.type = pl::modmenu::DrawCommandType::Text;
          const float approxW = static_cast<float>(k.label.size()) * textSize * 0.55f;
          text.x = k.x + (k.w - approxW) / 2.0f;
          text.y = k.y + (k.h - textSize) / 2.0f;
          text.w = k.w; text.h = k.h;
          text.size = textSize;
          text.color = 0xFFFFFFFFu;
          text.text = k.label;
          cmds.push_back(std::move(text));
        }
      }
    }
  }
  pl::modmenu::submitDrawCommands(kModuleId, cmds);
}

inline bool findKey(float px, float py, int& outRow, int& outCol) {
  for (int r = 0; r < static_cast<int>(g_rows.size()); ++r)
    for (int c = 0; c < static_cast<int>(g_rows[r].keys.size()); ++c) {
      const Key& k = g_rows[r].keys[c];
      if (px >= k.x && px < k.x + k.w && py >= k.y && py < k.y + k.h) {
        outRow = r; outCol = c; return true;
      }
    }
  return false;
}

inline bool inPanel(float px, float py) {
  return px >= g_panelX && px < g_panelX + g_panelW &&
         py >= g_panelY && py < g_panelY + g_panelH;
}

inline void activate(const Key& key) {   // lock held
  switch (key.kind) {
    case Kind::Char:
    case Kind::Space:
      g_buffer += (key.kind == Kind::Space) ? std::string(" ") : key.label;
      inject::setText(g_buffer);
      if (g_shift && !g_caps && g_page == 0) { g_shift = false; layoutRects(); }
      break;
    case Kind::Back:
      if (g_buffer.empty()) {
        inject::backspace();
      } else {
        g_buffer.pop_back();
        inject::setText(g_buffer);
      }
      break;
    case Kind::Enter:
      inject::enter();
      g_buffer.clear();
      break;
    case Kind::Shift: {                     // double-tap = caps lock
      const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count();
      if (g_shift && !g_caps && now - g_lastShiftMs < 400) {
        g_caps = true;
      } else if (g_caps) {
        g_caps = false; g_shift = false;
      } else {
        g_shift = !g_shift;
      }
      g_lastShiftMs = now;
      layoutRects();
      break;
    }
    case Kind::Symbols:                      // ?123 / ABC
      g_page = (g_page == 0) ? 1 : 0; g_shift = false; g_caps = false; layoutRects();
      break;
    case Kind::MoreSymbols:                  // =\< / ?123
      g_page = (g_page == 1) ? 2 : 1; layoutRects();
      break;
    case Kind::Hide:
      g_visible = false; g_buffer.clear();
      break;
  }
}

inline void configure(const Settings& s) {
  std::lock_guard lock(g_mutex);
  g_cfg = s;
  layoutRects();
  if (g_visible) submit();
}

// Returns true if the visibility actually changed.
inline bool setVisible(bool v) {
  std::lock_guard lock(g_mutex);
  if (g_visible == v) return false;
  g_visible = v;
  g_buffer.clear();
  g_page = 0; g_shift = false; g_caps = false;
  g_pressedRow = g_pressedCol = -1;
  g_activePointer = -1;
  layoutRects();
  submit();
  return true;
}

inline bool visible() {
  std::lock_guard lock(g_mutex);
  return g_visible;
}

inline bool onTouch(int rawAction, int pointerId, float x, float y) {
  const int action = rawAction & 0xFF;
  bool hidden = false;
  bool consumed = false;
  {
    std::lock_guard lock(g_mutex);
    if (!g_visible) return false;

    const bool isDown = (action == 0 || action == 5);
    const bool isUp = (action == 1 || action == 6);
    const bool isCancel = (action == 3);

    if (isDown) {
      if (!inPanel(x, y)) return false;
      g_activePointer = pointerId;
      int r, c;
      if (findKey(x, y, r, c)) { g_pressedRow = r; g_pressedCol = c; submit(); }
      return true;
    }
    if (pointerId != g_activePointer) return false;
    if (action == 2) return true;

    if (isUp || isCancel) {
      bool fire = false;
      Key key;
      int r, c;
      if (isUp && g_pressedRow >= 0 && findKey(x, y, r, c) &&
          r == g_pressedRow && c == g_pressedCol) {
        key = g_rows[r].keys[c];
        fire = true;
      }
      g_pressedRow = g_pressedCol = -1;
      g_activePointer = -1;
      if (fire) activate(key);
      hidden = !g_visible;
      submit();
      consumed = true;
    }
  }
  if (hidden && g_onHidden) g_onHidden();   // outside the lock
  return consumed;
}

} // namespace kb
