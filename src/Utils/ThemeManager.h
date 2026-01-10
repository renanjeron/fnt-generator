#pragma once
#include <string>
#include <vector>
#include "imgui.h"

namespace Utils {

    struct ThreadTheme {
        std::string Name;
    };

    struct EffectThemeColors {
        ImVec4 Header;
        ImVec4 HeaderHovered;
        ImVec4 HeaderActive;
        ImVec4 Text;
        ImVec4 CheckMark;
    };

    class ThemeManager {
    public:
        static void ApplyTheme(const std::string& name);
        static std::vector<std::string> GetAvailableThemes();
        static std::string GetCurrentThemeName();
        static EffectThemeColors GetEffectColors(bool enabled);
        
    private:
        static void ApplyDarkTheme();
        static void ApplyLightTheme();
        static void ApplyClassicTheme();
        static void ApplyDeepDarkTheme();
        static void ApplyCherryTheme();
        static void ApplySpectrumTheme();
        static void ApplySpectrumLightTheme();
        static void ApplyLedSynthTheme();
        static void ApplyComfortableDarkCyanTheme();
        static void ApplyComfortableLightOrangeTheme();


    };
}
