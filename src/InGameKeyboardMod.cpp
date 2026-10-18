#include <Mod.hpp>

// Mod UI State
static bool g_showKeyboard = false;
static ImVec2 g_btnPos = ImVec2(50, 100);
static float g_btnSize = 50.0f;
static float g_opacity = 0.85f;
static float g_kbScale = 1.0f;

// PC-like holdable key helper
void RenderHoldableKey(const char* label, int keyCode) {
    ImGui::Button(label, ImVec2(45 * g_kbScale, 45 * g_kbScale));

    if (ImGui::IsItemActive()) {
        // Active hold state (holding down the virtual key)
    } 
    else if (ImGui::IsItemDeactivated()) {
        // Released state
    }
}

// Main UI Rendering function
void RenderInGameKeyboardUI() {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_opacity);

    // 1. Movable & Transparent Toggle Button (Visible Everywhere)
    ImGui::SetNextWindowPos(g_btnPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(g_btnSize + 15, g_btnSize + 15));

    ImGuiWindowFlags btnFlags = ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("##KBToggle", nullptr, btnFlags)) {
        g_btnPos = ImGui::GetWindowPos();

        if (ImGui::Button("KB", ImVec2(g_btnSize, g_btnSize))) {
            g_showKeyboard = !g_showKeyboard;
        }
    }
    ImGui::End();

    // 2. Virtual Keyboard Interface Window
    if (g_showKeyboard) {
        ImGui::SetNextWindowSize(ImVec2(480 * g_kbScale, 240 * g_kbScale), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("In-Game Keyboard", &g_showKeyboard)) {
            
            if (ImGui::CollapsingHeader("Customization Settings")) {
                ImGui::SliderFloat("Opacity", &g_opacity, 0.2f, 1.0f);
                ImGui::SliderFloat("Button Size", &g_btnSize, 35.0f, 90.0f);
                ImGui::SliderFloat("Keyboard Scale", &g_kbScale, 0.6f, 1.8f);
            }

            ImGui::Separator();

            // Movement Keys Row
            RenderHoldableKey("W", 17);
            ImGui::SameLine();
            RenderHoldableKey("A", 30);
            ImGui::SameLine();
            RenderHoldableKey("S", 31);
            ImGui::SameLine();
            RenderHoldableKey("D", 32);
        }
        ImGui::End();
    }

    ImGui::PopStyleVar();
}

// Mod Initialization Hook
MOD_AUTHOR("Kirisuniruu");
MOD_NAME("InGameKeyboard");
MOD_VERSION("1.0.0");

void PreloaderModInit() {
    Mod::registerImGuiRender([]() {
        RenderInGameKeyboardUI();
    });
}

