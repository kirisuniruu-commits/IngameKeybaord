#pragma once
// Saves the keyboard settings in the mod's own data folder so they survive
// restarting the game.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "Keyboard.hpp"

namespace store {

inline std::filesystem::path g_file;

inline void init(const std::filesystem::path& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  g_file = dir / "settings.txt";
}

inline float clampf(float v, float lo, float hi) { return std::clamp(v, lo, hi); }

inline void load(kb::Settings& s) {
  std::ifstream in(g_file);
  if (!in) return;
  std::string line;
  while (std::getline(in, line)) {
    const auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = line.substr(0, eq);
    const float v = std::strtof(line.c_str() + eq + 1, nullptr);
    if (key == "posX")               s.xPercent = clampf(v, 0.0f, 100.0f);
    else if (key == "posY")          s.yPercent = clampf(v, 0.0f, 100.0f);
    else if (key == "scale")         s.scale = clampf(v, 0.5f, 2.0f);
    else if (key == "kbOpacity")     s.opacity = clampf(v, 0.1f, 1.0f);
    else if (key == "buttonOpacity") s.buttonOpacity = clampf(v, 0.1f, 1.0f);
    else if (key == "btnX")          s.btnX = clampf(v, 0.0f, 100.0f);
    else if (key == "btnY")          s.btnY = clampf(v, 0.0f, 100.0f);
  }
}

inline void save(const kb::Settings& s) {
  if (g_file.empty()) return;
  std::ofstream out(g_file, std::ios::trunc);
  if (!out) return;
  out << "posX=" << s.xPercent << "\n"
      << "posY=" << s.yPercent << "\n"
      << "scale=" << s.scale << "\n"
      << "kbOpacity=" << s.opacity << "\n"
      << "buttonOpacity=" << s.buttonOpacity << "\n"
      << "btnX=" << s.btnX << "\n"
      << "btnY=" << s.btnY << "\n";
}

} // namespace store
