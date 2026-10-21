// In-Game Keyboard - mod entry point and menu registration.
// The keyboard and the KB button are real Android views drawn above the game,
// so they stay visible on every Minecraft screen. Settings are saved to a file.
#include <pl/Input.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "Storage.hpp"

namespace {

std::string pct(float fraction) { return std::to_string(static_cast<int>(std::lround(fraction * 100.0f))); }
std::string whole(float v) { return std::to_string(static_cast<int>(std::lround(v))); }
std::string one(float v) { char b[16]; std::snprintf(b, sizeof b, "%.1f", v); return b; }

class InGameKeyboardMod {
public:
  static InGameKeyboardMod& instance() {
    static InGameKeyboardMod mod;
    return mod;
  }

  InGameKeyboardMod() : mSelf(*ll::mod::NativeMod::current()) {}

  [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

  bool load() {
    ui::g_vm = getSelf().getJavaVM();
    return true;
  }

  bool enable() {
    mEnabled = true;

    // Bring back the saved position, size and opacity.
    store::init(getSelf().getDataDir());
    kb::Settings s = kb::getSettings();
    store::load(s);
    kb::configure(s);
    kb::g_saveHook = [](const kb::Settings& v) { store::save(v); };

    if (!mTouchRegistered.exchange(true)) {
      pl::input::registerTouchCallback([](const pl::input::TouchEvent& e) {
        if (!instance().mEnabled) return false;
        return kb::onTouch(e.action, e.pointerId, e.x, e.y);
      });
    }

    const bool moduleOk =
        pl::modmenu::ModuleBuilder(kModuleId, "In-Game Keyboard")
            .modId(getSelf().getId())
            .description("Turn on to show the KB button. Tap KB to open or close "
                         "the keyboard; drag KB to move it. Works on every screen.")
            .defaultEnabled(true)
            .onToggle(onModuleToggle)
            .config("opacity", "Opacity (%)",
                    pl::modmenu::ConfigType::SliderInt, pct(s.buttonOpacity), "10", "100")
            .config("kbOpacity", "Keyboard Opacity (%)",
                    pl::modmenu::ConfigType::SliderInt, pct(s.opacity), "10", "100")
            .config("scale", "Keyboard Scale",
                    pl::modmenu::ConfigType::SliderFloat, one(s.scale), "0.5", "2.0")
            .config("posX", "Horizontal Position (%)",
                    pl::modmenu::ConfigType::SliderInt, whole(s.xPercent), "0", "100")
            .config("posY", "Vertical Position (%)",
                    pl::modmenu::ConfigType::SliderInt, whole(s.yPercent), "0", "100")
            .onConfigChanged(onConfigChanged)
            .registerModule();
    if (!moduleOk) {
      getSelf().getLogger().error("Module registration failed");
      return false;
    }
    kb::setEnabled(true);   // the launcher replays an "off" setting through onToggle
    return true;
  }

  bool disable() {
    mEnabled = false;
    kb::setEnabled(false);
    pl::modmenu::unregisterModule(kModuleId);
    return true;
  }

  bool unload() { return true; }

private:
  ll::mod::NativeMod& mSelf;
  std::atomic_bool mEnabled{false};
  std::atomic_bool mTouchRegistered{false};

  static void onModuleToggle(std::string_view moduleId, bool enabled) {
    if (moduleId != kModuleId) return;
    kb::setEnabled(enabled);
  }

  static void onConfigChanged(std::string_view moduleId, std::string_view key,
                              std::string_view value) {
    if (moduleId != kModuleId) return;
    const float v = std::strtof(std::string(value).c_str(), nullptr);
    kb::Settings s = kb::getSettings();
    if (key == "posX")            s.xPercent = std::clamp(v, 0.0f, 100.0f);
    else if (key == "posY")       s.yPercent = std::clamp(v, 0.0f, 100.0f);
    else if (key == "scale")      s.scale = std::clamp(v, 0.5f, 2.0f);
    else if (key == "opacity")    s.buttonOpacity = std::clamp(v / 100.0f, 0.1f, 1.0f);
    else if (key == "kbOpacity")  s.opacity = std::clamp(v / 100.0f, 0.1f, 1.0f);
    else return;
    kb::configure(s);
    store::save(s);
  }
};

} // namespace

PL_REGISTER_MOD(InGameKeyboardMod, InGameKeyboardMod::instance())
