#pragma once

namespace ik::kb {

struct Settings {
  float xPercent = 50.0f;   // 0 = left edge, 100 = right edge of the screen
  float yPercent = 100.0f;  // 0 = top edge,  100 = bottom edge of the screen
  float scale = 1.0f;       // key size multiplier (auto-shrinks to fit width)
};

void configure(const Settings& s);
void setVisible(bool visible);
bool visible();
void toggle();
void shutdown();   // hides and clears the overlay

// Touch hook. Returns true when the event was consumed by the keyboard.
bool onTouch(int action, int pointerId, float x, float y);

} // namespace ik::kb
