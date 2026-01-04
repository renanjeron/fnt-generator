#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <imgui_gradient/imgui_gradient.hpp>
#include <stdio.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <set>
#include <filesystem>

#include "Utils/PlatformUtils.h"
#include "Font/FontManager.h"
#include "Atlas/TextureGenerator.h"
#include "Utils/Exporter.h"
#include "Utils/UnicodeBlocks.h"
#include "Utils/StringUtils.h"
#include "Utils/JsonUtils.h"
#include "Utils/StyleUtils.h"
#include "Utils/UIUtils.h"



// --- State Variables ---
static std::vector<Utils::FontInfo> g_SystemFonts;
static FontManager g_FontManager;
static int g_SelectedFontIndex = -1;
static char g_InputText[1024] = "Hello Everyone";
static int g_FontSize = 72;
static int g_Padding = 5;

// Atlas Size
static int g_AtlasWidth = 0;
static int g_AtlasHeight = 0;

// Fill
// Fill
static float g_FillColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
static bool g_EnableGradient = false;
static ImGG::GradientWidget g_FillGradientWidget;
static int g_FillGradientType = 0; // 0=Linear, 1=Radial
static float g_FillGradientAngle = 90.0f;

// Stroke
static bool g_EnableStroke = false;
static bool g_EnableStrokeGradient = false;
static float g_StrokeWidth = 2.0f;
static float g_StrokeColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static ImGG::GradientWidget g_StrokeGradientWidget;
static int g_StrokeGradientType = 0;
static float g_StrokeGradientAngle = 90.0f;
static int g_StrokePosition = 0; // 0=Outside, 1=Center, 2=Inside

// Shadow
static bool g_EnableShadow = false;
static int g_ShadowOffsetX = 5;
static int g_ShadowOffsetY = 5;
static int g_ShadowBlur = 0;
static float g_ShadowColor[4] = {0.0f, 0.0f, 0.0f, 0.5f}; // RGBA

