#include "FontInfoDialog.h"
#include "imgui.h"
#include "../Utils/LanguageDefinitions.h"
#include <algorithm>
#include <vector>

static bool g_PlatformOpen = false;
static bool g_RequestOpen = false;
static FontMetadata g_CachedMetadata;
static std::string g_SelectedLanguage = "";
static std::vector<std::string> g_AvailableLanguages;

// Helper for ReadOnly InputText
static void TextReadOnly(const char* label, const std::string& text) {
    ImGui::InputText(label, (char*)text.c_str(), text.size() + 1, ImGuiInputTextFlags_ReadOnly);
}

namespace FontInfoDialog {
    
    // Internal helper for tooltips
    static void HelpMarker(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void RenderButton(const FontManager& fontManager) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0)); 
        if (ImGui::Button("##InfoBtn", ImVec2(18, 18))) {
             g_CachedMetadata = fontManager.GetMetadata();
             
             // Add File Path manually if not empty
             std::string path = fontManager.GetFilePath();
             if (!path.empty()) {
                 g_CachedMetadata.insert(g_CachedMetadata.begin(), { "File Path", "", path });
             }

             // Collect unique languages
             g_AvailableLanguages.clear();
             bool hasEn = false;
             for (const auto& entry : g_CachedMetadata) {
                 if (entry.language.empty()) continue; // Skip general info
                 
                 if (std::find(g_AvailableLanguages.begin(), g_AvailableLanguages.end(), entry.language) == g_AvailableLanguages.end()) {
                     g_AvailableLanguages.push_back(entry.language);
                     if (entry.language == "en") hasEn = true;
                 }
             }
             std::sort(g_AvailableLanguages.begin(), g_AvailableLanguages.end());

             if (hasEn) g_SelectedLanguage = "en";
             else if (!g_AvailableLanguages.empty()) g_SelectedLanguage = g_AvailableLanguages[0];
             else g_SelectedLanguage = "";

             g_RequestOpen = true; 
        }
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
             ImGui::SetTooltip("View Font Information");
        }

        // Draw 'i' icon
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetItemRectMin();
        ImVec2 p_max = ImGui::GetItemRectMax();
        ImVec2 center = ImVec2((float)(int)((p_min.x + p_max.x) * 0.5f), (float)(int)((p_min.y + p_max.y) * 0.5f));
        
        ImU32 iconColor = ImGui::GetColorU32(ImGuiCol_Text);
        if (ImGui::IsItemHovered()) iconColor = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);

        // Circle outline
        draw_list->AddCircle(center, 7.0f, iconColor, 0, 1.5f);
        
        // 'i' body (Rect for sharpness)
        draw_list->AddRectFilled(ImVec2(center.x - 1, center.y), ImVec2(center.x + 1, center.y + 4), iconColor);
        
        // 'i' dot (Circle)
        draw_list->AddCircleFilled(ImVec2(center.x, center.y - 3.0f), 1.5f, iconColor);
    }

    void RenderDialog(const FontManager& fontManager) {
        if (g_RequestOpen) {
            ImGui::OpenPopup("Font Information");
            g_PlatformOpen = true;
            g_RequestOpen = false;
        }

        if (ImGui::BeginPopupModal("Font Information", &g_PlatformOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            
            // Dimensions
            float maxHeight = ImGui::GetIO().DisplaySize.y * 0.8f;
            float maxWidth = ImGui::GetIO().DisplaySize.x * 0.8f;
            if (maxWidth < 450) maxWidth = 450;
            if (maxWidth > 900) maxWidth = 900;
            if (maxHeight > 700) maxHeight = 700;

            if (ImGui::BeginTabBar("FontInfoTabs")) {
                
                // --- INFO TAB ---
                if (ImGui::BeginTabItem("Information")) {
                    ImGui::BeginChild("MetaDataScroll", ImVec2(maxWidth, maxHeight - 40), false, ImGuiWindowFlags_HorizontalScrollbar);
                    
                    if (g_CachedMetadata.empty()) {
                        ImGui::Text("No metadata available.");
                    } else {
                        // Language Selector
                        if (!g_AvailableLanguages.empty()) {
                            ImGui::SetNextItemWidth(150.0f);
                            if (ImGui::BeginCombo("View Language", g_SelectedLanguage.c_str())) {
                                for (const auto& lang : g_AvailableLanguages) {
                                    bool isSelected = (g_SelectedLanguage == lang);
                                    if (ImGui::Selectable(lang.c_str(), isSelected)) {
                                        g_SelectedLanguage = lang;
                                    }
                                    if (isSelected) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::SameLine();
                            HelpMarker("Select which language strings to display. Metadata items without a language tag are always shown.");
                            ImGui::Spacing();
                        }

                        if (ImGui::BeginTable("MetadataTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit)) {
                            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            int rowId = 0;
                            for (const auto& entry : g_CachedMetadata) {
                                // Filter logic: Show if language is empty (General) OR matches selected language
                                if (!entry.language.empty() && entry.language != g_SelectedLanguage) {
                                    continue;
                                }

                                ImGui::PushID(rowId++);
                                ImGui::TableNextRow();
                                
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextUnformatted(entry.nameID.c_str());
                                
                                ImGui::TableSetColumnIndex(1);
                                std::string val = entry.value;
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                ImGui::InputText("##Val", (char*)val.c_str(), val.size()+1, ImGuiInputTextFlags_ReadOnly);
                                
                                ImGui::PopID();
                            }
                            ImGui::EndTable();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                // --- LANGUAGES TAB ---
                if (ImGui::BeginTabItem("Languages")) {
                     ImGui::BeginChild("LangScroll", ImVec2(maxWidth, maxHeight - 40), false);
                     
                     const auto& langs = GetLanguageDefinitions();
                     std::vector<std::string> supported;
                     
                     if (fontManager.IsLoaded()) {
                        for (const auto& lang : langs) {
                             if (fontManager.HasGlyphs(lang.requiredCodepoints)) {
                                 supported.push_back(lang.name);
                             }
                        }
                     }

                     ImGui::Text("Support for %d languages detected based on glyphs", (int)supported.size());
                     ImGui::Separator();
                     ImGui::Spacing();
                     
                     std::string listStr;
                     for(size_t i=0; i<supported.size(); i++) {
                         listStr += supported[i];
                         if (i < supported.size() - 1) listStr += ", ";
                     }
                     ImGui::TextWrapped("%s", listStr.c_str());

                     ImGui::EndChild();
                     ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }

            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                g_PlatformOpen = false;
            }
            ImGui::EndPopup();
        }
    }
}
