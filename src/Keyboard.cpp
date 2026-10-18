#include <pl/gui/imgui.h>

// Global state variables for our features
static bool g_showKeyboard = false;
static ImVec2 g_btnPos = ImVec2(50, 100); // Starting position for the KB toggle button
static float g_btnSize = 50.0f;           // KB button size
static float g_opacity = 0.85f;           // Transparency / Opacity (0.2 to 1.0)
static float g_kbScale = 1.0f;            // Keyboard window scaling

// Helper function for PC-like holdable keys (e.g., W, A, S, D)
void RenderHoldableKey(const char* label, int keyCode) {
    ImGui::Button(label, ImVec2(45 * g_kbScale, 45 * g_kbScale));

    // Detects when your finger is actively holding the button down
    if (ImGui::IsItemActive()) {
        // Send Key DOWN event continuously while holding
        // (Add your game input injection function here)
    } 
    else if (ImGui::IsItemDeactivated()) {
        // Send Key UP event when you lift your finger
        // (Add your key release function here)
    }
}

void RenderInGameKeyboardUI() {
    // Apply global opacity/alpha styling to the mod interface
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_opacity);

    // -------------------------------------------------------------
    // 1. MOVABLE & TRANSPARENT 'KB' TOGGLE BUTTON (Visible Everywhere)
    // -------------------------------------------------------------
    ImGui::SetNextWindowPos(g_btnPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(g_btnSize + 15, g_btnSize + 15));

    ImGuiWindowFlags btnFlags = ImGuiWindowFlags_NoTitleBar |
                                ImGuiWindowFlags_NoResize |
                                ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_AlwaysAutoResize;

    if (ImGui::Begin("##KBToggle", nullptr, btnFlags)) {
        // Automatically save position when you drag the button around the screen
        g_btnPos = ImGui::GetWindowPos();

        if (ImGui::Button("KB", ImVec2(g_btnSize, g_btnSize))) {
            g_showKeyboard = !g_showKeyboard;
        }
    }
    ImGui::End();

    // -------------------------------------------------------------
    // 2. VIRTUAL KEYBOARD WINDOW & SETTINGS
    // -------------------------------------------------------------
    if (g_showKeyboard) {
        ImGui::SetNextWindowSize(ImVec2(480 * g_kbScale, 240 * g_kbScale), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("In-Game Keyboard", &g_showKeyboard)) {
            
            // Customization Menu
            if (ImGui::CollapsingHeader("Customization Settings")) {
                ImGui::SliderFloat("Opacity", &g_opacity, 0.2f, 1.0f);
                ImGui::SliderFloat("Button Size", &g_btnSize, 35.0f, 90.0f);
                ImGui::SliderFloat("Keyboard Scale", &g_kbScale, 0.6f, 1.8f);
            }

            ImGui::Separator();

            // Movement Keys (PC-like continuous holding)
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
