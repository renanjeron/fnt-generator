#include "ThemeManager.h"
#include "imgui.h"
#include "imgui_internal.h" // For ColorConvertU32ToFloat4 if needed (It is in imgui.h usually)

namespace Utils {

    static std::string s_CurrentTheme = "Dark";

    std::vector<std::string> ThemeManager::GetAvailableThemes() {
        return { "Dark", "Light", "Classic", "Adobe Spectrum", "Adobe Spectrum Light", "Deep Dark", "Cherry", "LedSynth", "Comfortable Dark Cyan", "Comfortable Light Orange" };
    }

    std::string ThemeManager::GetCurrentThemeName() {
        return s_CurrentTheme;
    }

    void ThemeManager::ApplyTheme(const std::string& name) {
        // Reset to default style to avoid property persistence from previous themes
        ImGui::GetStyle() = ImGuiStyle();

        s_CurrentTheme = name;
        if (name == "Light") ApplyLightTheme();
        else if (name == "Classic") ApplyClassicTheme();
        else if (name == "Deep Dark") ApplyDeepDarkTheme();
        else if (name == "Cherry") ApplyCherryTheme();
        else if (name == "Adobe Spectrum") ApplySpectrumTheme();
        else if (name == "Adobe Spectrum Light") ApplySpectrumLightTheme();
        else if (name == "LedSynth") ApplyLedSynthTheme();
        else if (name == "Comfortable Dark Cyan") ApplyComfortableDarkCyanTheme();
        else if (name == "Comfortable Light Orange") ApplyComfortableLightOrangeTheme();
        else ApplyDarkTheme(); // Default
    }

    void ThemeManager::ApplyDarkTheme() {
        ImGui::StyleColorsDark();
    }

    void ThemeManager::ApplyLightTheme() {
        ImGui::StyleColorsLight();
    }

    void ThemeManager::ApplyClassicTheme() {
        ImGui::StyleColorsClassic();
    }

    // --- Spectrum Helper ---
    namespace Spectrum {
        inline unsigned int Color(unsigned int c) {
            const short a = 0xFF;
            const short r = (c >> 16) & 0xFF;
            const short g = (c >> 8) & 0xFF;
            const short b = (c >> 0) & 0xFF;
            return (a << 24) | (r << 0) | (g << 8) | (b << 16);
        }
        // Dark Theme Colors
        const unsigned int GRAY50 = Color(0x252525);
        const unsigned int GRAY75 = Color(0x2F2F2F);
        const unsigned int GRAY100 = Color(0x323232);
        const unsigned int GRAY200 = Color(0x393939);
        const unsigned int GRAY300 = Color(0x3E3E3E);
        const unsigned int GRAY400 = Color(0x4D4D4D);
        const unsigned int GRAY500 = Color(0x5C5C5C);
        const unsigned int GRAY600 = Color(0x7B7B7B);
        const unsigned int GRAY700 = Color(0x999999);
        const unsigned int GRAY800 = Color(0xCDCDCD);
        const unsigned int GRAY900 = Color(0xFFFFFF);
        const unsigned int BLUE400 = Color(0x2680EB);
        const unsigned int BLUE500 = Color(0x378EF0);
        const unsigned int BLUE600 = Color(0x4B9CF5);
        const unsigned int BLUE700 = Color(0x5AA9FA);
        
        namespace Light {
            const unsigned int GRAY50 = Color(0xFFFFFF);
            const unsigned int GRAY75 = Color(0xFAFAFA);
            const unsigned int GRAY100 = Color(0xF5F5F5);
            const unsigned int GRAY200 = Color(0xEAEAEA);
            const unsigned int GRAY300 = Color(0xE1E1E1);
            const unsigned int GRAY400 = Color(0xCACACA);
            const unsigned int GRAY500 = Color(0xB3B3B3);
            const unsigned int GRAY600 = Color(0x8E8E8E);
            const unsigned int GRAY700 = Color(0x707070);
            const unsigned int GRAY800 = Color(0x4B4B4B);
            const unsigned int GRAY900 = Color(0x2C2C2C);
            const unsigned int BLUE400 = Color(0x2680EB);
            const unsigned int BLUE500 = Color(0x1473E6);
            const unsigned int BLUE600 = Color(0x0D66D0);
            const unsigned int BLUE700 = Color(0x095ABA);
        }
    }

