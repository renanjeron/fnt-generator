#include "PreferencesPopup.h"
#include "imgui.h"
#include <string>
#include <vector>
#include "../Utils/ThemeManager.h"
#include "../Utils/StyleUtils.h" // For future use if needed
#include "../Utils/SettingsManager.h"

namespace UI {

    void PreferencesPopup::Show(bool* open, int* ssaaFactor, bool* showFontPreview, bool* showRecentError) {
        if (!*open) return;

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("PreferencesPopup");

        if (ImGui::BeginPopupModal("PreferencesPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Application Preferences");
            ImGui::Separator();

             ImGui::Text("Theme");
            
            static std::vector<std::string> themes = Utils::ThemeManager::GetAvailableThemes();
            static std::string currentTheme = Utils::ThemeManager::GetCurrentThemeName();
            
            // Find current index
            int currentThemeIdx = 0;
            for(size_t i=0; i<themes.size(); i++) {
                if(themes[i] == currentTheme) {
                    currentThemeIdx = (int)i;
                    break;
                }
            }
            
            if (ImGui::Combo("Select Theme", &currentThemeIdx, [](void* data, int idx, const char** out_text) {
                auto* vec = (std::vector<std::string>*)data;
                if (idx < 0 || idx >= (int)vec->size()) return false;
                *out_text = (*vec)[idx].c_str();
                return true;
            }, (void*)&themes, (int)themes.size())) 
            {
                currentTheme = themes[currentThemeIdx];
                Utils::ThemeManager::ApplyTheme(currentTheme);
                Utils::SettingsManager::Get().SetTheme(currentTheme);
            }

            ImGui::Separator();
            
            // Texture Quality / SSAA
            const char* ssaaOptions[] = { "Standard (1x)", "High Quality (2x)", "Ultra Quality (4x)" };
            int ssaaIdx = 0;
            if (*ssaaFactor == 2) ssaaIdx = 1;
            else if (*ssaaFactor == 4) ssaaIdx = 2;

            if (ImGui::Combo("Default Preview Quality", &ssaaIdx, ssaaOptions, IM_ARRAYSIZE(ssaaOptions))) {
                if (ssaaIdx == 0) *ssaaFactor = 1;
                else if (ssaaIdx == 1) *ssaaFactor = 2;
                else if (ssaaIdx == 2) *ssaaFactor = 4;
            }
            
            // Font Preview
            if (showFontPreview) {
                 ImGui::Checkbox("Show Font Previews in List", showFontPreview);
            }

           
    
            
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                *open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

}
