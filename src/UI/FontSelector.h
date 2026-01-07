#pragma once
#include <string>
#include <vector>
#include <set>
#include <functional>
#include "imgui.h"
#include <GLFW/glfw3.h>
#include "../Utils/PlatformUtils.h"
#include "../Font/FontManager.h"

namespace UI {
    void RenderFontSelector(
        const char* label,
        const char* comboId,
        int& selectedIndex,
        const std::vector<Utils::FontInfo>& systemFonts,
        std::set<std::string>& favorites,
        char* searchBuffer,
        size_t searchBufferSize,
        FontManager& fontManager,
        bool isFallback,
        bool showPreview,
        std::function<void()> onUpdate,
        std::function<void(const std::string&)> onFavoriteToggle
    );
}
