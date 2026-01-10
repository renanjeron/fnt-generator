#include "ExportDialog.h"
#include "imgui.h"
#include "../Font/FontManager.h"
#include "../Atlas/TextureGenerator.h"
#include "../Utils/Exporter.h"
#include "../Utils/PlatformUtils.h"
#include "../Utils/StringUtils.h"
#include <filesystem>
#include <algorithm>
#include <set>
#include <string>

// Externs from main.cpp
extern FontManager g_FontManager;
extern char g_ExportFilename[128];
extern std::string g_ExportPath;
extern int g_ExportFormat;
extern bool g_EnableKerning;
extern int g_SSAAFactor;
extern int g_HintingMode;
extern float g_ExportBgColor[4];
extern std::string g_CustomGlyphsText;
extern std::set<uint32_t> g_ExcludedGlyphs;
extern AtlasResult g_LastAtlas;
extern std::string g_StatusMessage;
extern double g_StatusTime;
extern bool g_StatusIsError;

// Forward declares from main.cpp (not in headers)
AtlasSettings ConstructSettings();
void UpdateAtlasTextures();

static bool g_Open = false;
static bool g_RequestOpen = false;
static float g_SuccessNotifyTimer = 0.0f;
static int s_ExportSSAAFactor = 1;

namespace ExportDialog {

    void Open() {
        g_RequestOpen = true;
    }

    bool RenderButton() {
        if (ImGui::Button("Export Font", ImVec2(-1, 40))) {
            Open();
            return true;
        }
        return false;
    }

