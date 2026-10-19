/**
 * @file InGameKeyboardMod.cpp
 * @brief In-game virtual keyboard for Minecraft Bedrock (Android) via preloader-android.
 *
 * UI:
 *   - Movable, semi-transparent floating "KB" button (drag to move, tap to toggle).
 *   - Full QWERTY keyboard window (digits, letters, Shift, Space, Backspace, Enter).
 *   - Settings: Show Everywhere, opacity, button size, keyboard scale.
 *
 * Input is delivered through ImGuiIO (AddInputCharacter / AddKeyEvent).
 */

#include <pl/Mod.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstring>

// ============================================================================
// State
// ============================================================================
namespace {

struct State {
    // Settings
    bool  showEverywhere = true;   // ON: button always visible.
                                   // OFF: button only while a text field wants input
                                   //      (or while the keyboard is open).
    float opacity        = 0.85f;  // 0.2 .. 1.0
    float buttonSize     = 56.0f;  // 35 .. 90 px
    float kbScale        = 1.0f;   // 0.6 .. 1.8

    // Runtime
    bool   keyboardOpen = false;
    bool   shift        = false;
    ImVec2 buttonPos    = ImVec2(40.0f, 120.0f);
    bool   dragMoved    = false;
    bool   posInit      = false;

    // Keys that must be released a few frames after being pressed.
    int backspaceHold = 0;
    int enterHold     = 0;
};

State g;

constexpr float kKeyUnit    = 40.0f;   // base key size in px (before scale)
constexpr float kKeySpacing = 4.0f;
constexpr float kDragThresh = 6.0f;

// ----------------------------------------------------------------------------
// Key helpers
// ----------------------------------------------------------------------------
void ReleaseHeldKeys(ImGuiIO& io) {
    if (g.backspaceHold > 0 && --g.backspaceHold == 0)
        io.AddKeyEvent(ImGuiKey_Backspace, false);
    if (g.enterHold > 0 && --g.enterHold == 0)
        io.AddKeyEvent(ImGuiKey_Enter, false);
}

float KeyW(float weight, float scale) {
    return (kKeyUnit * weight + kKeySpacing * (weight - 1.0f)) * scale;
}

bool Key(const char* label, float weight = 1.0f, bool highlight = false) {
    const float s = g.kbScale;
    if (highlight)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    const bool clicked = ImGui::Button(label, ImVec2(KeyW(weight, s), kKeyUnit * s));
    if (highlight) ImGui::PopStyleColor();
    return clicked;
}

void TypeChar(char c) {
    ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(static_cast<unsigned char>(c)));
}

// Row of plain character keys. `shifted` supplies the shift-layer characters
// (same length as `normal`); letters are upper-cased automatically.
void CharRow(const char* normal, const char* shifted = nullptr) {
    const size_t n = std::strlen(normal);
    for (size_t i = 0; i < n; ++i) {
        char c = normal[i];
        if (g.shift) {
            if (shifted)                    c = shifted[i];
            else if (c >= 'a' && c <= 'z')  c = static_cast<char>(c - 'a' + 'A');
        }
        const char label[2] = { c, '\0' };
        ImGui::PushID(static_cast<int>(i));
        if (Key(label)) {
            TypeChar(c);
            g.shift = false;               // one-shot shift
        }
        ImGui::PopID();
        if (i + 1 < n) ImGui::SameLine(0.0f, kKeySpacing * g.kbScale);
    }
}

// ----------------------------------------------------------------------------
// Settings block
// ----------------------------------------------------------------------------
void RenderSettings() {
    ImGui::Checkbox("Show Everywhere", &g.showEverywhere);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("ON: KB button always visible.\nOFF: only visible while a text field is active.");
    ImGui::SliderFloat("Opacity",         &g.opacity,    0.2f,  1.0f,  "%.2f");
    ImGui::SliderFloat("Button Size",     &g.buttonSize, 35.0f, 90.0f, "%.0f px");
    ImGui::SliderFloat("Keyboard Scale",  &g.kbScale,    0.6f,  1.8f,  "%.2f");
}

