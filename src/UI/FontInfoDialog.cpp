#include "FontInfoDialog.h"
#include "imgui.h"
#include "../Utils/LanguageDefinitions.h"
#include <algorithm>
#include <vector>

static bool g_PlatformOpen = false;
static bool g_RequestOpen = false;
static FontMetadata g_CachedMetadata;

// Helper for ReadOnly InputText
static void TextReadOnly(const char* label, const std::string& text) {
    ImGui::InputText(label, (char*)text.c_str(), text.size() + 1, ImGuiInputTextFlags_ReadOnly);
}

namespace FontInfoDialog {
    
    // ... RenderButton remains same ...
    void RenderButton(const FontManager& fontManager) {
         if (ImGui::Button(" i ")) {
             g_CachedMetadata = fontManager.GetMetadata();
             
             // Add File Path manually if not empty
             std::string path = fontManager.GetFilePath();
             if (!path.empty()) {
                 g_CachedMetadata.insert(g_CachedMetadata.begin(), { "File Path", "", path });
             }

             g_RequestOpen = true; 
        }
        if (ImGui::IsItemHovered()) {
             ImGui::SetTooltip("View Font Information");
        }
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
            if (maxWidth > 900) maxWidth = 900;
            if (maxHeight > 700) maxHeight = 700;

            if (ImGui::BeginTabBar("FontInfoTabs")) {
                
                // --- INFO TAB ---
                if (ImGui::BeginTabItem("Information")) {
                    ImGui::BeginChild("MetaDataScroll", ImVec2(maxWidth, maxHeight - 40), false, ImGuiWindowFlags_HorizontalScrollbar);
                    
                    if (g_CachedMetadata.empty()) {
                        ImGui::Text("No metadata available.");
                    } else {
                        if (ImGui::BeginTable("MetadataTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit)) {
                            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                            ImGui::TableSetupColumn("Lang", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            int rowId = 0;
                            for (const auto& entry : g_CachedMetadata) {
                                ImGui::PushID(rowId++);
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::TextUnformatted(entry.nameID.c_str());
                                
                                ImGui::TableSetColumnIndex(1);
                                ImGui::TextUnformatted(entry.language.c_str());

                                ImGui::TableSetColumnIndex(2);
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

                     ImGui::Text("Support for %d languages detected", (int)supported.size());
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
