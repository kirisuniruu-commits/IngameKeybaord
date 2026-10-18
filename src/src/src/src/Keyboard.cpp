#include "Keyboard.hpp"
#include "TextInject.hpp"

#include <pl/ModMenu.hpp>

#include <mutex>
#include <string>
#include <vector>

namespace ik::kb {
namespace {

constexpr const char* kModuleId = "in_game_keyboard.module";

enum class Kind { Char, Shift, Back, Space, Enter, Symbols, Hide };

struct Key {
  std::string label;
  Kind kind = Kind::Char;
  float weight = 1.0f;               // relative width inside a row
  float x = 0, y = 0, w = 0, h = 0;  // absolute overlay pixels
};

struct Row { std::vector<Key> keys; };

std::mutex       g_mutex;
Settings         g_cfg;
bool             g_visible = false;
bool             g_shift = false;
bool             g_symbols = false;
std::string      g_buffer;           // text typed since last Enter/Hide
std::vector<Row> g_rows;
float            g_effScale = 1.0f;
float            g_panelX = 0, g_panelY = 0, g_panelW = 0, g_panelH = 0;
int              g_activePointer = -1;
int              g_pressedRow = -1, g_pressedCol = -1;

constexpr float kBaseUnit = 56.0f;   // key size at scale 1
constexpr float kGap = 4.0f;
constexpr float kPad = 6.0f;

Key mk(const std::string& label, Kind kind = Kind::Char, float weight = 1.0f) {
  Key k; k.label = label; k.kind = kind; k.weight = weight; return k;
}

Row charRow(const char* chars, bool symbols, bool shift) {
  Row r;
  for (const char* p = chars; *p; ++p) {
    char c = *p;
    if (!symbols && shift && c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    r.keys.push_back(mk(std::string(1, c)));
  }
  return r;
}

std::vector<Row> makeLayout(bool symbols, bool shift) {
  std::vector<Row> rows;
  if (!symbols) {
    rows.push_back(charRow("qwertyuiop", false, shift));
    rows.push_back(charRow("asdfghjkl", false, shift));
    Row r3; r3.keys.push_back(mk("Shift", Kind::Shift, 1.5f));
    for (auto& k : charRow("zxcvbnm", false, shift).keys) r3.keys.push_back(k);
    r3.keys.push_back(mk("<-", Kind::Back, 1.5f));
    rows.push_back(std::move(r3));
  } else {
    rows.push_back(charRow("1234567890", true, false));
    rows.push_back(charRow("@#$%&-+()/", true, false));
    Row r3 = charRow("*\"':;!?_~", true, false);
    r3.keys.push_back(mk("<-", Kind::Back, 1.5f));
    rows.push_back(std::move(r3));
  }
  Row r4;
  r4.keys.push_back(mk(symbols ? "ABC" : "?123", Kind::Symbols, 1.5f));
  r4.keys.push_back(mk(","));
  r4.keys.push_back(mk(" ", Kind::Space, 3.5f));
  r4.keys.push_back(mk("."));
  r4.keys.push_back(mk("Enter", Kind::Enter, 1.8f));
  r4.keys.push_back(mk("Hide", Kind::Hide, 1.2f));
  rows.push_back(std::move(r4));
  return rows;
}

// Recompute absolute rectangles. Caller holds the lock.
void layoutRects() {
  g_rows = makeLayout(g_symbols, g_shift);

  // Screen size in the same pixel space as the draw commands.
  const auto surface = pl::modmenu::getHudSurfaceSize();
  const bool haveSurface = surface.width > 0.0f && surface.height > 0.0f;

  float scale = g_cfg.scale;
  if (haveSurface) {  // never wider than the screen
    const float fit = (surface.width - kPad * 2) / (kBaseUnit * 10.0f);
    if (fit < scale) scale = fit;
  }
  g_effScale = scale;
  const float unit = kBaseUnit * scale;
  const float rowW = unit * 10.0f;   // a full row is 10 unit-wide keys
  g_panelW = rowW + kPad * 2;
  g_panelH = (unit + kGap) * static_cast<float>(g_rows.size()) - kGap + kPad * 2;
  if (haveSurface) {
    const float freeX = surface.width - g_panelW;
    const float freeY = surface.height - g_panelH;
    g_panelX = (freeX > 0 ? freeX : 0) * g_cfg.xPercent / 100.0f;
    g_panelY = (freeY > 0 ? freeY : 0) * g_cfg.yPercent / 100.0f;
  } else {
    g_panelX = 40.0f; g_panelY = 300.0f;   // size not reported yet
  }

  float y = g_panelY + kPad;
  for (auto& row : g_rows) {
    float weightSum = 0;
    for (auto& k : row.keys) weightSum += k.weight;
    const float gaps = kGap * static_cast<float>(row.keys.size() - 1);
    // Rows that fill the width stretch; short rows (e.g. "asdfghjkl") center.
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

// Build and submit draw commands. Caller holds the lock. Only called when
// something visible changed (show/hide, key press/release, layout switch),
// never per frame.
void submit() {
  std::vector<pl::modmenu::DrawCommand> cmds;
  if (g_visible) {
    cmds.reserve(80);
    pl::modmenu::DrawCommand panel;
    panel.type = pl::modmenu::DrawCommandType::RectFilled;
    panel.x = g_panelX; panel.y = g_panelY; panel.w = g_panelW; panel.h = g_panelH;
    panel.color = 0xD0101010u;
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
        box.color = pressed ? 0xFF7A7A7Au
                  : (k.kind == Kind::Shift && g_shift) ? 0xFF3C7BD9u
                  : special ? 0xFF2B3A52u : 0xFF3A3A3Au;
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

bool findKey(float px, float py, int& outRow, int& outCol) {
  for (int r = 0; r < static_cast<int>(g_rows.size()); ++r)
    for (int c = 0; c < static_cast<int>(g_rows[r].keys.size()); ++c) {
      const Key& k = g_rows[r].keys[c];
      if (px >= k.x && px < k.x + k.w && py >= k.y && py < k.y + k.h) {
        outRow = r; outCol = c; return true;
      }
    }
  return false;
}

bool inPanel(float px, float py) {
  return px >= g_panelX && px < g_panelX + g_panelW &&
         py >= g_panelY && py < g_panelY + g_panelH;
}

// Caller holds the lock.
void activate(const Key& key) {
  switch (key.kind) {
    case Kind::Char:
    case Kind::Space:
      g_buffer += (key.kind == Kind::Space) ? std::string(" ") : key.label;
      ik::inject::setText(g_buffer);
      if (g_shift && !g_symbols) { g_shift = false; layoutRects(); }  // one-shot shift
      break;
    case Kind::Back:
      if (g_buffer.empty()) {
        ik::inject::backspace();      // delete text the game already had
      } else {
        g_buffer.pop_back();
        ik::inject::setText(g_buffer);
      }
      break;
    case Kind::Enter:
      ik::inject::enter();
      g_buffer.clear();
      break;
    case Kind::Shift:
      g_shift = !g_shift; layoutRects();
      break;
    case Kind::Symbols:
      g_symbols = !g_symbols; g_shift = false; layoutRects();
      break;
    case Kind::Hide:
      g_visible = false; g_buffer.clear();
      break;
  }
}

} // namespace

void configure(const Settings& s) {
  std::lock_guard lock(g_mutex);
  g_cfg = s;
  layoutRects();
  if (g_visible) submit();
}

void setVisible(bool v) {
  std::lock_guard lock(g_mutex);
  if (g_visible == v) return;
  g_visible = v;
  g_buffer.clear();
  g_pressedRow = g_pressedCol = -1;
  g_activePointer = -1;
  layoutRects();
  submit();
}

bool visible() {
  std::lock_guard lock(g_mutex);
  return g_visible;
}

void toggle() { setVisible(!visible()); }

void shutdown() {
  std::lock_guard lock(g_mutex);
  g_visible = false;
  submit();   // an empty command list clears the overlay
}

bool onTouch(int rawAction, int pointerId, float x, float y) {
  const int action = rawAction & 0xFF;   // strip pointer-index bits
  std::lock_guard lock(g_mutex);
  if (!g_visible) return false;

  const bool isDown = (action == 0 || action == 5);
  const bool isUp = (action == 1 || action == 6);
  const bool isCancel = (action == 3);

  if (isDown) {
    if (!inPanel(x, y)) return false;    // let the game have it
    g_activePointer = pointerId;
    int r, c;
    if (findKey(x, y, r, c)) {
      g_pressedRow = r; g_pressedCol = c;
      submit();
    }
    return true;
  }

  if (pointerId != g_activePointer) return false;
  if (action == 2) return true;          // move: consume only

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
    submit();                            // release highlight / new layout
    return true;
  }
  return false;
}

} // namespace ik::kb
