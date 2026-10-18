#include "Keyboard.hpp"
#include "TextInject.hpp"

#include <pl/Input.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>

#include <atomic>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

constexpr const char* kModuleId = "in_game_keyboard.module";
constexpr const char* kButtonId = "in_game_keyboard.button";

class InGameKeyboardMod {
public:
  static InGameKeyboardMod& instance() {
    static InGameKeyboardMod mod;
    return mod;
  }

  InGameKeyboardMod() : mSelf(*ll::mod::NativeMod::current()) {}

  [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

  bool load() {
    const bool ok = ik::inject::init(getSelf().getJavaVM());
    if (!ok) {
      // Keep loading so the menu entry still shows; typing will be a no-op.
      getSelf().getLogger().error(
          "Text-input natives not found in libminecraftpe.so; "
          "keyboard will draw but cannot type");
    }
    return true;
  }

  bool enable() {
    mEnabled = true;

    // One-time: touch callbacks cannot be unregistered, so gate on mEnabled.
    if (!mTouchRegistered.exchange(true)) {
      pl::input::registerTouchCallback([](const pl::input::TouchEvent& e) {
        if (!instance().mEnabled) return false;
        return ik::kb::onTouch(e.action, e.pointerId, e.x, e.y);
      });
    }

    const bool moduleOk =
        pl::modmenu::ModuleBuilder(kModuleId, "In-Game Keyboard")
            .modId(getSelf().getId())
            .description("Floating on-screen keyboard for chat, commands, "
                         "signs and search bars.")
            .defaultEnabled(false)
            .onToggle(onModuleToggle)
            .config("posX", "Horizontal Position (%)",
                    pl::modmenu::ConfigType::SliderInt, "50", "0", "100")
            .config("posY", "Vertical Position (%)",
                    pl::modmenu::ConfigType::SliderInt, "100", "0", "100")
            .config("scale", "Key Size", pl::modmenu::ConfigType::SliderFloat,
                    "1.0", "0.5", "2.0")
            .onConfigChanged(onConfigChanged)
            .registerModule();

    // Floating bubble to show/hide the keyboard without opening the menu.
    const bool buttonOk =
        moduleOk &&
        pl::modmenu::ButtonBuilder(kButtonId, "In-Game Keyboard")
            .moduleId(kModuleId)
            .label("KB")
            .behavior(pl::modmenu::ButtonBehavior::Toggle)
            .onEvent(onButtonEvent)
            .registerButton();

    if (!moduleOk || !buttonOk) {
      getSelf().getLogger().error("Menu registration failed: module={} button={}",
                                  moduleOk, buttonOk);
    }
    return moduleOk;
  }

  bool disable() {
    mEnabled = false;
    ik::kb::shutdown();
    pl::modmenu::unregisterButton(kButtonId);
    pl::modmenu::unregisterModule(kModuleId);
    return true;
  }

  bool unload() { return true; }

private:
  ll::mod::NativeMod& mSelf;
  std::atomic_bool mEnabled{false};
  std::atomic_bool mTouchRegistered{false};
  ik::kb::Settings mSettings;

  static void onModuleToggle(std::string_view moduleId, bool enabled) {
    if (moduleId != kModuleId) return;
    ik::kb::setVisible(enabled);
  }

  static void onButtonEvent(std::string_view buttonId,
                            pl::modmenu::ButtonEvent event, float value) {
    if (buttonId != kButtonId ||
        event != pl::modmenu::ButtonEvent::StateChanged) return;
    ik::kb::setVisible(value > 0.5f);
  }

  static void onConfigChanged(std::string_view moduleId, std::string_view key,
                              std::string_view value) {
    if (moduleId != kModuleId) return;
    auto& self = instance();
    const std::string v(value);
    if (key == "posX")       self.mSettings.xPercent = std::strtof(v.c_str(), nullptr);
    else if (key == "posY")  self.mSettings.yPercent = std::strtof(v.c_str(), nullptr);
    else if (key == "scale") self.mSettings.scale = std::strtof(v.c_str(), nullptr);
    else return;
    if (self.mSettings.scale < 0.5f) self.mSettings.scale = 0.5f;
    ik::kb::configure(self.mSettings);
  }
};

} // namespace

PL_REGISTER_MOD(InGameKeyboardMod, InGameKeyboardMod::instance())