// Bevel
static bool g_EnableBevel = false;
static int g_BevelAngle = 135;
static float g_BevelDistance = 4.0f;
static float g_BevelSpread = 4.0f;
static float g_BevelStrength = 1.0f;
static int g_BevelType = 0; // 0=Inner, 1=Outer
static float g_BevelHighlightColor[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // RGBA
static float g_BevelShadowColor[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // RGBA

// Inner Glow
static bool g_EnableInnerGlow = false;
static float g_InnerGlowSize = 5.0f;
static float g_InnerGlowChoke = 0.0f;
static float g_InnerGlowColor[4] = {1.0f, 1.0f, 1.0f, 0.5f}; // RGBA

static char g_FontSearch[128] = ""; // Search filter
// --- State Variables ---
// ... (Previous variables)
static float g_PreviewBgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Default Transparent
static char g_ExportFilename[128] = "my_font";
static int g_ExportFormat = 0; // 0=XML, 1=Text, 2=Binary
static bool g_UseExtendedCharset = false;
static bool g_UseCustomGlyphs = true; // Always true now
static std::string g_CustomGlyphsText = "";
static std::vector<UnicodeBlock> g_UnicodeBlocks = UNICODE_BLOCKS; // Mutable copy
static GLuint g_CheckerTexture = 0;
// Atlas Interaction
static std::set<uint32_t> g_ExcludedGlyphs;
static bool g_PreviewSSAA = false;
static int g_HintingMode = 0; // 0 = Smooth (No Hinting), 1 = Sharp (Auto), 2 = Crisp (Normal)
static bool g_OpenPreferences = false;
static int g_SelectedGlyphIndex = -1;
static float g_AtlasZoom = 1.0f;
static ImVec2 g_AtlasPan = { 0, 0 };
static bool g_IsPanning = false;

static float g_TextZoom = 1.0f;
static ImVec2 g_TextPan = { 0, 0 };
static bool g_IsPanningText = false;

// Char Adjustments
static int g_GlobalXAdvance = 0;
static int g_GlobalXOffset = 0;
static int g_GlobalYOffset = 0;

// Favorite fonts system
static std::set<std::string> g_FavoriteFonts;
// Recent Styles
static std::vector<std::string> g_RecentStyles;
static bool g_ShowRecentError = false;
static std::string g_RecentNotFoundPath = "";



// Simple JSON helpers
void SaveStyle(const std::string& path);
void LoadStyle(const std::string& path);
// Custom Knob Widget


void UpdatePreview(const char* text); // Forward checking

// Implementation of Style Save/Load (simplified)
void SaveStyle(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return;
    
    out << "{\n";
    out << "  \"fontSize\": " << g_FontSize << ",\n";
    out << "  \"padding\": " << g_Padding << ",\n";
    out << "  \"atlasWidth\": " << g_AtlasWidth << ",\n";
    out << "  \"atlasHeight\": " << g_AtlasHeight << ",\n";
    out << "  \"fillColor\": [" << g_FillColor[0] << ", " << g_FillColor[1] << ", " << g_FillColor[2] << ", " << g_FillColor[3] << "],\n";
    
    out << "  \"enableGradient\": " << (g_EnableGradient ? "true" : "false") << ",\n";
    out << "  \"fillGradientType\": " << g_FillGradientType << ",\n";
    out << "  \"fillGradientAngle\": " << g_FillGradientAngle << ",\n";
    out << "  \"fillGradientStops\": [";
    {
        auto marks = g_FillGradientWidget.gradient().get_marks();
        bool first = true;
        for(const auto& m : marks) {
            if(!first) out << ", ";
            out << "{ \"p\": " << m.position.get() << ", \"c\": [" << m.color.x << ", " << m.color.y << ", " << m.color.z << ", " << m.color.w << "] }";
            first = false;
        }
    }
    out << "],\n";
    
    out << "  \"enableStroke\": " << (g_EnableStroke ? "true" : "false") << ",\n";
    out << "  \"strokeWidth\": " << g_StrokeWidth << ",\n";
    out << "  \"strokeColor\": [" << g_StrokeColor[0] << ", " << g_StrokeColor[1] << ", " << g_StrokeColor[2] << ", " << g_StrokeColor[3] << "],\n";
    
    out << "  \"enableStrokeGradient\": " << (g_EnableStrokeGradient ? "true" : "false") << ",\n";
    out << "  \"strokeGradientType\": " << g_StrokeGradientType << ",\n";
    out << "  \"strokeGradientAngle\": " << g_StrokeGradientAngle << ",\n";
    out << "  \"strokeGradientStops\": [";
    {
        auto marks = g_StrokeGradientWidget.gradient().get_marks();
        bool first = true;
        for(const auto& m : marks) {
            if(!first) out << ", ";
            out << "{ \"p\": " << m.position.get() << ", \"c\": [" << m.color.x << ", " << m.color.y << ", " << m.color.z << ", " << m.color.w << "] }";
            first = false;
        }
    }
    out << "],\n";
    
    out << "  \"enableShadow\": " << (g_EnableShadow ? "true" : "false") << ",\n";
    out << "  \"shadowOffsetX\": " << g_ShadowOffsetX << ",\n";
    out << "  \"shadowOffsetY\": " << g_ShadowOffsetY << ",\n";
    out << "  \"shadowBlur\": " << g_ShadowBlur << ",\n";
    out << "  \"shadowColor\": [" << g_ShadowColor[0] << ", " << g_ShadowColor[1] << ", " << g_ShadowColor[2] << ", " << g_ShadowColor[3] << "],\n";

    out << "  \"enableInnerGlow\": " << (g_EnableInnerGlow ? "true" : "false") << ",\n";
    out << "  \"innerGlowSize\": " << g_InnerGlowSize << ",\n";
    out << "  \"innerGlowChoke\": " << g_InnerGlowChoke << ",\n";
    out << "  \"innerGlowColor\": [" << g_InnerGlowColor[0] << ", " << g_InnerGlowColor[1] << ", " << g_InnerGlowColor[2] << ", " << g_InnerGlowColor[3] << "],\n";
    
    out << "  \"enableBevel\": " << (g_EnableBevel ? "true" : "false") << ",\n";
    out << "  \"bevelDistance\": " << g_BevelDistance << ",\n";
    out << "  \"bevelAngle\": " << g_BevelAngle << ",\n";
    out << "  \"bevelSpread\": " << g_BevelSpread << ",\n";
    out << "  \"bevelStrength\": " << g_BevelStrength << ",\n";
    out << "  \"bevelType\": " << g_BevelType << ",\n";
    out << "  \"bevelHighlightColor\": [" << g_BevelHighlightColor[0] << ", " << g_BevelHighlightColor[1] << ", " << g_BevelHighlightColor[2] << ", " << g_BevelHighlightColor[3] << "],\n";
    out << "  \"bevelShadowColor\": [" << g_BevelShadowColor[0] << ", " << g_BevelShadowColor[1] << ", " << g_BevelShadowColor[2] << ", " << g_BevelShadowColor[3] << "],\n";

    out << "  \"exportFilename\": \"" << g_ExportFilename << "\",\n";
    
    // Font Path
    if (g_SelectedFontIndex >= 0 && g_SelectedFontIndex < (int)g_SystemFonts.size()) {
        std::string fp = g_SystemFonts[g_SelectedFontIndex].path;
        std::string escaped;
        for(char c : fp) {
            if(c == '\\') escaped += "\\\\";
            else escaped += c;
        }
        out << "  \"fontPath\": \"" << escaped << "\",\n";
    }
    
    // Excluded Glyphs
    out << "  \"excludedGlyphs\": [";
    bool firstEx = true;
    for(uint32_t code : g_ExcludedGlyphs) {
        if(!firstEx) out << ", ";
        out << code;
        firstEx = false;
    }
    out << "],\n";
    
    // Custom Glyphs
    out << "  \"useCustomGlyphs\": true,\n";
    std::string escCustom;
    for(char c : g_CustomGlyphsText) {
        if(c == '\\') escCustom += "\\\\";
        else if(c == '"') escCustom += "\\\"";
        else if(c == '\n') escCustom += "\\n";
        else escCustom += c;
    }
    out << "  \"customGlyphsText\": \"" << escCustom << "\",\n";
    
    // Export Format
    out << "  \"exportFormat\": " << g_ExportFormat << ",\n";

    // Char Adjustments
    out << "  \"globalXAdvance\": " << g_GlobalXAdvance << ",\n";
    out << "  \"globalXOffset\": " << g_GlobalXOffset << ",\n";
    out << "  \"globalYOffset\": " << g_GlobalYOffset << ",\n";

    // Blocks
    out << "  \"blocks\": [\n";
    bool first = true;
    for(const auto& b : g_UnicodeBlocks) {
        if(b.enabled) {
            if(!first) out << ",\n";
            out << "    \"" << b.name << "\"";
            first = false;
        }
    }
    out << "\n  ]\n";
    out << "}\n";
}



void ParseGradientStops(const std::string& content, const std::string& key, ImGG::GradientWidget& widget) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    if (start == std::string::npos) return;

    int brackets = 0;
    size_t aEnd = start;
    for(size_t i=start; i<content.length(); i++) {
        if(content[i] == '[') brackets++;
        else if(content[i] == ']') {
            brackets--;
            if(brackets == 0) { aEnd = i; break; }
        }
    }
    
    widget.gradient().clear();
    std::string block = content.substr(start, aEnd - start + 1);
    
    size_t bCur = 0;
    while(true) {
        size_t bObj = block.find("{", bCur);
        if(bObj == std::string::npos) break;
        size_t bObjClose = block.find("}", bObj);
        if(bObjClose == std::string::npos) break;
        
        std::string item = block.substr(bObj, bObjClose - bObj + 1);
        float p = Utils::ParseFloatValue(item, "p", 0.0f);
        float c[4] = {1,1,1,1};
        Utils::ParseColor4(item, "c", c);
        
        widget.gradient().add_mark(ImGG::Mark(ImGG::RelativePosition(p), ImVec4(c[0], c[1], c[2], c[3])));
        
        bCur = bObjClose + 1;
    }
    
    if(widget.gradient().get_marks().empty()) {
        widget.gradient().add_mark(ImGG::Mark(ImGG::RelativePosition(0.0f), ImVec4(0,0,0,1)));
        widget.gradient().add_mark(ImGG::Mark(ImGG::RelativePosition(1.0f), ImVec4(1,1,1,1)));
    }
}

void LoadStyle(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string c = buffer.str();
    
    g_FontSize = Utils::ParseIntValue(c, "fontSize", g_FontSize);
    g_Padding = Utils::ParseIntValue(c, "padding", g_Padding);
    g_AtlasWidth = Utils::ParseIntValue(c, "atlasWidth", Utils::ParseIntValue(c, "atlasSize", 1024));
    g_AtlasHeight = Utils::ParseIntValue(c, "atlasHeight", Utils::ParseIntValue(c, "atlasSize", 1024));
    
    Utils::ParseColor4(c, "fillColor", g_FillColor);
    g_EnableGradient = Utils::ParseBoolValue(c, "enableGradient", g_EnableGradient);
    
    g_FillGradientType = Utils::ParseIntValue(c, "fillGradientType", 0);
    g_FillGradientAngle = Utils::ParseFloatValue(c, "fillGradientAngle", 90.0f);
    ParseGradientStops(c, "fillGradientStops", g_FillGradientWidget);
    
    g_EnableStroke = Utils::ParseBoolValue(c, "enableStroke", false);
    g_StrokeWidth = Utils::ParseFloatValue(c, "strokeWidth", 2.0f);
    Utils::ParseColor4(c, "strokeColor", g_StrokeColor);
    
    g_EnableStrokeGradient = Utils::ParseBoolValue(c, "enableStrokeGradient", false);
    g_StrokeGradientType = Utils::ParseIntValue(c, "strokeGradientType", 0);
    g_StrokeGradientAngle = Utils::ParseFloatValue(c, "strokeGradientAngle", 90.0f);
    ParseGradientStops(c, "strokeGradientStops", g_StrokeGradientWidget);
    
    g_EnableShadow = Utils::ParseBoolValue(c, "enableShadow", g_EnableShadow);
    g_ShadowOffsetX = Utils::ParseIntValue(c, "shadowOffsetX", g_ShadowOffsetX);
    g_ShadowOffsetY = Utils::ParseIntValue(c, "shadowOffsetY", g_ShadowOffsetY);
    g_ShadowBlur = Utils::ParseIntValue(c, "shadowBlur", g_ShadowBlur);
    Utils::ParseColor4(c, "shadowColor", g_ShadowColor);
 
    g_EnableInnerGlow = Utils::ParseBoolValue(c, "enableInnerGlow", g_EnableInnerGlow);
    g_InnerGlowSize = Utils::ParseFloatValue(c, "innerGlowSize", g_InnerGlowSize);
    g_InnerGlowChoke = Utils::ParseFloatValue(c, "innerGlowChoke", g_InnerGlowChoke);
    Utils::ParseColor4(c, "innerGlowColor", g_InnerGlowColor);

    g_EnableBevel = Utils::ParseBoolValue(c, "enableBevel", g_EnableBevel);
    g_BevelDistance = Utils::ParseFloatValue(c, "bevelDistance", g_BevelDistance);
    g_BevelAngle = Utils::ParseIntValue(c, "bevelAngle", g_BevelAngle);
    g_BevelSpread = Utils::ParseFloatValue(c, "bevelSpread", g_BevelSpread);
    g_BevelStrength = Utils::ParseFloatValue(c, "bevelStrength", g_BevelStrength);
    g_BevelType = Utils::ParseIntValue(c, "bevelType", g_BevelType);
    Utils::ParseColor4(c, "bevelHighlightColor", g_BevelHighlightColor);
    Utils::ParseColor4(c, "bevelShadowColor", g_BevelShadowColor);
    
    // Legacy support for older styles
    if (c.find("\"bevelColor\"") != std::string::npos) {
        float oldCol[3];
        Utils::ParseColor3(c, "bevelColor", oldCol);
        g_BevelHighlightColor[0] = oldCol[0];
        g_BevelHighlightColor[1] = oldCol[1];
        g_BevelHighlightColor[2] = oldCol[2];
        g_BevelHighlightColor[3] = 1.0f;
    }
    
    std::string fname = Utils::ParseStringValue(c, "exportFilename");
    if(!fname.empty()) strncpy(g_ExportFilename, fname.c_str(), sizeof(g_ExportFilename));
    
    // Font Path
    std::string fp = Utils::ParseStringValue(c, "fontPath");
    if(!fp.empty()) {
        for(size_t i=0; i<g_SystemFonts.size(); i++) {
            if(g_SystemFonts[i].path == fp) {
                g_SelectedFontIndex = (int)i;
                g_FontManager.LoadFont(fp);
                break;
            }
        }
    }
    
    // Excluded Glyphs
    g_ExcludedGlyphs.clear();
    Utils::ParseIntArray(c, "excludedGlyphs", g_ExcludedGlyphs);
    
    g_UseCustomGlyphs = true;
    std::string customText = Utils::ParseStringValue(c, "customGlyphsText");
    if(!customText.empty()) {
        // Simple unescape for newline
        std::string unesc;
        for(size_t i=0; i<customText.size(); i++) {
            if(customText[i] == '\\' && i+1 < customText.size() && customText[i+1] == 'n') {
                unesc += '\n'; i++;
            } else {
                unesc += customText[i];
            }
        }
        g_CustomGlyphsText = unesc;
    }
    
    g_ExportFormat = Utils::ParseIntValue(c, "exportFormat", 0);
 
    g_GlobalXAdvance = Utils::ParseIntValue(c, "globalXAdvance", 0);
    g_GlobalXOffset = Utils::ParseIntValue(c, "globalXOffset", 0);
    g_GlobalYOffset = Utils::ParseIntValue(c, "globalYOffset", 0);

    // Blocks
    for(auto& b : g_UnicodeBlocks) b.enabled = false;
    size_t blocksStart = c.find("\"blocks\"");
    if (blocksStart != std::string::npos) {
        size_t arrStart = c.find("[", blocksStart);
        size_t arrEnd = c.find("]", arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            std::string blocksContent = c.substr(arrStart, arrEnd - arrStart);
            for(auto& b : g_UnicodeBlocks) {
                if (blocksContent.find("\"" + b.name + "\"") != std::string::npos) {
                    b.enabled = true;
                }
            }
        }
    }
    
    UpdatePreview(g_InputText);
}

// Toggle favorite status
void ToggleFavorite(const std::string& fontPath) {
    if (g_FavoriteFonts.count(fontPath)) {
        g_FavoriteFonts.erase(fontPath);
    } else {
        g_FavoriteFonts.insert(fontPath);
    }
    Utils::SaveFavorites(g_FavoriteFonts);
}

// IsFavorite remains as it's small/local
bool IsFavorite(const std::string& fontPath) {
    return g_FavoriteFonts.count(fontPath) > 0;
}

// checker texture helper moved to UIUtils/

// The extended charset from the web app
const char* EXTENDED_CHARSET_STR = 
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
    "¡¢£¤¥¦§¨©ª«¬\u00A0®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþœ"
    "ЁАБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдежзийклмнопрстуфхцчшщъыьэюяё";

// Texture for the preview (Atlas)
static GLuint g_PreviewTexture = 0;
static GLuint g_TextPreviewTexture = 0; // New: Text Line Preview
static int g_PreviewWidth = 0;
static int g_PreviewHeight = 0;
static AtlasResult g_LastAtlas;
static AtlasResult g_LastTextPreview; // New
static std::string g_StatusMessage = "";
static double g_StatusTime = 0.0;
static bool g_StatusIsError = false; // Track if status is error or success

// ... (UpdatePreview remains same) ...
AtlasSettings ConstructSettings() {
    AtlasSettings settings;
    settings.fontSize = g_FontSize;
    settings.padding = g_Padding;
    settings.atlasWidth = g_AtlasWidth;
    settings.atlasHeight = g_AtlasHeight;
    settings.useSuperSampling = false;
    settings.hintingMode = g_HintingMode;
    
    // Fill
    // Fill
    settings.fillColor[0] = (uint8_t)(g_FillColor[0] * 255);
    settings.fillColor[1] = (uint8_t)(g_FillColor[1] * 255);
    settings.fillColor[2] = (uint8_t)(g_FillColor[2] * 255);
    settings.fillColor[3] = (uint8_t)(g_FillColor[3] * 255);
    
    settings.fillGradient.enabled = g_EnableGradient;
    settings.fillGradient.type = (GradientType)g_FillGradientType;
    settings.fillGradient.angle = g_FillGradientAngle;
    if (g_EnableGradient) {
        auto marks = g_FillGradientWidget.gradient().get_marks();
        for (const auto& m : marks) {
            GradientStop s;
            s.position = m.position.get();
            s.color[0] = m.color.x; s.color[1] = m.color.y; s.color[2] = m.color.z; s.color[3] = m.color.w;
            settings.fillGradient.stops.push_back(s);
        }
    }

    // Stroke
    settings.enableStroke = g_EnableStroke;
    settings.strokeWidth = g_StrokeWidth;
    settings.strokePosition = g_StrokePosition;
    settings.strokeColor[0] = (uint8_t)(g_StrokeColor[0] * 255);
    settings.strokeColor[1] = (uint8_t)(g_StrokeColor[1] * 255);
    settings.strokeColor[2] = (uint8_t)(g_StrokeColor[2] * 255);
    settings.strokeColor[3] = (uint8_t)(g_StrokeColor[3] * 255);
    
    settings.strokeGradient.enabled = g_EnableStrokeGradient;
    settings.strokeGradient.type = (GradientType)g_StrokeGradientType;
    settings.strokeGradient.angle = g_StrokeGradientAngle;
    if (g_EnableStrokeGradient) {
         auto marks = g_StrokeGradientWidget.gradient().get_marks();
         for(const auto& m : marks) {
             GradientStop s;
             s.position = m.position.get();
             s.color[0] = m.color.x; s.color[1] = m.color.y; s.color[2] = m.color.z; s.color[3] = m.color.w;
             settings.strokeGradient.stops.push_back(s);
         }
    }

    // Shadow
    settings.enableShadow = g_EnableShadow;
    settings.shadowOffsetX = g_ShadowOffsetX;
    settings.shadowOffsetY = g_ShadowOffsetY;
    settings.shadowBlur = g_ShadowBlur;
    settings.shadowColor[0] = (uint8_t)(g_ShadowColor[0] * 255);
    settings.shadowColor[1] = (uint8_t)(g_ShadowColor[1] * 255);
    settings.shadowColor[2] = (uint8_t)(g_ShadowColor[2] * 255);
    settings.shadowColor[3] = (uint8_t)(g_ShadowColor[3] * 255);
    
    // Bevel
    settings.enableBevel = g_EnableBevel;
    settings.bevelAngle = g_BevelAngle;
    settings.bevelDistance = g_BevelDistance;
    settings.bevelSpread = g_BevelSpread;
    settings.bevelStrength = g_BevelStrength;
    settings.bevelType = g_BevelType;
    settings.bevelHighlightColor[0] = (uint8_t)(g_BevelHighlightColor[0] * 255);
    settings.bevelHighlightColor[1] = (uint8_t)(g_BevelHighlightColor[1] * 255);
    settings.bevelHighlightColor[2] = (uint8_t)(g_BevelHighlightColor[2] * 255);
    settings.bevelHighlightColor[3] = (uint8_t)(g_BevelHighlightColor[3] * 255);
    settings.bevelShadowColor[0] = (uint8_t)(g_BevelShadowColor[0] * 255);
    settings.bevelShadowColor[1] = (uint8_t)(g_BevelShadowColor[1] * 255);
    settings.bevelShadowColor[2] = (uint8_t)(g_BevelShadowColor[2] * 255);
    settings.bevelShadowColor[3] = (uint8_t)(g_BevelShadowColor[3] * 255);
    
    // Inner Glow
    settings.enableInnerGlow = g_EnableInnerGlow;
    settings.innerGlowSize = g_InnerGlowSize;
    settings.innerGlowChoke = g_InnerGlowChoke;
    settings.innerGlowColor[0] = (uint8_t)(g_InnerGlowColor[0] * 255);
    settings.innerGlowColor[1] = (uint8_t)(g_InnerGlowColor[1] * 255);
    settings.innerGlowColor[2] = (uint8_t)(g_InnerGlowColor[2] * 255);
    settings.innerGlowColor[3] = (uint8_t)(g_InnerGlowColor[3] * 255);
    
    // Char Adjustments
    settings.globalXAdvance = g_GlobalXAdvance;
    settings.globalXOffset = g_GlobalXOffset;
    settings.globalYOffset = g_GlobalYOffset;

    return settings;
}

void UpdatePreview(const char* text) {
    if (!g_FontManager.IsLoaded()) return;

    AtlasSettings settings = ConstructSettings();
    settings.useSuperSampling = g_PreviewSSAA;

    // 1. Generate Atlas (For Export)
    // Always uses selected Unicode blocks
    {
        // Generate charset
        std::vector<uint32_t> charset = Utils::DecodeUtf8(g_CustomGlyphsText.c_str());
        
        // Remove duplicates
        std::sort(charset.begin(), charset.end());
        charset.erase(std::unique(charset.begin(), charset.end()), charset.end());
        
        // If no blocks selected, fallback to Basic Latin to prevent empty atlas errors
        // or confusion, unless intentional. Let's ensure at least ASCII is present if list empty.
        if (charset.empty()) {
             // Fallback to Basic Latin (ASCII)
             for (uint32_t c = 0x0020; c <= 0x007E; c++) {
                 charset.push_back(c);
             }
        }
        
        // Filter out excluded glyphs
        if (!g_ExcludedGlyphs.empty()) {
            std::vector<uint32_t> filtered;
            filtered.reserve(charset.size());
            for (uint32_t c : charset) {
                if (g_ExcludedGlyphs.find(c) == g_ExcludedGlyphs.end()) {
                    filtered.push_back(c);
                }
            }
            charset = filtered;
        }
        
        g_LastAtlas = TextureGenerator::GenerateAtlas(g_FontManager, charset, settings);
    }
    
    // Check for errors
    if (g_LastAtlas.hasErrors) {
        g_StatusMessage = g_LastAtlas.errorMessage;
        g_StatusTime = ImGui::GetTime();
        g_StatusIsError = true;
    }
    
    // Update Atlas Texture
    if (g_LastAtlas.width > 0 && g_LastAtlas.height > 0) {
        if (g_PreviewTexture) glDeleteTextures(1, &g_PreviewTexture);
        glGenTextures(1, &g_PreviewTexture);
        glBindTexture(GL_TEXTURE_2D, g_PreviewTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_LastAtlas.width, g_LastAtlas.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_LastAtlas.pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        g_PreviewWidth = g_LastAtlas.width;
        g_PreviewHeight = g_LastAtlas.height;
    } else {
        if (g_PreviewTexture) {
            glDeleteTextures(1, &g_PreviewTexture);
            g_PreviewTexture = 0;
        }
    }

    // 2. Generate Text Preview (Visual Only)
    // Always uses input text, linearly
    g_LastTextPreview = TextureGenerator::GenerateTextPreview(g_FontManager, std::string(text), settings);
    
    // Update Text Texture
    if (g_LastTextPreview.width > 0 && g_LastTextPreview.height > 0) {
        if (g_TextPreviewTexture) glDeleteTextures(1, &g_TextPreviewTexture);
        glGenTextures(1, &g_TextPreviewTexture);
        glBindTexture(GL_TEXTURE_2D, g_TextPreviewTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_LastTextPreview.width, g_LastTextPreview.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_LastTextPreview.pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        if (g_TextPreviewTexture) {
            glDeleteTextures(1, &g_TextPreviewTexture);
            g_TextPreviewTexture = 0;
        }
    }
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Case insensitive substring search moved to StringUtils/

int main(int, char**) {
    // ... (Init code remains same until loop) ...
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // Not strictly required for 3.0
#endif

    // Load Window Config from imgui.ini (geometry) and window.cfg (custom)
    int winW = 1280, winH = 720, winX = 100, winY = 100;
    Utils::LoadWindowConfig(winX, winY, winW, winH, g_PreviewSSAA);

    GLFWwindow* window = glfwCreateWindow(winW, winH, "Fnt Generator", NULL, NULL);
    if (window == NULL)
        return 1;
        
    Utils::SetWindowIcon(window);
        
    // Set Position if available
    glfwSetWindowPos(window, winX, winY);
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 

    // Load Fonts with CJK Support
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault()); // Latin
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull()); // Chinese
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese()); // Japanese
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic()); // Cyrillic
    builder.AddText("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+[]{}|;:',.<>/?`~"); // Ensure basics
    builder.BuildRanges(&ranges);

    // Try to load a font that covers most of these (Microsoft YaHei is good for CJK on Windows)
    // Fallback order: Microsoft YaHei -> Arial -> Default
    bool fontLoaded = false;
    
    #ifdef _WIN32
    const char* fontPath = "C:\\Windows\\Fonts\\msyh.ttc"; // Microsoft YaHei
    if (std::filesystem::exists(fontPath)) {
        io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, NULL, ranges.Data);
        fontLoaded = true;
    } else {
        fontPath = "C:\\Windows\\Fonts\\Arial.ttf";
        if (std::filesystem::exists(fontPath)) {
            io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, NULL, ranges.Data);
            fontLoaded = true;
        }
    }
    #endif

    if (!fontLoaded) {
        io.Fonts->AddFontDefault(); 
        // Note: Default font doesn't support CJK. 
        // If on Linux/Mac, user might see ???? for CJK unless we load a specific font there too.
    }
    io.Fonts->Build(); 

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialization
    if (!g_FontManager.Initialize()) {
        printf("Failed to init FontManager\n");
    }
    g_SystemFonts = Utils::GetSystemFonts();
    Utils::LoadFavorites(g_FavoriteFonts); // Load favorite fonts
    Utils::LoadRecentStyles(g_RecentStyles);
    g_CheckerTexture = Utils::CreateCheckerTexture();
    
    // Initialize custom glyphs from default blocks (e.g. Basic Latin)
    std::vector<uint32_t> initialCharset = GenerateCharsetFromBlocks(g_UnicodeBlocks);
    g_CustomGlyphsText = Utils::EncodeUtf8(initialCharset);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Main Menu
        float menuHeight = 0;
        if (ImGui::BeginMainMenuBar()) {
            menuHeight = ImGui::GetWindowSize().y;
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Import Project...")) {
                     std::string file = Utils::PickFileDialog("JSON Project\0*.json\0");
                     if (!file.empty()) {
                         LoadStyle(file);
                         Utils::AddRecentStyle(g_RecentStyles, file);
                     }
                }
                if (ImGui::MenuItem("Save Project...")) {
                     std::string file = Utils::SaveFileDialog("JSON Project\0*.json\0", "project.json");
                     if (!file.empty()) {
                         if(file.find(".json") == std::string::npos) file += ".json";
                         SaveStyle(file);
                         Utils::AddRecentStyle(g_RecentStyles, file);
                     }
                }
                
                ImGui::Separator();
                
                if (ImGui::BeginMenu("Recent Projects")) {
                    if(g_RecentStyles.empty()) {
                        ImGui::MenuItem("No recent files", nullptr, false, false);
                    } else {
                        std::vector<std::string> recentsCopy = g_RecentStyles;
                        for(const auto& s : recentsCopy) {
                            if(ImGui::MenuItem(s.c_str())) {
                                std::ifstream fcheck(s);
                                if(fcheck.good()) {
                                    LoadStyle(s);
                                    Utils::AddRecentStyle(g_RecentStyles, s);
                                } else {
                                    g_RecentNotFoundPath = s;
                                    g_ShowRecentError = true; 
                                }
                            }
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::MenuItem("Preferences...")) {
                    g_OpenPreferences = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Main Window
        {
            ImGui::SetNextWindowPos(ImVec2(0, menuHeight));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - menuHeight));
            ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            // Left Panel (Settings)
            ImGui::BeginChild("Settings", ImVec2(300, 0), true);
            
            ImGui::TextDisabled("Configuration");
            ImGui::Separator();


            // Font Selector with integrated search and favorites
            std::string previewName = "Select Font...";
            if (g_SelectedFontIndex >= 0 && g_SelectedFontIndex < g_SystemFonts.size()) {
                previewName = g_SystemFonts[g_SelectedFontIndex].name;
            }

            if (ImGui::BeginCombo("Font", previewName.c_str())) {
                // Search input inside the combo
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::IsWindowAppearing()) {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::InputTextWithHint("##FontSearch", "Search...", g_FontSearch, IM_ARRAYSIZE(g_FontSearch));
                
                ImGui::Separator();
                
                // Create sorted list: favorites first, then alphabetical
                std::vector<int> sortedIndices;
                for (int i = 0; i < g_SystemFonts.size(); i++) {
                    sortedIndices.push_back(i);
                }
                
                std::sort(sortedIndices.begin(), sortedIndices.end(), [](int a, int b) {
                    bool aFav = IsFavorite(g_SystemFonts[a].path);
                    bool bFav = IsFavorite(g_SystemFonts[b].path);
                    if (aFav != bFav) return aFav; // Favorites first
                    return g_SystemFonts[a].name < g_SystemFonts[b].name; // Then alphabetical
                });
                
                // Scrollable list of filtered fonts
                for (int idx : sortedIndices) {
                    // Filter logic
                    if (g_FontSearch[0] != '\0' && !Utils::StringContains(g_SystemFonts[idx].name, g_FontSearch)) {
                        continue;
                    }
                    
                    bool isFav = IsFavorite(g_SystemFonts[idx].path);
                    
                    // Star button with color
                    ImGui::PushID(idx);
                    if (isFav) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Yellow/Gold
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.25f, 0.0f, 0.5f));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Gray
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
                    }
                    
                    if (ImGui::SmallButton(isFav ? "[*]" : "[ ]")) {
                        ToggleFavorite(g_SystemFonts[idx].path);
                    }
                    
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                    
                    ImGui::SameLine();
                    
                    // Highlight favorite fonts in the list
                    if (isFav) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.8f, 1.0f)); // Slight yellow tint
                    }
                    
                    bool is_selected = (g_SelectedFontIndex == idx);
                    if (ImGui::Selectable(g_SystemFonts[idx].name.c_str(), is_selected)) {
                        g_SelectedFontIndex = idx;
                        g_FontManager.LoadFont(g_SystemFonts[idx].path);
                        UpdatePreview(g_InputText);
                        // Clear search when selecting
                        g_FontSearch[0] = '\0';
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                    
                    if (isFav) {
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::EndCombo();
            }

            // Input Text - Always visible for Text Preview
            if (ImGui::InputText("Text", g_InputText, IM_ARRAYSIZE(g_InputText))) {
                UpdatePreview(g_InputText);
            }
            
            // Unicode Blocks & Custom Glyphs Selection
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Unicode Blocks", ImGuiTreeNodeFlags_DefaultOpen)) {
                
                // Excluded Glyphs Management - if any
                if (!g_ExcludedGlyphs.empty()) {
                    char btnLabel[64];
                    sprintf(btnLabel, "Clear Excluded (%d)###ClearExcluded", (int)g_ExcludedGlyphs.size());
                    if (ImGui::Button(btnLabel)) {
                        g_ExcludedGlyphs.clear();
                        UpdatePreview(g_InputText);
                    }
                    ImGui::SameLine();
                }

                if (ImGui::Button("Reset All")) {
                    g_CustomGlyphsText = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!№;%:?*()_+-=.,/|\"'@#$^&{}[]";
                    for(auto& b : g_UnicodeBlocks) b.enabled = false;
                    UpdatePreview(g_InputText);
                }
                ImGui::SameLine();
                
                // Save/Load Presets
                ImVec2 pSave = ImGui::GetCursorScreenPos();
                if (ImGui::Button("##SavePreset", ImVec2(24, 24))) {
                    ImGui::OpenPopup("SavePresetPopup");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save Preset");
                
                // Floppy Icon
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                float x = pSave.x + 4, y = pSave.y + 4;
                ImU32 iconCol = ImGui::GetColorU32(ImGuiCol_Text);
                draw_list->AddRect(ImVec2(x, y), ImVec2(x+16, y+16), iconCol, 1.0f);
                draw_list->AddRectFilled(ImVec2(x+4, y), ImVec2(x+12, y+5), iconCol);
                draw_list->AddRectFilled(ImVec2(x+3, y+10), ImVec2(x+13, y+16), iconCol);

                ImGui::SameLine();
                ImVec2 pLoad = ImGui::GetCursorScreenPos();
                if (ImGui::Button("##LoadPreset", ImVec2(24, 24))) {
                    ImGui::OpenPopup("LoadPresetPopup");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load Preset");

                // Folder Icon
                x = pLoad.x + 4; y = pLoad.y + 4;
                draw_list->AddRectFilled(ImVec2(x, y+2), ImVec2(x+16, y+14), iconCol, 1.0f);
                draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x+6, y+3), iconCol, 1.0f);
                draw_list->AddLine(ImVec2(x, y+4), ImVec2(x+16, y+4), ImGui::GetColorU32(ImGuiCol_WindowBg), 1.0f);

                if (ImGui::BeginPopup("SavePresetPopup")) {
                    static char presetName[64] = "";
                    ImGui::InputText("Name", presetName, 64);
                    if (ImGui::Button("Save")) {
                    Utils::SaveCustomGlyphsPreset(presetName, g_CustomGlyphsText);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                if (ImGui::BeginPopup("LoadPresetPopup")) {
                    std::vector<std::string> presets = Utils::GetCustomGlyphsPresets();
                    if (presets.empty()) {
                        ImGui::TextDisabled("No presets found");
                    } else {
                        for (const auto& p : presets) {
                            if (ImGui::Selectable(p.c_str())) {
                                std::string content = Utils::LoadCustomGlyphsPreset(p);
                                if (!content.empty()) {
                                    g_CustomGlyphsText = content;
                                    UpdatePreview(g_InputText);
                                }
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::Text("Custom Glyphs:");
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
                if (ImGui::InputTextMultiline("##CustomGlyphs", (char*)g_CustomGlyphsText.c_str(), g_CustomGlyphsText.capacity() + 1, ImVec2(-1, 150), ImGuiInputTextFlags_CallbackResize, Utils::MyResizeCallback, (void*)&g_CustomGlyphsText)) {
                    UpdatePreview(g_InputText);
                }
                ImGui::PopStyleVar();
                ImGui::TextDisabled("(Newlines are ignored in atlas)");

                // Quick actions for blocks
                if (ImGui::Button("Select All Blocks")) {
                    std::set<uint32_t> charSet;
                    for(auto& b : g_UnicodeBlocks) {
                        b.enabled = true;
                        for(uint32_t cp = b.start; cp <= b.end; cp++) if(cp >= 32) charSet.insert(cp);
                    }
                    g_CustomGlyphsText = Utils::EncodeUtf8(std::vector<uint32_t>(charSet.begin(), charSet.end()));
                    UpdatePreview(g_InputText);
                }
                ImGui::SameLine();
                if (ImGui::Button("Deselect All Blocks")) {
                    for(auto& b : g_UnicodeBlocks) b.enabled = false;
                    g_CustomGlyphsText = "";
                    UpdatePreview(g_InputText);
                }
                
                ImGui::BeginChild("BlocksList", ImVec2(0, 150), true);
                for (auto& block : g_UnicodeBlocks) {
                    if (ImGui::Checkbox(block.name.c_str(), &block.enabled)) {
                        // 1. Get current characters
                        std::vector<uint32_t> currentChars = Utils::DecodeUtf8(g_CustomGlyphsText.c_str());
                        std::set<uint32_t> charSet(currentChars.begin(), currentChars.end());
                        
                        // 2. Identify characters that were "manually" added (don't belong to any defined block)
                        std::set<uint32_t> manualChars;
                        for (uint32_t cp : charSet) {
                            bool belongsToAnyBlock = false;
                            for (const auto& b : g_UnicodeBlocks) {
                                if (cp >= b.start && cp <= b.end) {
                                    belongsToAnyBlock = true;
                                    break;
                                }
                            }
                            if (!belongsToAnyBlock) manualChars.insert(cp);
                        }
                        
                        // 3. Rebuild the set from enabled blocks + manual additions
                        std::set<uint32_t> finalSet = manualChars;
                        for (const auto& b : g_UnicodeBlocks) {
                            if (b.enabled) {
                                for (uint32_t cp = b.start; cp <= b.end; cp++) {
                                    if (cp >= 32) finalSet.insert(cp);
                                }
                            }
                        }
                        
                        std::vector<uint32_t> nextChars(finalSet.begin(), finalSet.end());
                        g_CustomGlyphsText = Utils::EncodeUtf8(nextChars);
                        UpdatePreview(g_InputText);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("U+%04X - U+%04X", block.start, block.end);
                    }
                }
                ImGui::EndChild();
            }

            // Size with +/- buttons
            if (ImGui::DragInt("Size##FontSize", &g_FontSize, 0.5f, 8, 256)) {
                UpdatePreview(g_InputText);
            }
            ImGui::SameLine();
            if (ImGui::Button("-##FontSizeMinus")) {
                if (g_FontSize > 8) {
                    g_FontSize--;
                    UpdatePreview(g_InputText);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+##FontSizePlus")) {
                if (g_FontSize < 256) {
                    g_FontSize++;
                    UpdatePreview(g_InputText);
                }
            }

            // Fill
            ImGui::Separator();
            ImGui::Text("Fill");
            if (ImGui::Button("Reset Fill")) { 
                 g_EnableGradient = false; 
                 g_FillColor[0]=1.0f; g_FillColor[1]=1.0f; g_FillColor[2]=1.0f; g_FillColor[3]=1.0f;
                 UpdatePreview(g_InputText);
            }
            if (ImGui::Checkbox("Gradient", &g_EnableGradient)) {
                UpdatePreview(g_InputText);
            }
            if (!g_EnableGradient) {
                if (ImGui::ColorEdit4("Color", g_FillColor)) {
                    UpdatePreview(g_InputText);
                }
            } else {
                if (ImGui::Combo("Type##Fill", &g_FillGradientType, "Linear\0Radial\0")) UpdatePreview(g_InputText);
                if (Utils::KnobAngle("Angle##Fill", &g_FillGradientAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);
                
                ImGui::Dummy(ImVec2(0, 5));
                if (g_FillGradientWidget.widget("Fill Gradient")) {
                    UpdatePreview(g_InputText);
                }
                ImGui::Dummy(ImVec2(0, 10));
            }

            // Effects
            ImGui::Separator();
            if (g_EnableStroke) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.35f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
            }
            if (ImGui::CollapsingHeader("Effects: Outline")) {
                ImGui::PopStyleColor(3);
                if (ImGui::Checkbox("Enable Outline", &g_EnableStroke)) {
                    UpdatePreview(g_InputText);
                }
                if (g_EnableStroke) {
                    if (ImGui::SliderFloat("Width", &g_StrokeWidth, 0.0f, 10.0f)) UpdatePreview(g_InputText);
                if (ImGui::Combo("Position##Stroke", &g_StrokePosition, "Outside\0Center\0Inside\0")) UpdatePreview(g_InputText);
                if (ImGui::Checkbox("Stroke Gradient", &g_EnableStrokeGradient)) UpdatePreview(g_InputText);
                    
                    if (!g_EnableStrokeGradient) {
                         if (ImGui::ColorEdit4("Line Color", g_StrokeColor)) UpdatePreview(g_InputText);
                    } else {
                         if (ImGui::Combo("Type##Stroke", &g_StrokeGradientType, "Linear\0Radial\0")) UpdatePreview(g_InputText);
                         if (Utils::KnobAngle("Angle##Stroke", &g_StrokeGradientAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);

                         ImGui::Dummy(ImVec2(0, 5));
                         if (g_StrokeGradientWidget.widget("Stroke Gradient")) {
                             UpdatePreview(g_InputText);
                         }
                         ImGui::Dummy(ImVec2(0, 10));
                    }
                }
            } else {
                ImGui::PopStyleColor(3);
            }

            ImGui::Separator();
            if (g_EnableShadow) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.35f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
            }
            if (ImGui::CollapsingHeader("Effects: Shadow")) {
                ImGui::PopStyleColor(3);
                if (ImGui::Checkbox("Enable Shadow", &g_EnableShadow)) {
                    UpdatePreview(g_InputText);
                }
                if (g_EnableShadow) {
                    if (ImGui::SliderInt("Offset X##Shadow", &g_ShadowOffsetX, -20, 20)) UpdatePreview(g_InputText);
                    if (ImGui::SliderInt("Offset Y##Shadow", &g_ShadowOffsetY, -20, 20)) UpdatePreview(g_InputText);
                    if (ImGui::SliderInt("Blur##Shadow", &g_ShadowBlur, 0, 10)) UpdatePreview(g_InputText);
                    if (ImGui::ColorEdit4("Color##Shadow", g_ShadowColor)) UpdatePreview(g_InputText);
                }
            } else {
                ImGui::PopStyleColor(3);
            }
            
            if (g_EnableBevel) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.35f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
            }
            if (ImGui::CollapsingHeader("Effects: Bevel")) {
                ImGui::PopStyleColor(3);
                if (ImGui::Checkbox("Enabled##Bevel", &g_EnableBevel)) {
                    UpdatePreview(g_InputText);
                }
                if (g_EnableBevel) {
                    if (ImGui::SliderFloat("Distance##Bevel", &g_BevelDistance, 0.0f, 20.0f)) UpdatePreview(g_InputText);
                    if (Utils::KnobAngle("Angle##Bevel", (float*)&g_BevelAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);
                    if (ImGui::SliderFloat("Spread##Bevel", &g_BevelSpread, 0.0f, 20.0f)) UpdatePreview(g_InputText);
                    if (ImGui::SliderFloat("Strength##Bevel", &g_BevelStrength, 0.0f, 10.0f)) UpdatePreview(g_InputText);
                    
                    const char* bevelTypes[] = { "Inner", "Outer" };
                    if (ImGui::Combo("Type##Bevel", &g_BevelType, bevelTypes, IM_ARRAYSIZE(bevelTypes))) UpdatePreview(g_InputText);

                    if (ImGui::ColorEdit4("Highlight color##Bevel", g_BevelHighlightColor)) UpdatePreview(g_InputText);
                    if (ImGui::ColorEdit4("Shadow color##Bevel", g_BevelShadowColor)) UpdatePreview(g_InputText);
                }
            } else {
                ImGui::PopStyleColor(3);
            }
            
            if (g_EnableInnerGlow) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.35f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
            }
            if (ImGui::CollapsingHeader("Effects: Inner Glow")) {
                ImGui::PopStyleColor(3);
                if (ImGui::Checkbox("Enable Inner Glow", &g_EnableInnerGlow)) {
                    UpdatePreview(g_InputText);
                }
                if (g_EnableInnerGlow) {
                    if (ImGui::SliderFloat("Size##InnerGlowSize", &g_InnerGlowSize, 0.0f, 20.0f)) UpdatePreview(g_InputText);
                    if (ImGui::SliderFloat("Choke##InnerGlow", &g_InnerGlowChoke, 0.0f, 100.0f)) UpdatePreview(g_InputText);
                    if (ImGui::ColorEdit4("Color##InnerGlow", g_EnableInnerGlow ? g_InnerGlowColor : g_InnerGlowColor)) UpdatePreview(g_InputText);
                }
            } else {
                ImGui::PopStyleColor(3);
            }

            // Preview Info & Atlas Settings
            ImGui::Separator();
            ImGui::Text("Atlas Size");
            const char* atlasSizes[] = { "Auto", "256", "512", "1024", "2048", "4096", "8192" };
            
            auto GetSizeIdx = [&](int size) {
                if (size <= 0) return 0;
                for (int i = 1; i < IM_ARRAYSIZE(atlasSizes); i++) {
                    if (atoi(atlasSizes[i]) == size) return i;
                }
                return 3; // Default 1024
            };

            int curWIdx = GetSizeIdx(g_AtlasWidth);
            int curHIdx = GetSizeIdx(g_AtlasHeight);

            ImGui::SetNextItemWidth(100);
            if (ImGui::Combo("W##Atlas", &curWIdx, atlasSizes, IM_ARRAYSIZE(atlasSizes))) {
                g_AtlasWidth = (curWIdx == 0) ? 0 : atoi(atlasSizes[curWIdx]);
                UpdatePreview(g_InputText);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            if (ImGui::Combo("H##Atlas", &curHIdx, atlasSizes, IM_ARRAYSIZE(atlasSizes))) {
                g_AtlasHeight = (curHIdx == 0) ? 0 : atoi(atlasSizes[curHIdx]);
                UpdatePreview(g_InputText);
            }

            // Quality Settings
            if (ImGui::Checkbox("High Quality (SSAA 2x)", &g_PreviewSSAA)) {
                UpdatePreview(g_InputText);
            }
            // Hinting Selector
            const char* hintingModes[] = { "Smooth (No Hinting)", "Sharp (Auto Hinting)", "Crisp (Normal Hinting)" };
            if (ImGui::Combo("Hinting", &g_HintingMode, hintingModes, IM_ARRAYSIZE(hintingModes))) {
                UpdatePreview(g_InputText);
            }
            
            // Preview Info
            ImGui::Text("Dims: %d x %d", g_PreviewWidth, g_PreviewHeight);
            
            ImGui::Separator();
            ImGui::Separator();
            ImGui::Text("Char Adjustments");
            if (ImGui::DragInt("xAdvance##Global", &g_GlobalXAdvance, 1, -100, 100)) UpdatePreview(g_InputText);
            if (ImGui::DragInt("xOffset##Global", &g_GlobalXOffset, 1, -100, 100)) UpdatePreview(g_InputText);
            if (ImGui::DragInt("yOffset##Global", &g_GlobalYOffset, 1, -100, 100)) UpdatePreview(g_InputText);

            ImGui::Separator();
            ImGui::Text("Export Settings");
            
            ImGui::Text("Format:");
            const char* formats[] = { ".fnt (BMFont XML)", ".fnt (BMFont Text)", ".xml (BMFont XML)", ".txt (BMFont Text)", ".fnt (BMFont Binary)" };
            ImGui::Combo("##Format", &g_ExportFormat, formats, IM_ARRAYSIZE(formats));
            
            ImGui::InputText("Filename", g_ExportFilename, IM_ARRAYSIZE(g_ExportFilename));
            
            // Background Control
            static bool bgTransparent = true;
            if (ImGui::Checkbox("Transparent BG", &bgTransparent)) {
                 // Logic handled in drawing
            }
            if (!bgTransparent) {
                ImGui::ColorEdit3("Preview BG", g_PreviewBgColor);
                // Ensure Alpha is 1.0
                g_PreviewBgColor[3] = 1.0f; 
            } else {
                g_PreviewBgColor[3] = 0.0f; // Transparent
            }

            // Export
            if (ImGui::Button("Export (Select Folder)", ImVec2(-1, 40))) {
                if (g_LastAtlas.width > 0) {
                    std::string folder = Utils::PickFolderDialog();
                    if (!folder.empty()) {
                         g_StatusMessage = "Generating High Quality Atlas...";
                         
                         AtlasSettings hqSettings = ConstructSettings();
                         hqSettings.useSuperSampling = true;
                         hqSettings.hintingMode = g_HintingMode;

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

                         if (!hqAtlas.hasErrors && hqAtlas.width > 0) {
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

                            if (Exporter::ExportAtlasToDisk(hqAtlas, folder, fname, formatEnum, ext)) {
                                 g_StatusMessage = "Export Success (HQ)!";
                                 g_StatusTime = ImGui::GetTime();
                                 g_StatusIsError = false;
                                 
                                 // Optional: Update preview with the HQ version
                                 g_LastAtlas = hqAtlas;
                                 if(g_PreviewTexture) glDeleteTextures(1, &g_PreviewTexture);
                                 glGenTextures(1, &g_PreviewTexture);
                                  glBindTexture(GL_TEXTURE_2D, g_PreviewTexture);
                                 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                                 glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_LastAtlas.width, g_LastAtlas.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_LastAtlas.pixels.data());
                                 g_PreviewWidth = g_LastAtlas.width;
                                 g_PreviewHeight = g_LastAtlas.height;
                                 
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
                } else {
                    g_StatusMessage = "Nothing to export!";
                    g_StatusTime = ImGui::GetTime();
                    g_StatusIsError = true;
                }
            }
            
            if (ImGui::GetTime() - g_StatusTime < 5.0) {
                ImVec4 color = g_StatusIsError ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                ImGui::TextColored(color, "%s", g_StatusMessage.c_str());
            }

            ImGui::EndChild();
            
            // Right Panel (Canvas)
            ImGui::SameLine();
            
            // Parent Container for Right Side
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0)); 
            ImGui::BeginChild("RightPanelContainer", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
            {
                // Get available height
                float totalH = ImGui::GetContentRegionAvail().y;
                float topH = totalH * 0.3f; // 30% for Text Preview
                if (topH < 150.0f) topH = 150.0f; // Minimal height
                float bottomH = totalH - topH - 5.0f; // Remaining
                
                // 1. Text Preview Region
                ImGui::BeginChild("TextPreviewRegion", ImVec2(0, topH), true, ImGuiWindowFlags_NoMove);
                {
                    // Toolbar
                    ImGui::TextDisabled("Preview Text");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
                    ImGui::Text("Scale: %d%%", (int)(g_TextZoom * 100));
                    ImGui::SameLine();
                    if (ImGui::SmallButton("1:1##Text")) { g_TextZoom = 1.0f; g_TextPan = { 0,0 }; }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Fit##Text")) {
                        float fitW = ImGui::GetContentRegionAvail().x / (float)g_LastTextPreview.width;
                        float fitH = ImGui::GetContentRegionAvail().y / (float)g_LastTextPreview.height;
                        g_TextZoom = std::min(fitW, fitH);
                        if (g_TextZoom > 1.0f) g_TextZoom = 1.0f;
                        g_TextPan = { 0,0 };
                    }
                     
                    ImVec2 p_min = ImGui::GetCursorScreenPos();
                    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
                    ImVec2 p_max = ImVec2(p_min.x + canvas_sz.x, p_min.y + canvas_sz.y);
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();

                    // Masking region for drawing
                    ImGui::PushClipRect(p_min, p_max, true);

                    // BG Checkered
                    if (bgTransparent && g_CheckerTexture) {
                         draw_list->AddImage((void*)(intptr_t)g_CheckerTexture, p_min, p_max, ImVec2(0,0), ImVec2(canvas_sz.x / 32.0f, canvas_sz.y / 32.0f));
                    } else if (!bgTransparent) {
                         draw_list->AddRectFilled(p_min, p_max, ImColor(g_PreviewBgColor[0], g_PreviewBgColor[1], g_PreviewBgColor[2], 1.0f));
                    }

                    // Interaction & Display
                    if (g_TextPreviewTexture) {
                        // Zoom via Scroll
                        if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0) {
                            float zoomStep = 0.1f * g_TextZoom;
                            g_TextZoom += ImGui::GetIO().MouseWheel * zoomStep;
                            if (g_TextZoom < 0.05f) g_TextZoom = 0.05f;
                            if (g_TextZoom > 20.0f) g_TextZoom = 20.0f;
                        }

                        float imgW = g_LastTextPreview.width * g_TextZoom;
                        float imgH = g_LastTextPreview.height * g_TextZoom;

                        // Center by default if smaller than canvas
                        ImVec2 offset = g_TextPan;
                        if (imgW < canvas_sz.x) offset.x = (canvas_sz.x - imgW) * 0.5f;
                        if (imgH < canvas_sz.y) offset.y = (canvas_sz.y - imgH) * 0.5f;

                        ImVec2 imgPos = ImVec2(p_min.x + offset.x, p_min.y + offset.y);
                        draw_list->AddImage((void*)(intptr_t)g_TextPreviewTexture, imgPos, ImVec2(imgPos.x + imgW, imgPos.y + imgH));

                        // Panning (Right Click)
                        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            g_IsPanningText = true;
                        }
                        if (g_IsPanningText) {
                            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                                g_IsPanningText = false;
                            } else {
                                ImVec2 delta = ImGui::GetIO().MouseDelta;
                                g_TextPan.x += delta.x;
                                g_TextPan.y += delta.y;
                            }
                        }
                    } else {
                        ImGui::Text("Preview generation failed or empty.");
                    }
                    ImGui::PopClipRect();
                }
                ImGui::EndChild();

                // 2. Atlas Preview Region (Remaining Height)
                ImGui::BeginChild("AtlasPreviewRegion", ImVec2(0, bottomH), true, ImGuiWindowFlags_NoMove);
                {
                    // Toolbar
                    ImGui::TextDisabled("Atlas Preview");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
                    ImGui::Text("Scale: %d%%", (int)(g_AtlasZoom * 100));
                    ImGui::SameLine();
                    if (ImGui::SmallButton("1:1")) { g_AtlasZoom = 1.0f; g_AtlasPan = {0,0}; }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Fit")) {
                        float fitW = ImGui::GetContentRegionAvail().x / (float)g_PreviewWidth;
                        float fitH = ImGui::GetContentRegionAvail().y / (float)g_PreviewHeight;
                        g_AtlasZoom = std::min(fitW, fitH);
                        if (g_AtlasZoom > 1.0f) g_AtlasZoom = 1.0f;
                        g_AtlasPan = {0,0};
                    }

                    ImVec2 p_min = ImGui::GetCursorScreenPos();
                    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
                    ImVec2 p_max = ImVec2(p_min.x + canvas_sz.x, p_min.y + canvas_sz.y);
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();

                    // Masking region for drawing
                    ImGui::PushClipRect(p_min, p_max, true);

                    // BG Checkered
                    if (bgTransparent && g_CheckerTexture) {
                         draw_list->AddImage((void*)(intptr_t)g_CheckerTexture, p_min, p_max, ImVec2(0,0), ImVec2(canvas_sz.x / 32.0f, canvas_sz.y / 32.0f));
                    } else if (!bgTransparent) {
                         draw_list->AddRectFilled(p_min, p_max, ImColor(g_PreviewBgColor[0], g_PreviewBgColor[1], g_PreviewBgColor[2], 1.0f));
                    }

                    // Atlas Interaction & Display
                    if (g_PreviewTexture) {
                        // Zoom via Scroll
                        if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0) {
                            float zoomStep = 0.1f * g_AtlasZoom;
                            g_AtlasZoom += ImGui::GetIO().MouseWheel * zoomStep;
                            if (g_AtlasZoom < 0.05f) g_AtlasZoom = 0.05f;
                            if (g_AtlasZoom > 20.0f) g_AtlasZoom = 20.0f;
                        }

                        float imgW = g_PreviewWidth * g_AtlasZoom;
                        float imgH = g_PreviewHeight * g_AtlasZoom;

                        // Center by default if smaller than canvas
                        ImVec2 offset = g_AtlasPan;
                        if (imgW < canvas_sz.x) offset.x = (canvas_sz.x - imgW) * 0.5f;
                        if (imgH < canvas_sz.y) offset.y = (canvas_sz.y - imgH) * 0.5f;

                        ImVec2 imgPos = ImVec2(p_min.x + offset.x, p_min.y + offset.y);
                        draw_list->AddImage((void*)(intptr_t)g_PreviewTexture, imgPos, ImVec2(imgPos.x + imgW, imgPos.y + imgH));

                        // 3. Out-of-Atlas Highlight (Logical border)
                        float logicalW = g_LastAtlas.atlasWidth * g_AtlasZoom;
                        float logicalH = g_LastAtlas.atlasHeight * g_AtlasZoom;
                        draw_list->AddRect(imgPos, ImVec2(imgPos.x + logicalW, imgPos.y + logicalH), IM_COL32(255, 255, 0, 150), 0, 0, 2.0f);

                        // Panning
                        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            g_IsPanning = true;
                        }
                        if (g_IsPanning) {
                            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                                g_IsPanning = false;
                            } else {
                                ImVec2 delta = ImGui::GetIO().MouseDelta;
                                g_AtlasPan.x += delta.x;
                                g_AtlasPan.y += delta.y;
                            }
                        }

                        // Glyph Hover logic
                        ImVec2 mousePos = ImGui::GetMousePos();
                        if (ImGui::IsWindowHovered() &&
                            mousePos.x >= imgPos.x && mousePos.x < imgPos.x + imgW &&
                            mousePos.y >= imgPos.y && mousePos.y < imgPos.y + imgH) 
                        {
                            int atlasX = (int)((mousePos.x - imgPos.x) / g_AtlasZoom);
                            int atlasY = (int)((mousePos.y - imgPos.y) / g_AtlasZoom);
                            
                            int hoveredIndex = -1;
                            for (size_t i = 0; i < g_LastAtlas.glyphs.size(); i++) {
                                const auto& g = g_LastAtlas.glyphs[i];
                                if (atlasX >= g.x && atlasX < g.x + g.width &&
                                    atlasY >= g.y && atlasY < g.y + g.height) {
                                    hoveredIndex = (int)i;
                                    break;
                                }
                            }
                            
                            if (hoveredIndex != -1) {
                                const auto& g = g_LastAtlas.glyphs[hoveredIndex];
                                float sX = imgPos.x + g.x * g_AtlasZoom;
                                float sY = imgPos.y + g.y * g_AtlasZoom;
                                float sW = g.width * g_AtlasZoom;
                                float sH = g.height * g_AtlasZoom;
                                draw_list->AddRect(ImVec2(sX, sY), ImVec2(sX + sW, sY + sH), IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
                                
                                ImGui::BeginTooltip();
                                ImGui::Text("Char: %c (U+%04X)", (char)g.charCode, g.charCode);
                                ImGui::Text("Size: %dx%d", g.width, g.height);
                                ImGui::Text("Pos: %d, %d", g.x, g.y);
                                if (g.y >= g_LastAtlas.atlasHeight) ImGui::TextColored(ImVec4(1,0.5f,0.5f,1), "OUT OF ATLAS BOUNDS");
                                ImGui::EndTooltip();
                                
                                if (ImGui::IsMouseClicked(0)) {
                                    g_SelectedGlyphIndex = hoveredIndex;
                                    ImGui::OpenPopup("GlyphContext");
                                }
                            }
                        }
                        
                        if (ImGui::BeginPopup("GlyphContext")) {
                            if (g_SelectedGlyphIndex >= 0 && g_SelectedGlyphIndex < (int)g_LastAtlas.glyphs.size()) {
                                const auto& g = g_LastAtlas.glyphs[g_SelectedGlyphIndex];
                                ImGui::Text("Selected: U+%04X", g.charCode);
                                ImGui::Separator();
                                 if (ImGui::MenuItem("Remove from Atlas")) {
                                     std::vector<uint32_t> chars = Utils::DecodeUtf8(g_CustomGlyphsText.c_str());
                                     std::vector<uint32_t> filtered;
                                     for(uint32_t c : chars) if(c != g.charCode) filtered.push_back(c);
                                     g_CustomGlyphsText = Utils::EncodeUtf8(filtered);
                                     
                                     UpdatePreview(g_InputText);
                                     g_SelectedGlyphIndex = -1;
                                 }
                            }
                            ImGui::EndPopup();
                        }
                    } else {
                        ImGui::Text("Atlas not generated.");
                    }
                    ImGui::PopClipRect();
                }
                ImGui::EndChild();
            }
            ImGui::EndChild(); // End RightPanelContainer
            ImGui::PopStyleColor();

            ImGui::End();
        }

        // Handle Recent File Error Popup
        if (g_ShowRecentError) {
             ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
             ImGui::OpenPopup("RecentNotFound");
             g_ShowRecentError = false; 
        }
        if (ImGui::BeginPopupModal("RecentNotFound", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("File not found:\n%s", g_RecentNotFoundPath.c_str());
            ImGui::Separator();
            if (ImGui::Button("Remove from Recents", ImVec2(160, 0))) {
                Utils::RemoveRecentStyle(g_RecentStyles, g_RecentNotFoundPath);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Handle Preferences Popup
        if (g_OpenPreferences) {
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::OpenPopup("PreferencesPopup");
            g_OpenPreferences = false;
        }
        if (ImGui::BeginPopupModal("PreferencesPopup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Application Preferences");
            ImGui::Separator();
            
            if (ImGui::Checkbox("Enable High Quality Preview (SSAA)", &g_PreviewSSAA)) {
                UpdatePreview(g_InputText); // Refresh immediately
            }
            ImGui::SameLine(); 
            ImGui::TextDisabled("(May affect performance)");
            
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }



    int curW, curH;
    int curX, curY;
    glfwGetWindowSize(window, &curW, &curH);
    glfwGetWindowPos(window, &curX, &curY);
    Utils::SaveWindowConfig(curX, curY, curW, curH, g_PreviewSSAA);

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#ifdef _WIN32
#include <windows.h>
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
#endif