    void ThemeManager::ApplySpectrumTheme() {
        ImGuiStyle* style = &ImGui::GetStyle();
        style->GrabRounding = 4.0f;
        
        ImVec4* colors = style->Colors;
        colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY800); 
        colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY500);
        colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY100);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY50); 
        colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY300);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); 
        colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY75);
        colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY50);
        colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY200);
        colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY300);
        colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY200);
        colors[ImGuiCol_TitleBgCollapsed] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY400);
        colors[ImGuiCol_MenuBarBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY100);
        colors[ImGuiCol_ScrollbarBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY100);
        colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY400);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY600);
        colors[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY700);
        colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE500);
        colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY700);
        colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY800);
        colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY75);
        colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY50);
        colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY200);
        colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE400);
        colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE500);
        colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE600);
        colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY400);
        colors[ImGuiCol_SeparatorHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY600);
        colors[ImGuiCol_SeparatorActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY700);
        colors[ImGuiCol_ResizeGrip] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY400);
        colors[ImGuiCol_ResizeGripHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY600);
        colors[ImGuiCol_ResizeGripActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::GRAY700);
        colors[ImGuiCol_PlotLines] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE400);
        colors[ImGuiCol_PlotLinesHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE600);
        colors[ImGuiCol_PlotHistogram] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE400);
        colors[ImGuiCol_PlotHistogramHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::BLUE600);
        colors[ImGuiCol_TextSelectedBg] = ImGui::ColorConvertU32ToFloat4((Spectrum::BLUE400 & 0x00FFFFFF) | 0x33000000);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImGui::ColorConvertU32ToFloat4((Spectrum::GRAY900 & 0x00FFFFFF) | 0x0A000000);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    }

    void ThemeManager::ApplyLedSynthTheme() {
        ImGuiStyle* style = &ImGui::GetStyle();
        
        style->WindowPadding            = ImVec2(15, 15);
        style->WindowRounding           = 5.0f;
        style->FramePadding             = ImVec2(5, 5);
        style->FrameRounding            = 4.0f;
        style->ItemSpacing              = ImVec2(12, 8);
        style->ItemInnerSpacing         = ImVec2(8, 6);
        style->IndentSpacing            = 25.0f;
        style->ScrollbarSize            = 15.0f;
        style->ScrollbarRounding        = 9.0f;
        style->GrabMinSize              = 5.0f;
        style->GrabRounding             = 3.0f;

        style->Colors[ImGuiCol_Text]                  = ImVec4(0.40f, 0.39f, 0.38f, 1.00f);
        style->Colors[ImGuiCol_TextDisabled]          = ImVec4(0.40f, 0.39f, 0.38f, 0.77f);
        style->Colors[ImGuiCol_WindowBg]              = ImVec4(0.92f, 0.91f, 0.88f, 0.70f);
        style->Colors[ImGuiCol_ChildBg]               = ImVec4(1.00f, 0.98f, 0.95f, 0.58f);
        style->Colors[ImGuiCol_PopupBg]               = ImVec4(0.92f, 0.91f, 0.88f, 0.92f);
        style->Colors[ImGuiCol_Border]                = ImVec4(0.84f, 0.83f, 0.80f, 0.65f);
        style->Colors[ImGuiCol_BorderShadow]          = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
        style->Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);
        style->Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.99f, 1.00f, 0.40f, 0.78f);
        style->Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_TitleBg]               = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);
        style->Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
        style->Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_MenuBarBg]             = ImVec4(1.00f, 0.98f, 0.95f, 0.47f);
        style->Colors[ImGuiCol_ScrollbarBg]           = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);
        style->Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.00f, 0.00f, 0.00f, 0.21f);
        style->Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.90f, 0.91f, 0.00f, 0.78f);
        style->Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        // style->Colors[ImGuiCol_ComboBg]               = ImVec4(1.00f, 0.98f, 0.95f, 1.00f); // Map to PopupBg
        style->Colors[ImGuiCol_CheckMark]             = ImVec4(0.25f, 1.00f, 0.00f, 0.80f);
        style->Colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.00f, 0.00f, 0.14f);
        style->Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_Button]                = ImVec4(0.00f, 0.00f, 0.00f, 0.14f);
        style->Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.99f, 1.00f, 0.22f, 0.86f);
        style->Colors[ImGuiCol_ButtonActive]          = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_Header]                = ImVec4(0.25f, 1.00f, 0.00f, 0.76f);
        style->Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.25f, 1.00f, 0.00f, 0.86f);
        style->Colors[ImGuiCol_HeaderActive]          = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_Separator]                = ImVec4(0.00f, 0.00f, 0.00f, 0.32f);
        style->Colors[ImGuiCol_SeparatorHovered]         = ImVec4(0.25f, 1.00f, 0.00f, 0.78f);
        style->Colors[ImGuiCol_SeparatorActive]          = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.04f);
        style->Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.25f, 1.00f, 0.00f, 0.78f);
        style->Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        // style->Colors[ImGuiCol_CloseButton]           = ImVec4(0.40f, 0.39f, 0.38f, 0.16f);
        // style->Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.40f, 0.39f, 0.38f, 0.39f);
        // style->Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.40f, 0.39f, 0.38f, 1.00f);
        style->Colors[ImGuiCol_PlotLines]             = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
        style->Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
        style->Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
        style->Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
        style->Colors[ImGuiCol_ModalWindowDimBg]  = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
    }

    void ThemeManager::ApplySpectrumLightTheme() {
        ImGuiStyle* style = &ImGui::GetStyle();
        style->GrabRounding = 4.0f;
        
        ImVec4* colors = style->Colors;
        colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY800); 
        colors[ImGuiCol_TextDisabled] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY500);
        colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY100);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY50); 
        colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY300);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); 
        colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY75);
        colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY50);
        colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY200);
        colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY300);
        colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY200);
        colors[ImGuiCol_TitleBgCollapsed] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY400);
        colors[ImGuiCol_MenuBarBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY100);
        colors[ImGuiCol_ScrollbarBg] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY100);
        colors[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY400);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY600);
        colors[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY700);
        colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE500);
        colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY700);
        colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY800);
        colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY75);
        colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY50);
        colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY200);
        colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE400);
        colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE500);
        colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE600);
        colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY400);
        colors[ImGuiCol_SeparatorHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY600);
        colors[ImGuiCol_SeparatorActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY700);
        colors[ImGuiCol_ResizeGrip] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY400);
        colors[ImGuiCol_ResizeGripHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY600);
        colors[ImGuiCol_ResizeGripActive] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::GRAY700);
        colors[ImGuiCol_PlotLines] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE400);
        colors[ImGuiCol_PlotLinesHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE600);
        colors[ImGuiCol_PlotHistogram] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE400);
        colors[ImGuiCol_PlotHistogramHovered] = ImGui::ColorConvertU32ToFloat4(Spectrum::Light::BLUE600);
        colors[ImGuiCol_TextSelectedBg] = ImGui::ColorConvertU32ToFloat4((Spectrum::Light::BLUE400 & 0x00FFFFFF) | 0x33000000);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImGui::ColorConvertU32ToFloat4((Spectrum::Light::GRAY900 & 0x00FFFFFF) | 0x0A000000);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
    }
 
    void ThemeManager::ApplyDeepDarkTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
        colors[ImGuiCol_Border]                 = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_Button]                 = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
        colors[ImGuiCol_Separator]              = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

        style.WindowPadding     = ImVec2(8.00f, 8.00f);
        style.FramePadding      = ImVec2(5.00f, 2.00f);
        style.CellPadding       = ImVec2(6.00f, 6.00f);
        style.ItemSpacing       = ImVec2(6.00f, 6.00f);
        style.ItemInnerSpacing  = ImVec2(6.00f, 6.00f);
        style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
        style.IndentSpacing     = 25;
        style.ScrollbarSize     = 15;
        style.GrabMinSize       = 10;
        style.WindowBorderSize  = 1;
        style.ChildBorderSize   = 1;
        style.PopupBorderSize   = 1;
        style.FrameBorderSize   = 1;
        style.TabBorderSize     = 1;
        style.WindowRounding    = 7;
        style.ChildRounding     = 4;
        style.FrameRounding     = 3;
        style.PopupRounding     = 4;
        style.ScrollbarRounding = 9;
        style.GrabRounding      = 3;
        style.LogSliderDeadzone = 4;
        style.TabRounding       = 4;
    }

    void ThemeManager::ApplyCherryTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text]                   = ImVec4(0.860f, 0.930f, 0.890f, 0.78f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.860f, 0.930f, 0.890f, 0.28f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.13f, 0.14f, 0.17f, 1.00f);
        colors[ImGuiCol_Border]                 = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.71f, 0.22f, 0.27f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.92f, 0.18f, 0.29f, 0.76f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.92f, 0.18f, 0.29f, 0.86f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.92f, 0.18f, 0.29f, 0.78f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.92f, 0.18f, 0.29f, 0.80f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.92f, 0.18f, 0.29f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        style.WindowPadding = ImVec2(8.00f, 8.00f);
        style.FramePadding = ImVec2(5.00f, 2.00f);
        style.CellPadding = ImVec2(6.00f, 6.00f);
        style.ItemSpacing = ImVec2(6.00f, 6.00f);
        style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
        style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
        style.IndentSpacing = 25;
        style.ScrollbarSize = 15;
        style.GrabMinSize = 10;
        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 1;
        style.TabBorderSize = 1;
        style.WindowRounding = 7;
        style.ChildRounding = 4;
        style.FrameRounding = 3;
        style.PopupRounding = 4;
        style.ScrollbarRounding = 9;
        style.GrabRounding = 3;
        style.LogSliderDeadzone = 4;
        style.TabRounding = 4;
    }


    EffectThemeColors ThemeManager::GetEffectColors(bool enabled) {
        EffectThemeColors colors;
        
        bool isLight = (s_CurrentTheme == "Light" || s_CurrentTheme == "Adobe Spectrum Light" || s_CurrentTheme == "Cherry"); 
        
        if (s_CurrentTheme == "LedSynth") {
             if (enabled) {
                colors.Header = ImVec4(0.25f, 1.00f, 0.00f, 0.76f);
                colors.HeaderHovered = ImVec4(0.25f, 1.00f, 0.00f, 0.86f);
                colors.HeaderActive = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
                colors.Text = ImVec4(0.40f, 0.39f, 0.38f, 1.00f); 
                colors.CheckMark = ImVec4(0.25f, 1.00f, 0.00f, 0.80f);
            } else {
                colors.Header = ImVec4(1.00f, 0.98f, 0.95f, 1.00f); 
                colors.HeaderHovered = ImVec4(0.99f, 1.00f, 0.40f, 0.78f);
                colors.HeaderActive = ImVec4(0.26f, 1.00f, 0.00f, 1.00f);
                colors.Text = ImVec4(0.40f, 0.39f, 0.38f, 0.77f);
                colors.CheckMark = ImVec4(0.25f, 1.00f, 0.00f, 0.80f);
            }
        }
        else if (isLight) {
            // Light Theme Logic
            if (enabled) {
                // Enabled: Pastel Green
                colors.Header = ImVec4(0.80f, 0.95f, 0.80f, 1.0f);
                colors.HeaderHovered = ImVec4(0.70f, 0.90f, 0.70f, 1.0f);
                colors.HeaderActive = ImVec4(0.60f, 0.85f, 0.60f, 1.0f);
                colors.Text = ImVec4(0.1f, 0.1f, 0.1f, 1.0f); // Dark Text
                colors.CheckMark = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Dark/Black Checkmark
            } else {
                // Disabled: Pastel Blue/Grey
                colors.Header = ImVec4(0.90f, 0.92f, 0.94f, 1.0f);
                colors.HeaderHovered = ImVec4(0.85f, 0.88f, 0.92f, 1.0f);
                colors.HeaderActive = ImVec4(0.75f, 0.80f, 0.88f, 1.0f);
                colors.Text = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // Dark Text
                colors.CheckMark = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); 
            }
        } else {
            // Dark Theme Logic (Original Hardcoded Values)
            if (enabled) {
                colors.Header = ImVec4(0.0f, 0.35f, 0.0f, 1.0f);
                colors.HeaderHovered = ImVec4(0.0f, 0.45f, 0.0f, 1.0f);
                colors.HeaderActive = ImVec4(0.0f, 0.55f, 0.0f, 1.0f);
                colors.Text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White Text
                colors.CheckMark = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White Checkmark
            } else {
                colors.Header = ImVec4(0.15f, 0.25f, 0.35f, 1.0f);
                colors.HeaderHovered = ImVec4(0.20f, 0.35f, 0.50f, 1.0f);
                colors.HeaderActive = ImVec4(0.25f, 0.45f, 0.65f, 1.0f);
                colors.Text = ImVec4(0.9f, 0.9f, 0.9f, 1.0f); // Light Text
                colors.CheckMark = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White Checkmark
            }
        }
        return colors;
    }

    void ThemeManager::ApplyComfortableDarkCyanTheme() {

        // Comfortable Dark Cyan style by SouthCraftX from ImThemes
        ImGuiStyle& style = ImGui::GetStyle();


        style.Alpha = 1.0f;
        style.DisabledAlpha = 1.0f;
        style.WindowPadding = ImVec2(20.0f, 20.0f);
        style.WindowRounding = 11.5f;
        style.WindowBorderSize = 0.0f;
        style.WindowMinSize = ImVec2(20.0f, 20.0f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ChildRounding = 20.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 17.4f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(20.0f, 3.4f);
        style.FrameRounding = 11.9f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(8.9f, 13.4f);
        style.ItemInnerSpacing = ImVec2(7.1f, 1.8f);
        style.CellPadding = ImVec2(12.1f, 9.2f);
        style.IndentSpacing = 0.0f;
        style.ColumnsMinSpacing = 8.7f;
        style.ScrollbarSize = 11.6f;
        style.ScrollbarRounding = 15.9f;
        style.GrabMinSize = 3.7f;
        style.GrabRounding = 20.0f;
        style.TabRounding = 9.8f;
        style.TabBorderSize = 0.0f;
        style.TabMinWidthForCloseButton = 0.0f;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
        
        style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.27450982f, 0.31764707f, 0.4509804f, 1.0f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09411765f, 0.101960786f, 0.11764706f, 1.0f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.11372549f, 0.1254902f, 0.15294118f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.03137255f, 0.9490196f, 0.84313726f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.03137255f, 0.9490196f, 0.84313726f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.6f, 0.9647059f, 0.03137255f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18039216f, 0.1882353f, 0.19607843f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15294118f, 0.15294118f, 0.15294118f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.14117648f, 0.16470589f, 0.20784314f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.105882354f, 0.105882354f, 0.105882354f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.12941177f, 0.14901961f, 0.19215687f, 1.0f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.14509805f, 0.14509805f, 0.14509805f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.03137255f, 0.9490196f, 0.84313726f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1254902f, 0.27450982f, 0.57254905f, 1.0f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.52156866f, 0.6f, 0.7019608f, 1.0f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.039215688f, 0.98039216f, 0.98039216f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.03137255f, 0.9490196f, 0.84313726f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9372549f, 0.9372549f, 0.9372549f, 1.0f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.26666668f, 0.2901961f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.49803922f, 0.5137255f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.19607843f, 0.1764706f, 0.54509807f, 0.5019608f);
    }

    void ThemeManager::ApplyComfortableLightOrangeTheme() {
        // Comfortable Light Orange style by SouthCraftX from ImThemes
        ImGuiStyle& style = ImGui::GetStyle();
        
        style.Alpha = 1.0f;
        style.DisabledAlpha = 1.0f;
        style.WindowPadding = ImVec2(20.0f, 20.0f);
        style.WindowRounding = 11.5f;
        style.WindowBorderSize = 0.0f;
        style.WindowMinSize = ImVec2(20.0f, 20.0f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ChildRounding = 20.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 17.4f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(20.0f, 3.4f);
        style.FrameRounding = 11.9f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(8.9f, 13.4f);
        style.ItemInnerSpacing = ImVec2(7.1f, 1.8f);
        style.CellPadding = ImVec2(12.1f, 9.2f);
        style.IndentSpacing = 0.0f;
        style.ColumnsMinSpacing = 8.7f;
        style.ScrollbarSize = 11.6f;
        style.ScrollbarRounding = 15.9f;
        style.GrabMinSize = 3.7f;
        style.GrabRounding = 20.0f;
        style.TabRounding = 9.8f;
        style.TabBorderSize = 0.0f;
        style.TabMinWidthForCloseButton = 0.0f;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
        
        style.Colors[ImGuiCol_Text] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.7254902f, 0.68235296f, 0.54901963f, 1.0f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.92156863f, 0.9137255f, 0.8980392f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.90588236f, 0.8980392f, 0.88235295f, 1.0f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.92156863f, 0.9137255f, 0.8980392f, 1.0f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.84313726f, 0.83137256f, 0.80784315f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.92156863f, 0.9137255f, 0.8980392f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.8862745f, 0.8745098f, 0.84705883f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.84313726f, 0.83137256f, 0.80784315f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.84313726f, 0.83137256f, 0.80784315f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.9529412f, 0.94509804f, 0.92941177f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.9529412f, 0.94509804f, 0.92941177f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.92156863f, 0.9137255f, 0.8980392f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.9019608f, 0.89411765f, 0.8784314f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.9529412f, 0.94509804f, 0.92941177f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.88235295f, 0.8666667f, 0.8509804f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.84313726f, 0.83137256f, 0.80784315f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.88235295f, 0.8666667f, 0.8509804f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.96862745f, 0.050980393f, 0.15686275f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.9647059f, 0.8f, 0.02745098f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.96862745f, 0.5882353f, 0.03529412f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.88235295f, 0.8666667f, 0.8509804f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.81960785f, 0.8117647f, 0.8039216f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.84705883f, 0.84705883f, 0.84705883f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.85882354f, 0.8352941f, 0.7921569f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.89411765f, 0.89411765f, 0.89411765f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.92156863f, 0.9137255f, 0.8980392f, 1.0f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.87058824f, 0.8509804f, 0.80784315f, 1.0f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.84313726f, 0.8156863f, 0.7490196f, 1.0f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.84313726f, 0.8156863f, 0.7490196f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.85490197f, 0.85490197f, 0.85490197f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.96862745f, 0.050980393f, 0.15686275f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.92156863f, 0.9137255f, 0.8980392f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.88235295f, 0.8666667f, 0.8509804f, 1.0f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.88235295f, 0.8666667f, 0.8509804f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.92156863f, 0.9137255f, 0.8980392f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.8745098f, 0.7254902f, 0.42745098f, 1.0f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.47843137f, 0.4f, 0.29803923f, 1.0f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.9607843f, 0.019607844f, 0.11764706f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90588236f, 0.6627451f, 0.30980393f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.6392157f, 0.39607844f, 0.043137256f, 1.0f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.9529412f, 0.94509804f, 0.92941177f, 1.0f);
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.9529412f, 0.94509804f, 0.92941177f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.88235295f, 0.8666667f, 0.8509804f, 1.0f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.9019608f, 0.89411765f, 0.8784314f, 1.0f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.0627451f, 0.0627451f, 0.0627451f, 1.0f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.5019608f, 0.4862745f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.73333335f, 0.70980394f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.5019608f, 0.4862745f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8039216f, 0.8235294f, 0.45490196f, 0.502f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.8039216f, 0.8235294f, 0.45490196f, 0.502f);
    }


}