// ----------------------------------------------------------------------------
// Floating toggle button
// ----------------------------------------------------------------------------
void RenderToggleButton(const ImGuiIO& io) {
    const float sz = g.buttonSize;

    // Keep the button on screen.
    g.buttonPos.x = std::clamp(g.buttonPos.x, 0.0f, std::max(0.0f, io.DisplaySize.x - sz));
    g.buttonPos.y = std::clamp(g.buttonPos.y, 0.0f, std::max(0.0f, io.DisplaySize.y - sz));

    ImGui::SetNextWindowPos(g.buttonPos, ImGuiCond_Always);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, sz * 0.25f);

    if (ImGui::Begin("##InGameKeyboardToggle", nullptr, flags)) {
        const bool clicked = ImGui::Button("KB", ImVec2(sz, sz));

        // ImGui buttons swallow the press, so implement drag-to-move manually.
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, kDragThresh)) {
            g.buttonPos.x += io.MouseDelta.x;
            g.buttonPos.y += io.MouseDelta.y;
            g.dragMoved = true;
        }
        if (clicked) {
            if (!g.dragMoved) g.keyboardOpen = !g.keyboardOpen;
            g.dragMoved = false;
        } else if (!ImGui::IsItemActive()) {
            g.dragMoved = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

// ----------------------------------------------------------------------------
// Keyboard window
// ----------------------------------------------------------------------------
void RenderKeyboard(ImGuiIO& io) {
    const float s = g.kbScale;

    if (!g.posInit) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 10.0f),
                                ImGuiCond_FirstUseEver, ImVec2(0.5f, 1.0f));
        g.posInit = true;
    }

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kKeySpacing * s, kKeySpacing * s));

    if (ImGui::Begin("Virtual Keyboard", &g.keyboardOpen, flags)) {
        ImGui::SetWindowFontScale(s);

        if (ImGui::CollapsingHeader("Settings")) {
            RenderSettings();
            ImGui::Separator();
        }

        // Number row
        CharRow("1234567890", "!@#$%^&*()");

        // Letter rows (rows 2 and 3 are indented like a real keyboard)
        CharRow("qwertyuiop");

        ImGui::Indent(KeyW(0.5f, s));
        CharRow("asdfghjkl");
        ImGui::Unindent(KeyW(0.5f, s));

        // Shift + zxcvbnm + Backspace
        if (Key("Shift", 1.5f, g.shift)) g.shift = !g.shift;
        ImGui::SameLine(0.0f, kKeySpacing * s);
        CharRow("zxcvbnm");
        ImGui::SameLine(0.0f, kKeySpacing * s);
        if (Key("Back", 1.5f)) {
            io.AddKeyEvent(ImGuiKey_Backspace, true);
            g.backspaceHold = 2;           // release two frames later
        }

        // Bottom row: , Space . Enter
        if (Key(g.shift ? "<" : ",", 1.0f)) { TypeChar(g.shift ? '<' : ','); g.shift = false; }
        ImGui::SameLine(0.0f, kKeySpacing * s);
        if (Key("Space", 5.0f))             TypeChar(' ');
        ImGui::SameLine(0.0f, kKeySpacing * s);
        if (Key(g.shift ? ">" : ".", 1.0f)) { TypeChar(g.shift ? '>' : '.'); g.shift = false; }
        ImGui::SameLine(0.0f, kKeySpacing * s);
        if (Key("Enter", 1.5f)) {
            io.AddKeyEvent(ImGuiKey_Enter, true);
            g.enterHold = 2;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

// ----------------------------------------------------------------------------
// Per-frame entry point
// ----------------------------------------------------------------------------
void RenderInGameKeyboardUI() {
    ImGuiIO& io = ImGui::GetIO();
    ReleaseHeldKeys(io);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(g.opacity, 0.2f, 1.0f));

    // Button is always reachable when the keyboard is open, so the user can
    // never lock themselves out by turning "Show Everywhere" off.
    if (g.showEverywhere || g.keyboardOpen || io.WantTextInput)
        RenderToggleButton(io);

    if (g.keyboardOpen)
        RenderKeyboard(io);

    ImGui::PopStyleVar();
}

} // namespace

// ============================================================================
// Mod entry point
// ----------------------------------------------------------------------------
// NOTE: `pl::Mod::registerImGuiRender` and the `PreloaderModInit` symbol name
// are taken from the original source and could not be verified against the
// preloader-android headers in the authoring environment. If your checkout
// names them differently, this is the ONLY place that needs to change.
// ============================================================================
extern "C" __attribute__((visibility("default"))) void PreloaderModInit() {
    pl::Mod::registerImGuiRender([]() { RenderInGameKeyboardUI(); });
}