    void RenderDialog() {
        if (g_RequestOpen) {
            ImGui::OpenPopup("Export Font Settings");
            g_Open = true;
            g_RequestOpen = false;
            s_ExportSSAAFactor = g_SSAAFactor; // Reset to match global setting initially
        }

        if (ImGui::BeginPopupModal("Export Font Settings", &g_Open, ImGuiWindowFlags_AlwaysAutoResize)) {
            
            // 1. Export Folder
            ImGui::Text("Export Folder:");
            ImGui::InputText("##ExportPath", (char*)g_ExportPath.c_str(), g_ExportPath.size() + 1, ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("Select Folder...")) {
                std::string folder = Utils::PickFolderDialog();
                if (!folder.empty()) {
                    g_ExportPath = folder;
                }
            }
            if (g_ExportPath.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Please select a folder!");
            }

            ImGui::Separator();
            ImGui::Spacing();

            // 2. Font/Export Name
            ImGui::Text("Font Name:");
            ImGui::InputText("##Filename", g_ExportFilename, IM_ARRAYSIZE(g_ExportFilename));

            // 3. Format
            ImGui::Text("Format:");
            const char* formats[] = { ".fnt (BMFont XML)", ".fnt (BMFont Text)", ".xml (BMFont XML)", ".txt (BMFont Text)", ".fnt (BMFont Binary)" };
            ImGui::Combo("##FormatDlg", &g_ExportFormat, formats, IM_ARRAYSIZE(formats));

            ImGui::Separator();
            ImGui::Spacing();

            // 4. Kerning
            ImGui::Checkbox("Enable Kerning", &g_EnableKerning);

            // 5. SSAA (Quality)
            const char* ssaaOptions[] = { "Standard (1x)", "High Quality (2x)", "Ultra Quality (4x)" };
            int ssaaIdx = 0;
            if (s_ExportSSAAFactor == 2) ssaaIdx = 1;
            else if (s_ExportSSAAFactor == 4) ssaaIdx = 2;

            if (ImGui::Combo("Quality (SSAA)", &ssaaIdx, ssaaOptions, IM_ARRAYSIZE(ssaaOptions))) {
                if (ssaaIdx == 0) s_ExportSSAAFactor = 1;
                else if (ssaaIdx == 1) s_ExportSSAAFactor = 2;
                else if (ssaaIdx == 2) s_ExportSSAAFactor = 4;
            }

            // 6. Background Color
            bool bgTransparent = (g_ExportBgColor[3] == 0.0f);
            if (ImGui::Checkbox("Transparent Background", &bgTransparent)) {
                if (bgTransparent) {
                    g_ExportBgColor[3] = 0.0f;
                } else {
                    g_ExportBgColor[3] = 1.0f;
                }
            }
            if (!bgTransparent) {
                ImGui::ColorEdit4("Background Color", g_ExportBgColor, ImGuiColorEditFlags_NoAlpha);
                g_ExportBgColor[3] = 1.0f;
            }

            ImGui::Separator();
            ImGui::Spacing();

            // Action Buttons
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                g_Open = false;
            }
            ImGui::SameLine();
            
            bool canExport = !g_ExportPath.empty() && !g_LastAtlas.pages.empty();
            if (!canExport) ImGui::BeginDisabled();
            
            if (ImGui::Button("Export", ImVec2(120, 0))) {
                // Actual Export Logic (Copied from main.cpp)
                g_StatusMessage = "Generating High Quality Atlas...";
                
                AtlasSettings hqSettings = ConstructSettings();
                hqSettings.hintingMode = g_HintingMode;
                hqSettings.superSamplingFactor = s_ExportSSAAFactor; // Override with local setting

                // Re-generate charset logic
                std::vector<uint32_t> charset = Utils::DecodeUtf8(g_CustomGlyphsText.c_str());
                std::sort(charset.begin(), charset.end());
                charset.erase(std::unique(charset.begin(), charset.end()), charset.end());

                if(charset.empty()) { for(uint32_t c=0x20; c<=0x7E; c++) charset.push_back(c); }
                
                if (!g_ExcludedGlyphs.empty()) {
                std::vector<uint32_t> filtered;
                for (uint32_t c : charset) if (g_ExcludedGlyphs.find(c) == g_ExcludedGlyphs.end()) filtered.push_back(c);
                charset = filtered;
                }

                AtlasResult hqAtlas = TextureGenerator::GenerateAtlas(g_FontManager, charset, hqSettings);

                if (!hqAtlas.hasErrors && !hqAtlas.pages.empty()) {
                std::string fname = (std::string(g_ExportFilename).empty()) ? "font_export" : std::string(g_ExportFilename);
                
                    int formatEnum = 0; // Default XML
                    std::string ext = ".fnt";
                    switch(g_ExportFormat) {
                        case 0: formatEnum = 0; ext = ".fnt"; break; // XML
                        case 1: formatEnum = 1; ext = ".fnt"; break; // Text
                        case 2: formatEnum = 0; ext = ".xml"; break; // XML
                        case 3: formatEnum = 1; ext = ".txt"; break; // Text
                        case 4: formatEnum = 2; ext = ".fnt"; break; // Binary
                    }

                uint8_t exportBg[4] = {
                    (uint8_t)(g_ExportBgColor[0] * 255),
                    (uint8_t)(g_ExportBgColor[1] * 255),
                    (uint8_t)(g_ExportBgColor[2] * 255),
                    (uint8_t)(g_ExportBgColor[3] * 255)
                };
                if (Exporter::ExportAtlasToDisk(hqAtlas, g_ExportPath, fname, formatEnum, ext, exportBg)) {
                        g_StatusMessage = "Export Success (HQ)!";
                        g_StatusTime = ImGui::GetTime();
                        g_StatusIsError = false;
                        
                        // Update preview with the HQ version
                        g_LastAtlas = hqAtlas;
                        UpdateAtlasTextures(); 
                        
                        // Trigger success notification (3 seconds)
                        g_SuccessNotifyTimer = 3.0f;
                        
                        ImGui::CloseCurrentPopup();
                        g_Open = false;
                } else {
                        g_StatusMessage = "Export Failed!";
                        g_StatusTime = ImGui::GetTime();
                        g_StatusIsError = true;
                }
                } else {
                g_StatusMessage = hqAtlas.errorMessage.empty() ? "Generation Failed" : hqAtlas.errorMessage;
                g_StatusTime = ImGui::GetTime();
                g_StatusIsError = true;
                }
            }

            if (!canExport) ImGui::EndDisabled();

            ImGui::EndPopup();
        }
    }

    void RenderSuccessNotification() {
        if (g_SuccessNotifyTimer <= 0.0f) return;

        g_SuccessNotifyTimer -= ImGui::GetIO().DeltaTime;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 center = viewport->GetCenter();

        const char* msg = "EXPORT SUCCESSFUL!";
        ImVec2 msgSize = ImGui::CalcTextSize(msg);
        float paddingX = 40.0f;
        float paddingY = 15.0f;
        float boxW = msgSize.x + paddingX * 2.0f;
        float boxH = msgSize.y + paddingY * 2.0f;

        ImVec2 boxPos = ImVec2(center.x - boxW * 0.5f, center.y - boxH * 0.5f);
        
        // Render using the foreground draw list to be on top of everything
        ImDrawList* dl = ImGui::GetForegroundDrawList();

        // Style matching the user's image but in green
        // Dark background (Deep green)
        dl->AddRectFilled(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH), IM_COL32(0, 40, 0, 220), 2.0f);
        // Bright green border
        dl->AddRect(boxPos, ImVec2(boxPos.x + boxW, boxPos.y + boxH), IM_COL32(0, 255, 0, 255), 2.0f, 0, 1.5f);
        
        // Text (Bright green or slightly yellowish green for contrast)
        dl->AddText(ImVec2(boxPos.x + paddingX, boxPos.y + paddingY), IM_COL32(100, 255, 100, 255), msg);
    }
}
