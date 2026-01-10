#include "FontSelector.h"
#include <algorithm>
#include <cmath>
#include "../Utils/StringUtils.h"
#include "../Utils/FontPreviewUtils.h"
#include "../Utils/ThemeManager.h"

namespace UI {

    static void DrawStarIcon(bool isFav, const std::string& path, std::function<void(const std::string&)> onFavoriteToggle) {
        ImGui::PushID(path.c_str());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
        
        if (ImGui::Button("##FavBtn", ImVec2(18, 18))) {
            onFavoriteToggle(path);
        }
        ImGui::PopStyleColor(3);
        
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(isFav ? "Remove from Favorites" : "Add to Favorites");

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetItemRectMin();
        ImVec2 p_max = ImGui::GetItemRectMax();
        ImVec2 center = ImVec2((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f);
        float radius = 7.0f;

        ImVec4 favoriteColor = Utils::ThemeManager::GetFavoriteColor();
        ImVec4 disabledColor = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        ImU32 starColor = isFav ? ImGui::ColorConvertFloat4ToU32(favoriteColor) : ImGui::ColorConvertFloat4ToU32(disabledColor);
        
        if (ImGui::IsItemHovered()) {
            if (isFav) {
                // Slightly brighter favorite
                ImVec4 hoveredFav = favoriteColor;
                hoveredFav.x = (std::min)(1.0f, hoveredFav.x + 0.1f);
                hoveredFav.y = (std::min)(1.0f, hoveredFav.y + 0.1f);
                starColor = ImGui::ColorConvertFloat4ToU32(hoveredFav);
            } else {
                starColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text));
            }
        }

        ImVec2 starPoints[10];
        for(int k=0; k<10; k++) {
                float a = (k * 36.0f - 90.0f) * 3.14159f / 180.0f;
                float r = (k % 2 == 0) ? radius : radius * 0.45f;
                starPoints[k] = ImVec2(center.x + cosf(a) * r, center.y + sinf(a) * r);
        }

        if (isFav) {
            for(int k=0; k<10; k++) {
                draw_list->AddTriangleFilled(center, starPoints[k], starPoints[(k+1)%10], starColor);
            }
        }
        else draw_list->AddPolyline(starPoints, 10, starColor, true, 1.5f);

        ImGui::PopID();
    }

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
    ) {
        std::string previewName = (isFallback) ? "None (Optional)" : "Select Font...";
        if (selectedIndex >= 0 && selectedIndex < (int)systemFonts.size()) {
            previewName = systemFonts[selectedIndex].name;
        }

        // To fix small width issue, ensure we use available width
        ImGui::SetNextItemWidth(-FLT_MIN); 
        
        if (ImGui::BeginCombo(comboId, previewName.c_str(), ImGuiComboFlags_HeightLarge)) {
            // Search input (Fixed at top)
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextWithHint("##FontSearch", "Search...", searchBuffer, searchBufferSize);
            ImGui::Separator();
            
            // Sort Logic
            std::vector<int> sortedIndices;
            sortedIndices.reserve(systemFonts.size());
            for (int i = 0; i < (int)systemFonts.size(); i++) sortedIndices.push_back(i);
            
            std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
                bool aFav = favorites.count(systemFonts[a].path) > 0;
                bool bFav = favorites.count(systemFonts[b].path) > 0;
                if (aFav != bFav) return aFav; 
                return systemFonts[a].name < systemFonts[b].name; 
            });
            
            // Scrollable List
            // Using a child window for the list allows the search bar to remain fixed at the top
            ImGui::BeginChild("##FontList", ImVec2(0, 300), false, ImGuiWindowFlags_HorizontalScrollbar);
            
            // "None" option for fallback
            if (isFallback) {
                if (ImGui::Selectable("None (Clear)", selectedIndex == -1)) {
                    selectedIndex = -1;
                    fontManager.ClearFallbackFont();
                    onUpdate();
                    ImGui::CloseCurrentPopup();
                }
            }

            for (int idx : sortedIndices) {
                if (searchBuffer[0] != '\0' && !Utils::StringContains(systemFonts[idx].name, searchBuffer)) continue;
                
                bool isFav = favorites.count(systemFonts[idx].path) > 0;
                
                DrawStarIcon(isFav, systemFonts[idx].path, onFavoriteToggle);
                ImGui::SameLine();
                
                if (isFav) ImGui::PushStyleColor(ImGuiCol_Text, Utils::ThemeManager::GetFavoriteColor()); 

                if (showPreview) {
                    GLuint tex = Utils::GenerateFontPreview(systemFonts[idx].path);
                    if (tex != 0) {
                        ImGui::Image((void*)(intptr_t)tex, ImVec2(40, 20), ImVec2(0,0), ImVec2(1,1), Utils::ThemeManager::GetFontPreviewColor(), ImVec4(1,1,1,0.0f));
                        ImGui::SameLine();
                    }
                }
                
                bool is_selected = (selectedIndex == idx);
                if (ImGui::Selectable(systemFonts[idx].name.c_str(), is_selected)) {
                    selectedIndex = idx;
                    if (isFallback) {
                        fontManager.LoadFallbackFont(systemFonts[idx].path);
                    } else {
                        fontManager.LoadFont(systemFonts[idx].path);
                    }
                    onUpdate();
                    searchBuffer[0] = '\0'; // Clear search on select
                    ImGui::CloseCurrentPopup();
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
                
                if (isFav) ImGui::PopStyleColor();
            }
            ImGui::EndChild();
            ImGui::EndCombo();
        }
    }
}
