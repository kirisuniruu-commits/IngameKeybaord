// In-Game Keyboard - mod entry point and menu registration.
// No ImGui. Uses only preloader-android APIs: ModuleBuilder, ButtonBuilder
// (hold buttons that send real Android key events) and the touch callback.
#include <pl/Input.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>

#include "Keyboard.hpp"

namespace {

// Android KeyEvent codes for the hold-to-move buttons.
struct MoveKey { const char* id; const char* label; int keyCode; };
constexpr MoveKey kMoveKeys[] = {
    {"in_game_keyboard.w", "W", 51},  // KEYCODE_W
    {"in_game_keyboard.a", "A", 29},  // KEYCODE_A
    {"in_game_keyboard.s", "S", 47},  // KEYCODE_S
    {"in_game_keyboard.d", "D", 32},  // KEYCODE_D
};

class InGameKeyboardMod {
public:
  static InGameKeyboardMod& instance() {
    static InGameKeyboardMod mod;
    return mod;
  }

  InGameKeyboardMod() : mSelf(*ll::mod::NativeMod::current()) {}

  [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

  bool load() {
    if (!inject::init(getSelf().getJavaVM())) {
      getSelf().getLogger().error(
          "Text-input natives not found in libminecraftpe.so; "
          "keyboard will draw but cannot type");
    }
    return true;
  }

  bool enable() {
    mEnabled = true;
    kb::g_onHidden = &InGameKeyboardMod::onKeyboardHidden;

    if (!mTouchRegistered.exchange(true)) {
      pl::input::registerTouchCallback([](const pl::input::TouchEvent& e) {
        if (!instance().mEnabled) return false;
        return kb::onTouch(e.action, e.pointerId, e.x, e.y);
      });
    }

    const bool moduleOk =
        pl::modmenu::ModuleBuilder(kModuleId, "In-Game Keyboard")
            .modId(getSelf().getId())
            .description("On-screen keyboard plus hold-to-move W/A/S/D buttons.")
            .defaultEnabled(false)
            .onToggle(onModuleToggle)
            .config("opacity", "Opacity (%)",
                    pl::modmenu::ConfigType::SliderInt, "85", "10", "100")
            .config("buttonSize", "Button Size (%)",
                    pl::modmenu::ConfigType::SliderInt, "100", "50", "200")
            .config("scale", "Keyboard Scale",
                    pl::modmenu::ConfigType::SliderFloat, "1.0", "0.5", "2.0")
            .config("posX", "Horizontal Position (%)",
                    pl::modmenu::ConfigType::SliderInt, "50", "0", "100")
            .config("posY", "Vertical Position (%)",
                    pl::modmenu::ConfigType::SliderInt, "100", "0", "100")
            .onConfigChanged(onConfigChanged)
            .registerModule();
    if (!moduleOk) {
      getSelf().getLogger().error("Module registration failed");
      return false;
    }
    registerKbButton();
    return true;
  }

  bool disable() {
    mEnabled = false;
    kb::setVisible(false);
    unregisterMoveButtons();
    pl::modmenu::unregisterButton(kKbButtonId);
    pl::modmenu::unregisterModule(kModuleId);
    return true;
  }

  bool unload() { return true; }

private:
  ll::mod::NativeMod& mSelf;
  std::atomic_bool mEnabled{false};
  std::atomic_bool mTouchRegistered{false};
  std::mutex mLock;
  kb::Settings mSettings;
  float mButtonSize = 1.0f;
  bool mMoveButtonsShown = false;

  static std::uint32_t alphaColor(std::uint32_t argb, float opacity) {
    const float a = static_cast<float>(argb >> 24) * opacity;
    return (static_cast<std::uint32_t>(a) << 24) | (argb & 0x00FFFFFFu);
  }

  // Transparent "KB" toggle button. The launcher owns the floating button's
  // placement; drag/resize it in the launcher's button layout editor.
  void registerKbButton() {
    pl::modmenu::unregisterButton(kKbButtonId);
    const float o = mSettings.opacity;
    (void)pl::modmenu::ButtonBuilder(kKbButtonId, "KB")
        .moduleId(kModuleId)
        .label("KB")
        .behavior(pl::modmenu::ButtonBehavior::Toggle)
        .styleColors(alphaColor(0x80000000u, o), alphaColor(0xA03C7BD9u, o),
                     alphaColor(0x80FFFFFFu, o))
        .textColor(alphaColor(0xFFFFFFFFu, o))
        .activeTextColor(0xFFFFFFFFu)
        .sizeScale(mButtonSize)
        .onEvent(onButtonEvent)
        .registerButton();
  }

  // Hold buttons send KeyEvent ACTION_DOWN on press and ACTION_UP on release,
  // so the game sees a real held key.
  void registerMoveButtons() {
    unregisterMoveButtons();
    const float o = mSettings.opacity;
    for (const auto& k : kMoveKeys) {
      (void)pl::modmenu::ButtonBuilder(k.id, k.label)
          .moduleId(kModuleId)
          .label(k.label)
          .androidKeyCode(k.keyCode)
          .behavior(pl::modmenu::ButtonBehavior::Hold)
          .styleColors(alphaColor(0x80000000u, o), alphaColor(0xC03C7BD9u, o),
                       alphaColor(0x80FFFFFFu, o))
          .textColor(alphaColor(0xFFFFFFFFu, o))
          .activeTextColor(0xFFFFFFFFu)
          .sizeScale(mButtonSize)
          .registerButton();
    }
    mMoveButtonsShown = true;
  }

  void unregisterMoveButtons() {
    for (const auto& k : kMoveKeys) pl::modmenu::unregisterButton(k.id);
    mMoveButtonsShown = false;
  }

  void applyVisibility(bool open) {
    std::lock_guard lock(mLock);
    if (open) registerMoveButtons(); else unregisterMoveButtons();
  }

  static void onKeyboardHidden() {   // Hide key pressed on the keyboard
    instance().applyVisibility(false);
  }

  static void onModuleToggle(std::string_view moduleId, bool enabled) {
    if (moduleId != kModuleId) return;
    kb::setVisible(enabled);
    instance().applyVisibility(enabled);
  }

  static void onButtonEvent(std::string_view buttonId,
                            pl::modmenu::ButtonEvent event, float value) {
    if (buttonId != kKbButtonId ||
        event != pl::modmenu::ButtonEvent::StateChanged) return;
    const bool open = value > 0.5f;
    kb::setVisible(open);
    instance().applyVisibility(open);
  }

  static void onConfigChanged(std::string_view moduleId, std::string_view key,
                              std::string_view value) {
    if (moduleId != kModuleId) return;
    auto& self = instance();
    const float v = std::strtof(std::string(value).c_str(), nullptr);
    bool buttonsChanged = false;
    if (key == "posX")            self.mSettings.xPercent = v;
    else if (key == "posY")       self.mSettings.yPercent = v;
    else if (key == "scale")      self.mSettings.scale = std::max(0.5f, v);
    else if (key == "opacity") {
      self.mSettings.opacity = std::clamp(v / 100.0f, 0.1f, 1.0f);
      buttonsChanged = true;
    } else if (key == "buttonSize") {
      self.mButtonSize = std::clamp(v / 100.0f, 0.5f, 2.0f);
      buttonsChanged = true;
    } else return;

    kb::configure(self.mSettings);
    if (buttonsChanged) {
      std::lock_guard lock(self.mLock);
      self.registerKbButton();
      if (self.mMoveButtonsShown) self.registerMoveButtons();
    }
  }
};

} // namespace

PL_REGISTER_MOD(InGameKeyboardMod, InGameKeyboardMod::instance())
