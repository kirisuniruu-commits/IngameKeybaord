/**
 * @file InGameKeyboardMod.cpp
 * @brief Full-featured In-Game Virtual Keyboard Mod for Minecraft Bedrock (Android)
 * @author Kirisuniruu
 */

#include <pl/Mod.hpp>
#include <imgui/imgui.h>

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

/**
 * Renders an individual virtual keyboard key and sends character input 
 * directly to the game's active text stream (like chat boxes or text inputs).
 */
static void RenderTypeKey(const char* label, char character, float customWidth = 38.0f) {
    ImVec2 keySize = ImVec2(customWidth * g_kbScale, 38.0f * g_kbScale);
    if (ImGui::Button(label, keySize)) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharacter(character);
    }
}

// ==========================================
// Main UI Rendering Pipeline
// ==========================================
static void RenderInGameKeyboardUI() {
    // Apply global opacity styling across all mod windows
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_opacity);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | 
                                   ImGuiWindowFlags_AlwaysAutoResize;

    // ------------------------------------------
    // 1. Floating "KB" Toggle Button Window
    // ------------------------------------------
    if (g_showEverywhere) {
        ImGui::SetNextWindowPos(g_btnPos, ImGuiCond_FirstUseEver);
        
        ImGuiWindowFlags toggleFlags = windowFlags | ImGuiWindowFlags_NoTitleBar |
                                                     ImGuiWindowFlags_NoResize |
                                                     ImGuiWindowFlags_NoScrollbar;

        if (ImGui::Begin("##KBToggleWindow", nullptr, toggleFlags)) {
            // Track window position if dragged by the user
            g_btnPos = ImGui::GetWindowPos();

            if (ImGui::Button("KB", ImVec2(g_btnSize, g_btnSize))) {
                g_showKeyboard = !g_showKeyboard;
            }
        }
        ImGui::End();
    }

    // ------------------------------------------
    // 2. Full QWERTY Virtual Keyboard Window
    // ------------------------------------------
    if (g_showKeyboard) {
        ImGui::SetNextWindowSize(ImVec2(480.0f * g_kbScale, 280.0f * g_kbScale), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("In-Game Virtual Keyboard", &g_showKeyboard, windowFlags)) {
            
            // Collapsible Customization Settings Section
            if (ImGui::CollapsingHeader("Keyboard Settings")) {
                ImGui::Checkbox("Show Everywhere (Menus, Settings, Chat)", &g_showEverywhere);
                ImGui::SliderFloat("Opacity", &g_opacity, 0.2f, 1.0f, "%.2f");
                ImGui::SliderFloat("Button Size", &g_btnSize, 35.0f, 90.0f, "%.0f px");
                ImGui::SliderFloat("Keyboard Scale", &g_kbScale, 0.6f, 1.8f, "%.2f x");
            }

            ImGui::Separator();

            // Row 1: Q W E R T Y U I O P
            const char* row1 = "qwertyuiop";
            for (int i = 0; i < 10; ++i) {
                char lbl[2] = { row1[i], '\0' };
                RenderTypeKey(lbl, row1[i]);
                if (i < 9) ImGui::SameLine();
            }

            // Row 2: A S D F G H J K L
            const char* row2 = "asdfghjkl";
            for (int i = 0; i < 9; ++i) {
                char lbl[2] = { row2[i], '\0' };
                RenderTypeKey(lbl, row2[i]);
                if (i < 8) ImGui::SameLine();
            }

            // Row 3: Z X C V B N M
            const char* row3 = "zxcvbnm";
            for (int i = 0; i < 7; ++i) {
                char lbl[2] = { row3[i], '\0' };
                RenderTypeKey(lbl, row3[i]);
                if (i < 6) ImGui::SameLine();
            }

            ImGui::Separator();

            // Bottom Control Row (Spacebar and Backspace)
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
// Mod Registration Hooks
// ==========================================
MOD_AUTHOR("Kirisuniruu");
MOD_NAME("InGameKeyboard");
MOD_VERSION("2.0.0");

void PreloaderModInit() {
    Mod::registerImGuiRender([]() {
        RenderInGameKeyboardUI();
    });
}
