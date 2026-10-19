/**
 * @file InGameKeyboardMod.cpp
 * @brief Full-featured In-Game Virtual Keyboard Mod for Minecraft Bedrock (Android)
 * @author Kirisuniruu
 */

#include <pl/Mod.hpp>
#include <imgui.h>

// ==========================================
// Mod State & Configuration Variables
// ==========================================
static bool g_showEverywhere = true;
static bool g_showKeyboard = false;

// UI Layout & Customization State
static ImVec2 g_btnPos = ImVec2(50.0f, 100.0f);
static float g_btnSize = 50.0f;
static float g_opacity = 0.85f;
static float g_kbScale = 1.0f;

// ==========================================
// Helper Functions
// ==========================================
static void RenderTypeKey(const char* label, char character, float width = 38.0f) {
    ImVec2 keySize = ImVec2(width * g_kbScale, 38.0f * g_kbScale);
    if (ImGui::Button(label, keySize)) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharacter(character);
    }
}

// ==========================================
// Main UI Rendering Pipeline
// ==========================================
static void RenderInGameKeyboardUI() {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_opacity);

    ImGuiWindowFlags commonFlags = ImGuiWindowFlags_NoCollapse | 
                                   ImGuiWindowFlags_AlwaysAutoResize;

    // 1. Floating "KB" Toggle Button Window
    if (g_showEverywhere) {
        ImGui::SetNextWindowPos(g_btnPos, ImGuiCond_FirstUseEver);
        
        ImGuiWindowFlags btnFlags = commonFlags | ImGuiWindowFlags_NoTitleBar |
                                                  ImGuiWindowFlags_NoResize |
                                                  ImGuiWindowFlags_NoScrollbar;

        if (ImGui::Begin("##KBToggle", nullptr, btnFlags)) {
            g_btnPos = ImGui::GetWindowPos();

            if (ImGui::Button("KB", ImVec2(g_btnSize, g_btnSize))) {
                g_showKeyboard = !g_showKeyboard;
            }
        }
        ImGui::End();
    }

    // 2. Full Typable Virtual Keyboard Interface
    if (g_showKeyboard) {
        ImGui::SetNextWindowSize(ImVec2(480.0f * g_kbScale, 280.0f * g_kbScale), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Virtual Keyboard", &g_showKeyboard, commonFlags)) {
            
            // Settings Menu (Show Everywhere lives here along with Size & Opacity)
            if (ImGui::CollapsingHeader("Settings")) {
                ImGui::Checkbox("Show Everywhere (Menus, Settings, Chat)", &g_showEverywhere);
                ImGui::SliderFloat("Opacity", &g_opacity, 0.2f, 1.0f);
                ImGui::SliderFloat("Button Size", &g_btnSize, 35.0f, 90.0f);
                ImGui::SliderFloat("Keyboard Scale", &g_kbScale, 0.6f, 1.8f);
            }

            ImGui::Separator();

            // QWERTY Row 1
            const char* row1Str = "qwertyuiop";
            for (int i = 0; i < 10; ++i) {
                char label[2] = { row1Str[i], '\0' };
                RenderTypeKey(label, row1Str[i]);
                if (i < 9) ImGui::SameLine();
            }

            // QWERTY Row 2
            const char* row2Str = "asdfghjkl";
            for (int i = 0; i < 9; ++i) {
                char label[2] = { row2Str[i], '\0' };
                RenderTypeKey(label, row2Str[i]);
                if (i < 8) ImGui::SameLine();
            }

            // QWERTY Row 3
            const char* row3Str = "zxcvbnm";
            for (int i = 0; i < 7; ++i) {
                char label[2] = { row3Str[i], '\0' };
                RenderTypeKey(label, row3Str[i]);
                if (i < 6) ImGui::SameLine();
            }

            ImGui::Separator();

            // Spacebar & Backspace
            if (ImGui::Button("Space", ImVec2(200.0f * g_kbScale, 36.0f * g_kbScale))) {
                ImGui::GetIO().AddInputCharacter(' ');
            }
            ImGui::SameLine();
            if (ImGui::Button("Back", ImVec2(90.0f * g_kbScale, 36.0f * g_kbScale))) {
                ImGuiIO& io = ImGui::GetIO();
                io.AddKeyEvent(ImGuiKey_Backspace, true);
                io.AddKeyEvent(ImGuiKey_Backspace, false);
            }
        }
        ImGui::End();
    }

    ImGui::PopStyleVar();
}

// ==========================================
// Mod Initialization Hook
// ==========================================
MOD_AUTHOR("Kirisuniruu");
MOD_NAME("InGameKeyboard");
MOD_VERSION("2.6.0");

void PreloaderModInit() {
    Mod::registerImGuiRender([]() {
        RenderInGameKeyboardUI();
    });
}
