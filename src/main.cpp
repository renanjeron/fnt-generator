#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <imgui_gradient/imgui_gradient.hpp>
#include <stdio.h>

#include <cmath>
#include <vector>
#include "Utils/PatLoader.h"
#include <cstdint>
#include <future>
#include <mutex>
#include <mutex>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <GLFW/glfw3.h>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
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
#include "Utils/FontPreviewUtils.h"
#include "UI/FontInfoDialog.h"
#include "UI/ExportDialog.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef APP_VERSION
#define APP_VERSION "1.1.0-dev"
#endif



// --- State Variables ---
static std::vector<Utils::FontInfo> g_SystemFonts;
FontManager g_FontManager;
static int g_SelectedFontIndex = -1;
static char g_InputText[1024] = "Hello Everyone";
static int g_FontSize = 72;
static int g_Padding = 5;

// Atlas Size
static int g_AtlasWidth = 0;
static int g_AtlasHeight = 0;
static bool g_AllowMultiPage = false;

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
static int g_StrokeJoinStyle = 1; // 0=Bevel, 1=Miter, 2=Round
static float g_StrokeMiterLimit = 2.0f;

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
static float g_InnerGlowSize = 0.0f;
static float g_InnerGlowChoke = 0.0f;
static float g_InnerGlowColor[4] = { 1.0f, 1.0f, 1.0f, 0.5f }; // RGBA
static int g_InnerGlowBlendMode = 0; // 0 = Normal

// Pattern
static bool g_EnablePattern = false;

// Font Preview
static bool g_ShowFontPreview = false;
static std::string g_PatternPath = "";
static std::vector<PatImage> g_LoadedPatterns;
static bool g_ShowPatternSelector = false;
static std::vector<GLuint> g_PatternThumbnails;
static float g_PatternOpacity = 1.0f;
static float g_PatternAngle = 0.0f;
static float g_PatternScale = 1.0f;
static int g_PatternBlendMode = 0; // BlendMode::Normal
static int g_PatternMappingMode = 0; // 0=Glyph, 1=Global
static std::string g_OriginalPatternPath = "";
static int g_SelectedPatternIndex = -1;
static bool g_ShowMissingPatternDialog = false;
static std::string g_MissingPatternPath = "";
static bool g_RequestPatternPopup = false; 
static GLuint g_PatternPreviewTexture = 0;

static char g_FontSearch[128] = ""; // Search filter
// --- State Variables ---
// ... (Previous variables)
float g_PreviewBgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // UI Canvas Background
float g_ExportBgColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // Exported File Background
char g_ExportFilename[128] = "my_font";
std::string g_ExportPath = "";
int g_ExportFormat = 0; // 0=XML, 1=Text, 2=Binary
static bool g_UseExtendedCharset = false;
static bool g_UseCustomGlyphs = true; // Always true now
std::string g_CustomGlyphsText = "";
static std::vector<UnicodeBlock> g_UnicodeBlocks = UNICODE_BLOCKS; // Mutable copy
static GLuint g_CheckerTexture = 0;
// Atlas Interaction
std::set<uint32_t> g_ExcludedGlyphs;
int g_SSAAFactor = 1; // 1=None, 2=2x, 4=4x
int g_HintingMode = 0; // 0 = Smooth (No Hinting), 1 = Sharp (Auto), 2 = Crisp (Normal)
static bool g_OpenPreferences = false;
static int g_SelectedGlyphIndex = -1;
static float g_AtlasZoom = 1.0f;
static ImVec2 g_AtlasPan = { 0, 0 };
static bool g_IsPanning = false;
static bool g_AtlasUpdatePending = false;
static double g_LastInteractionTime = 0.0;
const double g_AtlasDebounceTime = 0.8; // Seconds to wait before heavy atlas update
static bool g_IsGeneratingAtlas = false;
static std::future<AtlasResult> g_AtlasFuture;

static float g_TextZoom = 1.0f;
static ImVec2 g_TextPan = { 0, 0 };
static bool g_IsPanningText = false;

// Char Adjustments
static int g_GlobalXAdvance = 0;
static int g_GlobalXOffset = 0;
static int g_GlobalYOffset = 0;
// Kerning
bool g_EnableKerning = true;

// Favorite fonts system
static std::set<std::string> g_FavoriteFonts;
// Recent Styles
static std::vector<std::string> g_RecentStyles;
static bool g_ShowRecentError = false;
static std::string g_RecentNotFoundPath = "";



// Simple Fingerprint to detect actual changes in gradients
struct GradientFingerprint {
    struct Mark {
        float pos;
        float r, g, b, a;
    };
    std::vector<Mark> marks;
    bool operator!=(const GradientFingerprint& o) const {
        if (marks.size() != o.marks.size()) return true;
        for (size_t i = 0; i < marks.size(); i++) {
            if (marks[i].pos != o.marks[i].pos || marks[i].r != o.marks[i].r || 
                marks[i].g != o.marks[i].g || marks[i].b != o.marks[i].b || marks[i].a != o.marks[i].a) return true;
        }
        return false;
    }
};

static GradientFingerprint GetFingerprint(const ImGG::GradientWidget& w) {
    GradientFingerprint fp;
    for (const auto& m : w.gradient().get_marks()) {
        fp.marks.push_back({ m.position.get(), m.color.x, m.color.y, m.color.z, m.color.w });
    }
    return fp;
}

static GradientFingerprint g_LastFillFp;
static GradientFingerprint g_LastStrokeFp;


AtlasSettings ConstructSettings();
void UpdatePreview(const char* text); // Forward checking

// Header to update the UI texture for pattern preview (Defined here to avoid prototype issues)
void UpdatePatternPreviewTexture() {
    if (g_PatternPath.empty()) return;
    
    std::vector<uint8_t> pixels;
    int w, h;
    if (BitmapUtils::GetPatternPixels(g_PatternPath, pixels, w, h)) {
        if (g_PatternPreviewTexture != 0) {
            glDeleteTextures(1, &g_PatternPreviewTexture);
            g_PatternPreviewTexture = 0;
        }
        
        glGenTextures(1, &g_PatternPreviewTexture);
        glBindTexture(GL_TEXTURE_2D, g_PatternPreviewTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    } else {
        if (g_PatternPreviewTexture != 0) {
            glDeleteTextures(1, &g_PatternPreviewTexture);
            g_PatternPreviewTexture = 0;
        }
    }
}

// Implementation of Style Save/Load (simplified)
static void SaveStyle(const std::string& path) {
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
    out << "  \"strokeJoinStyle\": " << g_StrokeJoinStyle << ",\n";
    out << "  \"strokeMiterLimit\": " << g_StrokeMiterLimit << ",\n";
    
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
    out << "  \"innerGlowBlendMode\": " << g_InnerGlowBlendMode << ",\n";
    
    out << "  \"enableBevel\": " << (g_EnableBevel ? "true" : "false") << ",\n";
    out << "  \"bevelDistance\": " << g_BevelDistance << ",\n";
    out << "  \"bevelAngle\": " << g_BevelAngle << ",\n";
    out << "  \"bevelSpread\": " << g_BevelSpread << ",\n";
    out << "  \"bevelStrength\": " << g_BevelStrength << ",\n";
    out << "  \"bevelType\": " << g_BevelType << ",\n";
    out << "  \"bevelHighlightColor\": [" << g_BevelHighlightColor[0] << ", " << g_BevelHighlightColor[1] << ", " << g_BevelHighlightColor[2] << ", " << g_BevelHighlightColor[3] << "],\n";
    out << "  \"bevelShadowColor\": [" << g_BevelShadowColor[0] << ", " << g_BevelShadowColor[1] << ", " << g_BevelShadowColor[2] << ", " << g_BevelShadowColor[3] << "],\n";
    
    // Pattern
    out << "  \"enablePattern\": " << (g_EnablePattern ? "true" : "false") << ",\n";
    {
        std::string pathAndIndex = ""; // Not used, just logic
        std::string savePath = !g_OriginalPatternPath.empty() ? g_OriginalPatternPath : g_PatternPath;
        
        std::string escapedPath;
        for(char c : savePath) {
            if(c == '\\') escapedPath += "\\\\";
            else escapedPath += c;
        }
        out << "  \"patternPath\": \"" << escapedPath << "\",\n";
        out << "  \"patternIndex\": " << g_SelectedPatternIndex << ",\n";
    }
    out << "  \"patternOpacity\": " << g_PatternOpacity << ",\n";
    out << "  \"patternAngle\": " << g_PatternAngle << ",\n";
    out << "  \"patternScale\": " << g_PatternScale << ",\n";
    out << "  \"patternBlendMode\": " << g_PatternBlendMode << ",\n";
    out << "  \"patternMappingMode\": " << g_PatternMappingMode << ",\n";

    out << "  \"exportFilename\": \"" << g_ExportFilename << "\",\n";
    {
        std::string escaped;
        for(char c : g_ExportPath) {
            if(c == '\\') escaped += "\\\\";
            else escaped += c;
        }
        out << "  \"exportPath\": \"" << escaped << "\",\n";
    }
    out << "  \"ssaaFactor\": " << g_SSAAFactor << ",\n";
    out << "  \"hintingMode\": " << g_HintingMode << ",\n";
    
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
    Utils::ParseGradientStops(c, "fillGradientStops", g_FillGradientWidget);
    
    g_EnableStroke = Utils::ParseBoolValue(c, "enableStroke", false);
    g_StrokeWidth = Utils::ParseFloatValue(c, "strokeWidth", 2.0f);
    Utils::ParseColor4(c, "strokeColor", g_StrokeColor);
    g_StrokeJoinStyle = Utils::ParseIntValue(c, "strokeJoinStyle", 1);
    g_StrokeMiterLimit = Utils::ParseFloatValue(c, "strokeMiterLimit", 2.0f);
    
    g_EnableStrokeGradient = Utils::ParseBoolValue(c, "enableStrokeGradient", false);
    g_StrokeGradientType = Utils::ParseIntValue(c, "strokeGradientType", 0);
    g_StrokeGradientAngle = Utils::ParseFloatValue(c, "strokeGradientAngle", 90.0f);
    Utils::ParseGradientStops(c, "strokeGradientStops", g_StrokeGradientWidget);
    
    g_EnableShadow = Utils::ParseBoolValue(c, "enableShadow", g_EnableShadow);
    g_ShadowOffsetX = Utils::ParseIntValue(c, "shadowOffsetX", g_ShadowOffsetX);
    g_ShadowOffsetY = Utils::ParseIntValue(c, "shadowOffsetY", g_ShadowOffsetY);
    g_ShadowBlur = Utils::ParseIntValue(c, "shadowBlur", g_ShadowBlur);
    Utils::ParseColor4(c, "shadowColor", g_ShadowColor);
 
    g_EnableInnerGlow = Utils::ParseBoolValue(c, "enableInnerGlow", g_EnableInnerGlow);
    g_InnerGlowSize = Utils::ParseFloatValue(c, "innerGlowSize", g_InnerGlowSize);
    g_InnerGlowChoke = Utils::ParseFloatValue(c, "innerGlowChoke", g_InnerGlowChoke);
    Utils::ParseColor4(c, "innerGlowColor", g_InnerGlowColor);
    g_InnerGlowBlendMode = Utils::ParseIntValue(c, "innerGlowBlendMode", 0);

    g_EnableBevel = Utils::ParseBoolValue(c, "enableBevel", g_EnableBevel);
    g_BevelDistance = Utils::ParseFloatValue(c, "bevelDistance", g_BevelDistance);
    g_BevelAngle = Utils::ParseIntValue(c, "bevelAngle", g_BevelAngle);
    g_BevelSpread = Utils::ParseFloatValue(c, "bevelSpread", g_BevelSpread);
    g_BevelStrength = Utils::ParseFloatValue(c, "bevelStrength", g_BevelStrength);
    g_BevelType = Utils::ParseIntValue(c, "bevelType", g_BevelType);
    Utils::ParseColor4(c, "bevelHighlightColor", g_BevelHighlightColor);
    Utils::ParseColor4(c, "bevelShadowColor", g_BevelShadowColor);

    // Pattern
    g_EnablePattern = Utils::ParseBoolValue(c, "enablePattern", false);
    std::string pPath = Utils::ParseStringValue(c, "patternPath");
    int pIndex = Utils::ParseIntValue(c, "patternIndex", -1);
    
    g_PatternOpacity = Utils::ParseFloatValue(c, "patternOpacity", 1.0f);
    g_PatternAngle = Utils::ParseFloatValue(c, "patternAngle", 0.0f);
    g_PatternScale = Utils::ParseFloatValue(c, "patternScale", 1.0f);
    g_PatternBlendMode = Utils::ParseIntValue(c, "patternBlendMode", 0);
    g_PatternMappingMode = Utils::ParseIntValue(c, "patternMappingMode", 0);
    
    // Pattern Restoration Logic
    if (g_EnablePattern && !pPath.empty()) {
        g_SelectedPatternIndex = pIndex; // Preserve index even if missing
        
        if (!std::filesystem::exists(pPath)) {
             g_MissingPatternPath = pPath;
             g_ShowMissingPatternDialog = true;
             // Keep enabled but path empty? Or disable?
             // Keep enabled so user sees they need to fix it.
             g_PatternPath = ""; 
        } else {
             g_OriginalPatternPath = pPath;
             
             std::string ext = std::filesystem::path(pPath).extension().string();
             std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
             
             if (ext == ".pat" && pIndex >= 0) {
                 // Autoload PAT
                 g_LoadedPatterns = PatLoader::Load(pPath);
                 if (pIndex < (int)g_LoadedPatterns.size()) {
                     // Regenerate Thumbnails
                      for (auto& t : g_PatternThumbnails) glDeleteTextures(1, &t);
                      g_PatternThumbnails.clear();
                      for (const auto& pat : g_LoadedPatterns) {
                            GLuint tex = 0;
                            glGenTextures(1, &tex);
                            glBindTexture(GL_TEXTURE_2D, tex);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pat.width, pat.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pat.pixels.data());
                            g_PatternThumbnails.push_back(tex);
                      }
                      
                      // Extract Specifc
                      const auto& pat = g_LoadedPatterns[pIndex];
                      
                      std::string configDir = Utils::GetConfigDir();
                      std::string tempPath = (std::filesystem::path(configDir) / "temp_pattern.png").string();
                      
                      if (BitmapUtils::SaveImage(tempPath, pat.width, pat.height, pat.pixels)) {
                          BitmapUtils::ClearPatternCache();
                          g_PatternPath = tempPath; // Absolute path is better, but this likely implies absolute if configDir is absolute.
                          // Actually GetConfigDir usually returns absolute.
                          // Let's stick to what we have or ensure absolute.
                          g_PatternPath = std::filesystem::absolute(tempPath).string();
                      }
                 } else {
                     // Index OOB
                     g_MissingPatternPath = pPath + " (Index invalid)";
                     g_ShowMissingPatternDialog = true;
                 }
             } else {
                 g_PatternPath = pPath;
                 // Clear old cache just in case
                 g_LoadedPatterns.clear();
                 for (auto& t : g_PatternThumbnails) glDeleteTextures(1, &t);
                 g_PatternThumbnails.clear();
             }
        }
    } else {
         g_PatternPath = "";
         g_LoadedPatterns.clear();
         for(auto& t : g_PatternThumbnails) glDeleteTextures(1, &t);
         g_PatternThumbnails.clear();
    }
    
    // Refresh pattern preview texture if path valid
    if (!g_PatternPath.empty() && std::filesystem::exists(g_PatternPath)) {
        UpdatePatternPreviewTexture();
    } else {
        if(g_PatternPreviewTexture) { glDeleteTextures(1, &g_PatternPreviewTexture); g_PatternPreviewTexture=0;}
    }

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
    
    g_SSAAFactor = Utils::ParseIntValue(c, "ssaaFactor", 1);
    g_ExportPath = Utils::ParseStringValue(c, "exportPath");
    // Legacy support for old boolean previewSSAA
    if (c.find("\"previewSSAA\": true") != std::string::npos) g_SSAAFactor = 2;

    g_HintingMode = Utils::ParseIntValue(c, "hintingMode", 0);
    
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
        g_CustomGlyphsText = customText;
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
    g_LastFillFp = GetFingerprint(g_FillGradientWidget);
    g_LastStrokeFp = GetFingerprint(g_StrokeGradientWidget);
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
static std::vector<GLuint> g_PreviewTextures;
static GLuint g_TextPreviewTexture = 0; // New: Text Line Preview
static int g_PreviewWidth = 0;
static int g_PreviewHeight = 0;
AtlasResult g_LastAtlas;
static AtlasResult g_LastTextPreview; // New
std::string g_StatusMessage = "";
double g_StatusTime = 0.0;
bool g_StatusIsError = false; 

// ... (UpdatePreview remains same) ...
AtlasSettings ConstructSettings() {
    AtlasSettings settings;
    settings.fontSize = g_FontSize;
    settings.padding = g_Padding;
    settings.atlasWidth = g_AtlasWidth;
    settings.atlasHeight = g_AtlasHeight;
    settings.superSamplingFactor = g_SSAAFactor;
    settings.superSamplingFactor = g_SSAAFactor;
    settings.hintingMode = g_HintingMode;
    settings.enableKerning = g_EnableKerning;
    
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
    settings.strokeJoinStyle = g_StrokeJoinStyle;
    settings.strokeMiterLimit = g_StrokeMiterLimit;
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
    settings.innerGlowBlendMode = static_cast<BlendMode>(g_InnerGlowBlendMode);
    
    // Pattern
    settings.pattern.enabled = g_EnablePattern;
    settings.pattern.imagePath = g_PatternPath;
    settings.pattern.opacity = g_PatternOpacity;
    settings.pattern.angle = g_PatternAngle;
    settings.pattern.scale = g_PatternScale;
    settings.pattern.blendMode = static_cast<BlendMode>(g_PatternBlendMode);
    settings.pattern.mappingMode = static_cast<PatternMapping>(g_PatternMappingMode);

    // Char Adjustments
    settings.globalXAdvance = g_GlobalXAdvance;
    settings.globalXOffset = g_GlobalXOffset;
    settings.globalYOffset = g_GlobalYOffset;

    // SSAA
    settings.superSamplingFactor = g_SSAAFactor;
    
    // Multi Page
    settings.allowMultiPage = g_AllowMultiPage;
    settings.keepInputOrder = true;

    return settings;
}

void UpdateAtlasTextures() {
    // Check for errors
    if (g_LastAtlas.hasErrors) {
        g_StatusMessage = g_LastAtlas.errorMessage;
        g_StatusTime = ImGui::GetTime();
        g_StatusIsError = true;
    }
    
    // Update Atlas Textures (Must be called on Main Thread)
    for(auto t : g_PreviewTextures) glDeleteTextures(1, &t);
    g_PreviewTextures.clear();

    if (!g_LastAtlas.pages.empty()) {
        for(const auto& page : g_LastAtlas.pages) {
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, page.width, page.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, page.pixels.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            g_PreviewTextures.push_back(tex);
        }

        // Calculate total logical dims (Layout: Horizontal with spacing)
        if (g_LastAtlas.atlasWidth > 0 && g_LastAtlas.atlasHeight > 0 && g_LastAtlas.pages.size() == 1) {
            // Use logical size if available and single page (handles overflow case)
            g_PreviewWidth = g_LastAtlas.atlasWidth;
            g_PreviewHeight = g_LastAtlas.atlasHeight;
        } else {
            // Fallback or aggregate for pages
            g_PreviewWidth = 0;
            g_PreviewHeight = 0;
            int spacing = 20;
            for(const auto& p : g_LastAtlas.pages) {
                 g_PreviewWidth += p.width;
                 g_PreviewHeight = std::max(g_PreviewHeight, p.height);
            }
            if(g_LastAtlas.pages.size() > 1) g_PreviewWidth += (int)(g_LastAtlas.pages.size() - 1) * spacing;
        }
    }
}

void UpdateTextPreview(const char* text) {
    if (!g_FontManager.IsLoaded()) return;
    AtlasSettings settings = ConstructSettings();

    g_LastTextPreview = TextureGenerator::GenerateTextPreview(g_FontManager, std::string(text), settings);
    
    if (!g_LastTextPreview.pages.empty()) {
        const auto& page = g_LastTextPreview.pages[0];
        if (page.width > 0 && page.height > 0) {
            if (g_TextPreviewTexture) glDeleteTextures(1, &g_TextPreviewTexture);
            glGenTextures(1, &g_TextPreviewTexture);
            glBindTexture(GL_TEXTURE_2D, g_TextPreviewTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, page.width, page.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, page.pixels.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    }
}

void UpdatePreview(const char* text) {
    UpdateTextPreview(text);
    g_AtlasUpdatePending = true;
    g_LastInteractionTime = ImGui::GetTime();
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
    Utils::LoadWindowConfig(winX, winY, winW, winH, g_SSAAFactor);

    char title[128];
    sprintf(title, "Fnt Generator v%s", APP_VERSION);
    GLFWwindow* window = glfwCreateWindow(winW, winH, title, NULL, NULL);
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

    // Fallback order: res/ui_font.ttf -> System Specific (YaHei/PingFang/Arial) -> Default
    bool fontLoaded = false;
    
    // 1. Try bundled font first (for consistency across platforms)
    std::string bundledFontSelf = Utils::GetExecutablePath() + "/res/ui_font.ttf";
    std::string bundledFontPath = "";
    if (std::filesystem::exists(bundledFontSelf)) {
        bundledFontPath = bundledFontSelf;
    } else if (std::filesystem::exists("res/ui_font.ttf")) {
        bundledFontPath = "res/ui_font.ttf";
    }

    if (!bundledFontPath.empty()) {
        if (io.Fonts->AddFontFromFileTTF(bundledFontPath.c_str(), 18.0f, NULL, ranges.Data)) {
            fontLoaded = true;
        }
    }

    if (!fontLoaded) {
        #ifdef _WIN32
        const char* winFont = "C:\\Windows\\Fonts\\msyh.ttc";
        if (std::filesystem::exists(winFont)) {
            io.Fonts->AddFontFromFileTTF(winFont, 18.0f, NULL, ranges.Data);
            fontLoaded = true;
        } else {
            winFont = "C:\\Windows\\Fonts\\Arial.ttf";
            if (std::filesystem::exists(winFont)) {
                io.Fonts->AddFontFromFileTTF(winFont, 18.0f, NULL, ranges.Data);
                fontLoaded = true;
            }
        }
        #elif defined(__APPLE__)
        const char* macFont = "/System/Library/Fonts/PingFang.ttc";
        if (std::filesystem::exists(macFont)) {
            io.Fonts->AddFontFromFileTTF(macFont, 18.0f, NULL, ranges.Data);
            fontLoaded = true;
        }
        #endif
    }

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

        // 1. Debounce Logic & Async Trigger
        if (g_AtlasUpdatePending && !g_IsGeneratingAtlas && (ImGui::GetTime() - g_LastInteractionTime) > g_AtlasDebounceTime) {
            if (g_FontManager.IsLoaded()) {
                g_IsGeneratingAtlas = true;
                g_AtlasUpdatePending = false;
                
                // Prepare params
                AtlasSettings settings = ConstructSettings();
                std::vector<uint32_t> charset = Utils::DecodeUtf8(g_CustomGlyphsText.c_str());
                std::sort(charset.begin(), charset.end());
                charset.erase(std::unique(charset.begin(), charset.end()), charset.end());
                if (charset.empty()) {
                    for (uint32_t c = 0x20; c <= 0x7E; c++) charset.push_back(c);
                }
                if (!g_ExcludedGlyphs.empty()) {
                    std::vector<uint32_t> filtered;
                    for (uint32_t c : charset) if (g_ExcludedGlyphs.find(c) == g_ExcludedGlyphs.end()) filtered.push_back(c);
                    charset = filtered;
                }

                // Launch Thread
                g_AtlasFuture = std::async(std::launch::async, [settings, charset]() {
                    return TextureGenerator::GenerateAtlas(g_FontManager, charset, settings);
                });
            }
        }

        // 2. Async Completion Logic
        if (g_IsGeneratingAtlas && g_AtlasFuture.valid()) {
            if (g_AtlasFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                g_LastAtlas = g_AtlasFuture.get();
                UpdateAtlasTextures();
                g_IsGeneratingAtlas = false;
            }
        }

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
                if (ImGui::MenuItem("Export Font")) {
                    ExportDialog::Open();
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

            if (ImGui::BeginCombo("##FontSelector", previewName.c_str())) {
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
                    
                    // Star button
                    ImGui::PushID(idx);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
                    
                    if (ImGui::Button("##FavBtn", ImVec2(18, 18))) {
                        ToggleFavorite(g_SystemFonts[idx].path);
                    }
                    ImGui::PopStyleColor(3);
                    
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip(isFav ? "Remove from Favorites" : "Add to Favorites");

                    // Draw Star
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();
                    ImVec2 p_min = ImGui::GetItemRectMin();
                    ImVec2 p_max = ImGui::GetItemRectMax();
                    ImVec2 center = ImVec2((p_min.x + p_max.x) * 0.5f, (p_min.y + p_max.y) * 0.5f);
                    float radius = 7.0f;

                    ImU32 starColor = isFav ? IM_COL32(255, 215, 0, 255) : IM_COL32(100, 100, 100, 150);
                    if (ImGui::IsItemHovered()) starColor = isFav ? IM_COL32(255, 230, 50, 255) : IM_COL32(180, 180, 180, 255);

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
                    
                    ImGui::SameLine();
                    
                    // Highlight favorite fonts in the list
                    if (isFav) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.8f, 1.0f)); // Slight yellow tint
                    }
                    
                    if (g_ShowFontPreview) {
                        GLuint tex = Utils::GenerateFontPreview(g_SystemFonts[idx].path);
                        if (tex != 0) {
                            ImGui::Image((void*)(intptr_t)tex, ImVec2(40, 20), ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), ImVec4(1,1,1,0.0f));
                            ImGui::SameLine();
                        }
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
            ImGui::SameLine();
            FontInfoDialog::RenderButton(g_FontManager);
            ImGui::SameLine();
            ImGui::Text("Font");

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
                    auto current = GetFingerprint(g_FillGradientWidget);
                    if (current != g_LastFillFp) {
                        g_LastFillFp = current;
                        UpdatePreview(g_InputText);
                    }
                }
                ImGui::Dummy(ImVec2(0, 10));
            }

            // Effects
            ImGui::Separator();
            // Helper for Header Click Flash
            static std::map<std::string, float> g_HeaderFlashTimers;
            auto RenderFlash = [&](const char* id, ImVec2 pos, ImVec2 size) {
                if (g_HeaderFlashTimers.find(id) != g_HeaderFlashTimers.end()) {
                    float t = ImGui::GetTime() - g_HeaderFlashTimers[id];
                    if (t < 0.4f) {
                        float alpha = 1.0f - (t / 0.4f);
                        // Pulse size
                        float pulse = 4.0f * sinf(t * 3.14159f / 0.4f);
                        ImGui::GetWindowDrawList()->AddRect(
                            ImVec2(pos.x - pulse, pos.y - pulse),
                            ImVec2(pos.x + size.x + pulse, pos.y + size.y + pulse),
                            IM_COL32(255, 255, 255, (int)(255 * alpha)),
                            4.0f, 0, 2.0f + pulse
                        );
                         ImGui::GetWindowDrawList()->AddRect(
                            ImVec2(pos.x - pulse*0.5f, pos.y - pulse*0.5f),
                            ImVec2(pos.x + size.x + pulse*0.5f, pos.y + size.y + pulse*0.5f),
                            IM_COL32(255, 255, 255, (int)(200 * alpha)),
                            4.0f, 0, 2.0f
                        );
                    }
                }
            };

            if (g_EnableStroke) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.35f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
            }
            float strokeHStart = ImGui::GetCursorPosY();
            ImGui::SetNextItemAllowOverlap(); // Vital for the checkbox to work!
            
            ImGuiTreeNodeFlags strokeFlags = 0;
            if (!g_EnableStroke) strokeFlags |= ImGuiTreeNodeFlags_Leaf;
            
            bool openStroke = ImGui::CollapsingHeader("Effects: Outline", strokeFlags);
            // Click detection on disabled header
            if (!g_EnableStroke && ImGui::IsItemClicked()) {
                 g_HeaderFlashTimers["Outline"] = ImGui::GetTime();
            }

            float strokeHEnd = ImGui::GetCursorPosY();
            ImGui::PopStyleColor(3);

            // Checkbox on top
            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - 30, strokeHStart));
            RenderFlash("Outline", ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));

            ImGui::PushID("EnableOutlineCheck");
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::Checkbox("##EnableOutline", &g_EnableStroke)) {
                UpdatePreview(g_InputText);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::SetCursorPosY(strokeHEnd); // Restore
            
            if (g_EnableStroke && openStroke) {
                if (ImGui::SliderFloat("Width", &g_StrokeWidth, 0.0f, 10.0f)) UpdatePreview(g_InputText);
                if (ImGui::Combo("Position##Stroke", &g_StrokePosition, "Outside\0Center\0Inside\0")) UpdatePreview(g_InputText);
                if (ImGui::Combo("Join style##Stroke", &g_StrokeJoinStyle, "bevel\0miter\0round\0")) UpdatePreview(g_InputText);
                if (g_StrokeJoinStyle == 1) { // Miter
                    if (ImGui::SliderFloat("Miter limit##Stroke", &g_StrokeMiterLimit, 1.0f, 10.0f)) UpdatePreview(g_InputText);
                }
                if (ImGui::Checkbox("Stroke Gradient", &g_EnableStrokeGradient)) UpdatePreview(g_InputText);
                    
                if (!g_EnableStrokeGradient) {
                     if (ImGui::ColorEdit4("Line Color", g_StrokeColor)) UpdatePreview(g_InputText);
                } else {
                     if (ImGui::Combo("Type##Stroke", &g_StrokeGradientType, "Linear\0Radial\0")) UpdatePreview(g_InputText);
                     if (Utils::KnobAngle("Angle##Stroke", &g_StrokeGradientAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);

                     ImGui::Dummy(ImVec2(0, 5));
                     if (g_StrokeGradientWidget.widget("Stroke Gradient")) {
                         auto current = GetFingerprint(g_StrokeGradientWidget);
                         if (current != g_LastStrokeFp) {
                             g_LastStrokeFp = current;
                             UpdatePreview(g_InputText);
                         }
                     }
                     ImGui::Dummy(ImVec2(0, 10));
                }
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
            float shadowHStart = ImGui::GetCursorPosY();
            ImGui::SetNextItemAllowOverlap();
            
            ImGuiTreeNodeFlags shadowFlags = 0;
            if (!g_EnableShadow) shadowFlags |= ImGuiTreeNodeFlags_Leaf;
            
            bool openShadow = ImGui::CollapsingHeader("Effects: Shadow", shadowFlags);
            if (!g_EnableShadow && ImGui::IsItemClicked()) {
                 g_HeaderFlashTimers["Shadow"] = ImGui::GetTime();
            }

            float shadowHEnd = ImGui::GetCursorPosY();
            ImGui::PopStyleColor(3);

            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - 30, shadowHStart));
            RenderFlash("Shadow", ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));

            ImGui::PushID("EnableShadowCheck");
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::Checkbox("##EnableShadow", &g_EnableShadow)) {
                UpdatePreview(g_InputText);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::SetCursorPosY(shadowHEnd);

            if (g_EnableShadow && openShadow) {
                if (ImGui::SliderInt("Offset X##Shadow", &g_ShadowOffsetX, -20, 20)) UpdatePreview(g_InputText);
                if (ImGui::SliderInt("Offset Y##Shadow", &g_ShadowOffsetY, -20, 20)) UpdatePreview(g_InputText);
                if (ImGui::SliderInt("Blur##Shadow", &g_ShadowBlur, 0, 10)) UpdatePreview(g_InputText);
                if (ImGui::ColorEdit4("Color##Shadow", g_ShadowColor)) UpdatePreview(g_InputText);
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
            float bevelHStart = ImGui::GetCursorPosY();
            ImGui::SetNextItemAllowOverlap();
            
            ImGuiTreeNodeFlags bevelFlags = 0;
            if (!g_EnableBevel) bevelFlags |= ImGuiTreeNodeFlags_Leaf;
            
            bool openBevel = ImGui::CollapsingHeader("Effects: Bevel", bevelFlags);
            if (!g_EnableBevel && ImGui::IsItemClicked()) {
                 g_HeaderFlashTimers["Bevel"] = ImGui::GetTime();
            }

            float bevelHEnd = ImGui::GetCursorPosY();
            ImGui::PopStyleColor(3);

            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - 30, bevelHStart));
            RenderFlash("Bevel", ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));

            ImGui::PushID("EnableBevelCheck");
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::Checkbox("##EnableBevel", &g_EnableBevel)) {
                UpdatePreview(g_InputText);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::SetCursorPosY(bevelHEnd);

            if (g_EnableBevel && openBevel) {
                if (ImGui::SliderFloat("Distance##Bevel", &g_BevelDistance, 0.0f, 20.0f)) UpdatePreview(g_InputText);
                if (Utils::KnobAngle("Angle##Bevel", (float*)&g_BevelAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);
                if (ImGui::SliderFloat("Spread##Bevel", &g_BevelSpread, 0.0f, 20.0f)) UpdatePreview(g_InputText);
                if (ImGui::SliderFloat("Strength##Bevel", &g_BevelStrength, 0.0f, 10.0f)) UpdatePreview(g_InputText);
                
                const char* bevelTypes[] = { "Inner", "Outer" };
                if (ImGui::Combo("Type##Bevel", &g_BevelType, bevelTypes, IM_ARRAYSIZE(bevelTypes))) UpdatePreview(g_InputText);

                if (ImGui::ColorEdit4("Highlight color##Bevel", g_BevelHighlightColor)) UpdatePreview(g_InputText);
                if (ImGui::ColorEdit4("Shadow color##Bevel", g_BevelShadowColor)) UpdatePreview(g_InputText);
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
            float glowHStart = ImGui::GetCursorPosY();
            ImGui::SetNextItemAllowOverlap();
            
            ImGuiTreeNodeFlags glowFlags = 0;
            if (!g_EnableInnerGlow) glowFlags |= ImGuiTreeNodeFlags_Leaf;
            
            bool openGlow = ImGui::CollapsingHeader("Effects: Inner Glow", glowFlags);
            if (!g_EnableInnerGlow && ImGui::IsItemClicked()) {
                 g_HeaderFlashTimers["InnerGlow"] = ImGui::GetTime();
            }

            float glowHEnd = ImGui::GetCursorPosY();
            ImGui::PopStyleColor(3);

            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - 30, glowHStart));
            RenderFlash("InnerGlow", ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));

            ImGui::PushID("EnableGlowCheck");
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::Checkbox("##EnableInnerGlow", &g_EnableInnerGlow)) {
                UpdatePreview(g_InputText);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::SetCursorPosY(glowHEnd);

            if (g_EnableInnerGlow && openGlow) {
                if (ImGui::SliderFloat("Size##InnerGlow", &g_InnerGlowSize, 0.0f, 50.0f)) UpdatePreview(g_InputText);
                if (ImGui::SliderFloat("Choke##InnerGlow", &g_InnerGlowChoke, 0.0f, 100.0f)) UpdatePreview(g_InputText);
                const char* blendModes[] = { "Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Color Dodge", "Color Burn", "Hard Light", "Soft Light", "Difference", "Exclusion", "Subtract", "Divide" };
                if (ImGui::Combo("Blend Mode##InnerGlow", &g_InnerGlowBlendMode, blendModes, IM_ARRAYSIZE(blendModes))) UpdatePreview(g_InputText);
                if (ImGui::ColorEdit4("Color##InnerGlow", g_InnerGlowColor, ImGuiColorEditFlags_AlphaBar)) UpdatePreview(g_InputText);
            }

            // Pattern Overlay
            if (g_EnablePattern) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.35f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.45f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.55f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.15f, 0.25f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.35f, 0.50f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
            }
            float patternHStart = ImGui::GetCursorPosY();
            ImGui::SetNextItemAllowOverlap();
            
            ImGuiTreeNodeFlags patternFlags = 0;
            if (!g_EnablePattern) patternFlags |= ImGuiTreeNodeFlags_Leaf;
            
            bool openPattern = ImGui::CollapsingHeader("Effects: Pattern", patternFlags);
            if (!g_EnablePattern && ImGui::IsItemClicked()) {
                 g_HeaderFlashTimers["Pattern"] = ImGui::GetTime();
            }

            float patternHEnd = ImGui::GetCursorPosY();
            ImGui::PopStyleColor(3);

            ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x - 30, patternHStart));
            RenderFlash("Pattern", ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
            
            ImGui::PushID("EnablePatternCheck");
            ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::Checkbox("##EnablePattern", &g_EnablePattern)) {
                UpdatePreview(g_InputText);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::SetCursorPosY(patternHEnd);
            
            if (g_EnablePattern && openPattern) {
                    const char* blendModes[] = { "Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Color Dodge", "Color Burn", "Hard Light", "Soft Light", "Difference", "Exclusion", "Subtract", "Divide" };
                    if (ImGui::Combo("Blend Mode##Pattern", &g_PatternBlendMode, blendModes, IM_ARRAYSIZE(blendModes))) UpdatePreview(g_InputText);
                    if (ImGui::SliderFloat("Opacity##Pattern", &g_PatternOpacity, 0.0f, 1.0f)) UpdatePreview(g_InputText);
                    
                    ImGui::Text("Mapping:");
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Per Glyph", &g_PatternMappingMode, 0)) UpdatePreview(g_InputText);
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Global", &g_PatternMappingMode, 1)) UpdatePreview(g_InputText);

                    if (g_PatternPreviewTexture != 0) {
                        ImGui::Text("Preview:");
                        ImGui::Image((ImTextureID)(intptr_t)g_PatternPreviewTexture, ImVec2(64, 64), ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), ImVec4(1,1,1,0.5f));
                        
                        if (!g_LoadedPatterns.empty()) {
                            ImGui::SameLine();
                            if (ImGui::ArrowButton("##SelectPattern", ImGuiDir_Down)) {
                                g_RequestPatternPopup = true; // Defer to root scope
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose another pattern from the file");
                        }
                    }

                    ImGui::Text("File: %s", g_PatternPath.empty() ? "None" : g_PatternPath.c_str());
                    if (ImGui::Button("Choose Pattern...")) {
                        std::string path = Utils::PickFileDialog("Images/Pat (*.png, *.jpg, *.pat)\0*.png;*.jpg;*.jpeg;*.pat\0All Files\0*.*\0");
                        if (!path.empty()) {
                            // Check extension
                            std::string ext = std::filesystem::path(path).extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                            
                            
                            if (ext == ".pat") {
                                // Load PAT
                                g_OriginalPatternPath = path;
                                g_SelectedPatternIndex = -1; // None selected yet? Or default 0?
                                g_LoadedPatterns = PatLoader::Load(path);
                                if (!g_LoadedPatterns.empty()) {
                                    // Generate thumbnails
                                    for (auto& t : g_PatternThumbnails) glDeleteTextures(1, &t);
                                    g_PatternThumbnails.clear();
                                    
                                    for (const auto& pat : g_LoadedPatterns) {
                                        GLuint tex = 0;
                                        glGenTextures(1, &tex);
                                        glBindTexture(GL_TEXTURE_2D, tex);
                                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pat.width, pat.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pat.pixels.data());
                                        g_PatternThumbnails.push_back(tex);
                                    }
                                    g_ShowPatternSelector = true;
                                    g_RequestPatternPopup = true; 
                                } else {
                                     g_StatusMessage = "Error: Failed to load .pat file. See pat_debug_log.txt for details.";
                                     g_StatusTime = ImGui::GetTime();
                                     g_StatusIsError = true;
                                }
                            } else {
                                // Clear previously loaded patterns
                                g_LoadedPatterns.clear();
                                for (auto& t : g_PatternThumbnails) glDeleteTextures(1, &t);
                                g_PatternThumbnails.clear();

                                g_OriginalPatternPath = path;
                                g_SelectedPatternIndex = -1;
                                g_PatternPath = path;
                                UpdatePatternPreviewTexture();
                                UpdatePreview(g_InputText);
                            }
                        }
                    }

                    // Pattern Selector Modal

                if (Utils::KnobAngle("Angle##Pattern", &g_PatternAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);
                if (ImGui::SliderFloat("Scale##Pattern", &g_PatternScale, 0.1f, 5.0f)) UpdatePreview(g_InputText);
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

            if (ImGui::DragInt("Atlas Padding", &g_Padding, 0.5f, 0, 100)) {
                UpdatePreview(g_InputText);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Spacing between glyphs in the atlas texture to prevent bleeding.");

            // Multi Page Toggle (Always Available)
            if (ImGui::Checkbox("Multi Page", &g_AllowMultiPage)) UpdatePreview(g_InputText);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Allow generating multiple texture pages if content doesn't fit.\nWarning: Not natively supported by some frameworks like Starling.");

            // Quality Settings
            const char* ssaaOptions[] = { "Standard (1x)", "High Quality (2x)", "Ultra Quality (4x)" };
            int ssaaIdx = 0;
            if (g_SSAAFactor == 2) ssaaIdx = 1;
            else if (g_SSAAFactor == 4) ssaaIdx = 2;

            if (ImGui::Combo("Quality (SSAA)", &ssaaIdx, ssaaOptions, IM_ARRAYSIZE(ssaaOptions))) {
                if (ssaaIdx == 0) g_SSAAFactor = 1;
                else if (ssaaIdx == 1) g_SSAAFactor = 2;
                else if (ssaaIdx == 2) g_SSAAFactor = 4;
                UpdatePreview(g_InputText);
            }
            // Hinting Selector
            const char* hintingModes[] = { "Smooth (No Hinting)", "Sharp (Auto Hinting)", "Crisp (Normal Hinting)" };
            if (ImGui::Combo("Hinting", &g_HintingMode, hintingModes, IM_ARRAYSIZE(hintingModes))) {
                UpdatePreview(g_InputText);
            }
            
            // Canvas Preview Background
            ImGui::Separator();
            ImGui::Text("Canvas Background");
            bool canvasTransparent = (g_PreviewBgColor[3] == 0.0f);
            if (ImGui::Checkbox("Transparent Canvas", &canvasTransparent)) {
                g_PreviewBgColor[3] = canvasTransparent ? 0.0f : 1.0f;
            }
            if (!canvasTransparent) {
                ImGui::ColorEdit3("Canvas Color", g_PreviewBgColor);
                g_PreviewBgColor[3] = 1.0f;
            }
            
            // Preview Info
            if (!g_LastAtlas.pages.empty()) {
                if (g_LastAtlas.pages.size() > 1) {
                     // Show Per-Page dimensions and Count
                     ImGui::Text("Dims: %d x %d (%d Pages)", g_LastAtlas.pages[0].width, g_LastAtlas.pages[0].height, (int)g_LastAtlas.pages.size());
                     if (ImGui::IsItemHovered()) ImGui::SetTooltip("Total combined width for preview: %d px", g_PreviewWidth);
                } else {
                     ImGui::Text("Dims: %d x %d", g_LastAtlas.pages[0].width, g_LastAtlas.pages[0].height);
                }
            } else {
                 ImGui::Text("Dims: -");
            }
            
            ImGui::Separator();
            ImGui::Separator();
            ImGui::Text("Char Adjustments");
            if (ImGui::DragInt("xAdvance##Global", &g_GlobalXAdvance, 1, -100, 100)) UpdatePreview(g_InputText);
            if (ImGui::DragInt("xOffset##Global", &g_GlobalXOffset, 1, -100, 100)) UpdatePreview(g_InputText);
            if (ImGui::DragInt("yOffset##Global", &g_GlobalYOffset, 1, -100, 100)) UpdatePreview(g_InputText);
            if (ImGui::Checkbox("Enable Kerning", &g_EnableKerning)) UpdatePreview(g_InputText);

            ImGui::Separator();
            
            ExportDialog::RenderButton();
            
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
                        if (!g_LastTextPreview.pages.empty()) {
                            float fitW = ImGui::GetContentRegionAvail().x / (float)g_LastTextPreview.pages[0].width;
                            float fitH = ImGui::GetContentRegionAvail().y / (float)g_LastTextPreview.pages[0].height;
                            g_TextZoom = std::min(fitW, fitH);
                            if (g_TextZoom > 1.0f) g_TextZoom = 1.0f;
                            g_TextPan = { 0,0 };
                        }
                    }
                     
                    ImVec2 p_min = ImGui::GetCursorScreenPos();
                    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
                    ImVec2 p_max = ImVec2(p_min.x + canvas_sz.x, p_min.y + canvas_sz.y);
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();

                    // Masking region for drawing
                    ImGui::PushClipRect(p_min, p_max, true);

                    // BG Checkered
                    if (g_PreviewBgColor[3] == 0.0f && g_CheckerTexture) {
                         draw_list->AddImage((void*)(intptr_t)g_CheckerTexture, p_min, p_max, ImVec2(0,0), ImVec2(canvas_sz.x / 32.0f, canvas_sz.y / 32.0f));
                    } else if (g_PreviewBgColor[3] != 0.0f) {
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

                        if (!g_LastTextPreview.pages.empty()) {
                            const auto& page = g_LastTextPreview.pages[0];
                            float imgW = page.width * g_TextZoom;
                            float imgH = page.height * g_TextZoom;

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
                        if (g_PreviewHeight > 0 && g_PreviewWidth > 0) {
                            float fitW = ImGui::GetContentRegionAvail().x / (float)g_PreviewWidth;
                            float fitH = ImGui::GetContentRegionAvail().y / (float)g_PreviewHeight;
                            g_AtlasZoom = std::min(fitW, fitH);
                            if (g_AtlasZoom > 1.0f) g_AtlasZoom = 1.0f;
                            g_AtlasPan = {0,0};
                        }
                    }

                    ImVec2 p_min = ImGui::GetCursorScreenPos();
                    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
                    ImVec2 p_max = ImVec2(p_min.x + canvas_sz.x, p_min.y + canvas_sz.y);
                    ImDrawList* draw_list = ImGui::GetWindowDrawList();

                    // Masking region for drawing
                    ImGui::PushClipRect(p_min, p_max, true);

                    // BG Checkered
                    if (g_PreviewBgColor[3] == 0.0f && g_CheckerTexture) {
                         draw_list->AddImage((void*)(intptr_t)g_CheckerTexture, p_min, p_max, ImVec2(0,0), ImVec2(canvas_sz.x / 32.0f, canvas_sz.y / 32.0f));
                    } else if (g_PreviewBgColor[3] != 0.0f) {
                         draw_list->AddRectFilled(p_min, p_max, ImColor(g_PreviewBgColor[0], g_PreviewBgColor[1], g_PreviewBgColor[2], 1.0f));
                    }

                    // Atlas Interaction & Display
                    if (!g_PreviewTextures.empty()) {
                        // Zoom via Scroll
                        if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0) {
                            float zoomStep = 0.1f * g_AtlasZoom;
                            g_AtlasZoom += ImGui::GetIO().MouseWheel * zoomStep;
                            if (g_AtlasZoom < 0.05f) g_AtlasZoom = 0.05f;
                            if (g_AtlasZoom > 20.0f) g_AtlasZoom = 20.0f;
                        }

                        // Determine layout dimensions
                        float spacing = 20.0f * g_AtlasZoom;
                        float totalLogicW = 0;
                        float maxLogicH = 0;
                        for(const auto& page : g_LastAtlas.pages) {
                            totalLogicW += page.width * g_AtlasZoom;
                            maxLogicH = std::max(maxLogicH, (float)page.height * g_AtlasZoom);
                        }
                        if(g_LastAtlas.pages.size() > 1) totalLogicW += (g_LastAtlas.pages.size() - 1) * spacing;

                        // Center by default if smaller than canvas
                        ImVec2 offset = g_AtlasPan;
                        if (totalLogicW < canvas_sz.x) offset.x = (canvas_sz.x - totalLogicW) * 0.5f;
                        if (maxLogicH < canvas_sz.y) offset.y = (canvas_sz.y - maxLogicH) * 0.5f;

                        float currentX = p_min.x + offset.x;
                        float startY = p_min.y + offset.y;

                        // Draw Loop
                        for (size_t i = 0; i < g_PreviewTextures.size(); i++) {
                             if (i >= g_LastAtlas.pages.size()) break; 
                             const auto& page = g_LastAtlas.pages[i];
                             GLuint tex = g_PreviewTextures[i];
                             
                             float pW = page.width * g_AtlasZoom;
                             float pH = page.height * g_AtlasZoom;
                             
                             ImVec2 pos(currentX, startY);
                             draw_list->AddImage((void*)(intptr_t)tex, pos, ImVec2(pos.x + pW, pos.y + pH));
                             
                             int logicalW = (g_LastAtlas.atlasWidth > 0) ? g_LastAtlas.atlasWidth : page.width;
                             int logicalH = (g_LastAtlas.atlasHeight > 0) ? g_LastAtlas.atlasHeight : page.height;

                             float lW = logicalW * g_AtlasZoom;
                             float lH = logicalH * g_AtlasZoom;

                             // Border (Yellow for Logical Page Boundary)
                             draw_list->AddRect(pos, ImVec2(pos.x + lW, pos.y + lH), IM_COL32(255, 255, 0, 150), 0, 0, 2.0f);

                             // Overflow Overlay (Red/Dark Tint)
                             if (page.width > logicalW || page.height > logicalH) {
                                 // Horizontal Strip (Right side)
                                 if (page.width > logicalW) {
                                     draw_list->AddRectFilled(
                                         ImVec2(pos.x + lW, pos.y), 
                                         ImVec2(pos.x + pW, pos.y + pH), 
                                         IM_COL32(50, 0, 0, 100) // Reddish-Dark Tint
                                     );
                                      draw_list->AddRect(
                                         ImVec2(pos.x + lW, pos.y), 
                                         ImVec2(pos.x + pW, pos.y + pH), 
                                         IM_COL32(255, 0, 0, 100) // Red Border
                                     );
                                 }
                                 // Vertical Strip (Bottom side, excluding the corner if already covered)
                                 if (page.height > logicalH) {
                                     float rightX = (page.width > logicalW) ? (pos.x + lW) : (pos.x + pW);
                                     draw_list->AddRectFilled(
                                         ImVec2(pos.x, pos.y + lH), 
                                         ImVec2(rightX, pos.y + pH), 
                                         IM_COL32(50, 0, 0, 100) 
                                     );
                                     draw_list->AddRect(
                                         ImVec2(pos.x, pos.y + lH), 
                                         ImVec2(rightX, pos.y + pH), 
                                         IM_COL32(255, 0, 0, 100) 
                                     );
                                 }
                             }
                             
                             // Page ID
                             char pageLabel[32];
                             sprintf(pageLabel, "Page %d", (int)i);
                             ImVec2 textPos = ImVec2(pos.x, pos.y - 20);
                             // Outline
                             for(int ox=-1; ox<=1; ox++) {
                                 for(int oy=-1; oy<=1; oy++) {
                                     if(ox==0 && oy==0) continue;
                                     draw_list->AddText(ImVec2(textPos.x+ox, textPos.y+oy), IM_COL32(0,0,0,255), pageLabel);
                                 }
                             }
                             draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), pageLabel);



                             // Glyph Hover logic
                             ImVec2 mousePos = ImGui::GetMousePos();
                             if (ImGui::IsWindowHovered() &&
                                 mousePos.x >= pos.x && mousePos.x < pos.x + pW &&
                                 mousePos.y >= pos.y && mousePos.y < pos.y + pH) 
                             {
                                 int relX = (int)((mousePos.x - pos.x) / g_AtlasZoom);
                                 int relY = (int)((mousePos.y - pos.y) / g_AtlasZoom);
                                 
                                 int hoveredIndex = -1;
                                 for (size_t k = 0; k < g_LastAtlas.glyphs.size(); k++) {
                                     const auto& g = g_LastAtlas.glyphs[k];
                                     if (g.pageIndex == (int)i) {
                                         if (relX >= g.x && relX < g.x + g.width &&
                                             relY >= g.y && relY < g.y + g.height) {
                                             hoveredIndex = (int)k;
                                             break;
                                         }
                                     }
                                 }
                                 
                                 if (hoveredIndex != -1) {
                                     const auto& g = g_LastAtlas.glyphs[hoveredIndex];
                                     float sX = pos.x + g.x * g_AtlasZoom;
                                     float sY = pos.y + g.y * g_AtlasZoom;
                                     float sW = g.width * g_AtlasZoom;
                                     float sH = g.height * g_AtlasZoom;
                                     draw_list->AddRect(ImVec2(sX, sY), ImVec2(sX + sW, sY + sH), IM_COL32(0, 255, 0, 255), 0.0f, 0, 2.0f);
                                     
                                     ImGui::BeginTooltip();
                                     std::string charStr = Utils::EncodeUtf8({g.charCode});
                                     ImGui::Text("Char: %s (U+%04X)", charStr.c_str(), g.charCode);
                                     ImGui::Text("Size: %dx%d", g.width, g.height);
                                     ImGui::Text("Pos: %d, %d (Page %d)", g.x, g.y, g.pageIndex);
                                     
                                     // Overflow Warning in Tooltip
                                     if (g.x + g.width > logicalW || g.y + g.height > logicalH) {
                                         ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
                                         ImGui::Text("WARNING: Glyph outside atlas bounds!");
                                         ImGui::PopStyleColor();
                                     }

                                     ImGui::EndTooltip();
                                     
                                     if (ImGui::IsMouseClicked(0)) {
                                         g_SelectedGlyphIndex = hoveredIndex;
                                         ImGui::OpenPopup("GlyphContext");
                                     }
                                 }
                             }
                             currentX += pW + spacing;
                        }

                        // Panning
                        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            g_IsPanning = true;
                        }
                        
                         // WARNING: Skipped Glyphs
                        if (g_LastAtlas.skippedGlyphs > 0) {
                             char warnMsg[64];
                             sprintf(warnMsg, "WARNING: %d Glyphs Omitted (Space)", g_LastAtlas.skippedGlyphs);
                             ImVec2 warnPos = ImVec2(p_min.x + 10, p_min.y + 10);
                             
                             // Background box
                             ImVec2 txtSz = ImGui::CalcTextSize(warnMsg);
                             draw_list->AddRectFilled(warnPos, ImVec2(warnPos.x + txtSz.x + 10, warnPos.y + txtSz.y + 10), IM_COL32(50, 0, 0, 200));
                             draw_list->AddRect(warnPos, ImVec2(warnPos.x + txtSz.x + 10, warnPos.y + txtSz.y + 10), IM_COL32(255, 0, 0, 255));
                             
                             draw_list->AddText(ImVec2(warnPos.x + 5, warnPos.y + 5), IM_COL32(255, 200, 50, 255), warnMsg);

                             // Warning Hover Tooltip
                             ImVec2 mouseP = ImGui::GetMousePos();
                             if (mouseP.x >= warnPos.x && mouseP.x <= warnPos.x + txtSz.x + 10 &&
                                 mouseP.y >= warnPos.y && mouseP.y <= warnPos.y + txtSz.y + 10) 
                             {
                                 ImGui::BeginTooltip();
                                 ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Some characters did not fit in the texture!");
                                 ImGui::Separator();
                                 ImGui::Text("- Try increasing the Atlas Size (Width/Height).");
                                 ImGui::Text("- Set Atlas Size to 'Auto' for automatic sizing.");
                                 ImGui::Text("- Enable 'Multi Page' to allow multiple textures.");
                                 ImGui::EndTooltip();
                             }
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

                    // Loading Overlay (Drawn last to be on top)
                    if (g_IsGeneratingAtlas || g_AtlasUpdatePending) {
                        ImVec2 winPos = ImGui::GetWindowPos();
                        ImVec2 winSize = ImGui::GetWindowSize();
                        ImDrawList* dl = ImGui::GetWindowDrawList();
                        
                        dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), IM_COL32(0, 0, 0, 150));
                        
                        // Spinner
                        ImVec2 center = ImVec2(winPos.x + winSize.x * 0.5f, winPos.y + winSize.y * 0.5f);
                        float radius = 20.0f;
                        float thickness = 4.0f;
                        float time = (float)ImGui::GetTime();
                        
                        dl->PathClear();
                        dl->PathArcTo(center, radius, time * 6.0f, time * 6.0f + 4.0f, 30);
                        dl->PathStroke(IM_COL32(255, 255, 0, 255), 0, thickness);

                        std::string loadingText = g_IsGeneratingAtlas ? "UPDATING" : "WAITING";
                        ImVec2 textSize = ImGui::CalcTextSize(loadingText.c_str());
                        dl->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y + radius + 10.0f), IM_COL32(255, 255, 255, 255), loadingText.c_str());
                    }
                }
                ImGui::EndChild();
            }
            ImGui::EndChild(); // End RightPanelContainer
            ImGui::PopStyleColor();

            ImGui::End();
        }
        
        // Popups (Moved to global scope to ensure visibility)
        if (g_RequestPatternPopup) {
            ImGui::OpenPopup("Select Pattern");
            g_ShowPatternSelector = true;
            g_RequestPatternPopup = false;
        }

        // Center Modal
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        
        // Limit height to 85% of screen and allow scrolling if needed
        float maxY = ImGui::GetMainViewport()->WorkSize.y * 0.85f;
        ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, maxY));

        if (ImGui::BeginPopupModal("Select Pattern", &g_ShowPatternSelector, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Choose a pattern from the file:");
            ImGui::Separator();
            
            ImGuiStyle& style = ImGui::GetStyle();
            float itemStep = 64.0f + style.ItemSpacing.x;
            float availW = ImGui::GetMainViewport()->WorkSize.x * 0.9f - style.WindowPadding.x * 2.0f;
            int cols = (int)(availW / itemStep);
            if (cols > 5) cols = 5;
            if (cols < 1) cols = 1;

            for (size_t i = 0; i < g_LoadedPatterns.size(); i++) {
                ImGui::PushID((int)i);
                
                // Adaptive Cols
                if ((i % cols) != 0) ImGui::SameLine();
                
                if (ImGui::ImageButton((ImTextureID)(intptr_t)g_PatternThumbnails[i], ImVec2(64, 64))) {
                    // Selected! Save to temp file.
                    g_SelectedPatternIndex = (int)i;
                    const auto& pat = g_LoadedPatterns[i];
                    std::string configDir = Utils::GetConfigDir();
                    std::string tempPath = (std::filesystem::path(configDir) / "temp_pattern.png").string();
                    
                    if (BitmapUtils::SaveImage(tempPath, pat.width, pat.height, pat.pixels)) {
                        BitmapUtils::ClearPatternCache(); // Force reload
                        g_PatternPath = std::filesystem::absolute(tempPath).string();
                        
                        UpdatePatternPreviewTexture();
                        UpdatePreview(g_InputText);
                    } else {
                        // Handle error?
                        g_StatusMessage = "Failed to save temp pattern file.";
                        g_StatusTime = ImGui::GetTime();
                        g_StatusIsError = true;
                    }

                    g_ShowPatternSelector = false;
                    ImGui::CloseCurrentPopup();
                }
                
                // Draw Green Border if selected
                if ((int)i == g_SelectedPatternIndex) {
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(0, 255, 0, 255), 3.0f, 0, 3.0f);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s (%dx%d)", g_LoadedPatterns[i].name.c_str(), g_LoadedPatterns[i].width, g_LoadedPatterns[i].height);
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        
        if (g_ShowMissingPatternDialog) {
                ImGui::OpenPopup("Missing Pattern File");
                g_ShowMissingPatternDialog = false;
        }
        
        if (ImGui::BeginPopupModal("Missing Pattern File", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Could not find the pattern file:");
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "%s", g_MissingPatternPath.c_str());
            ImGui::Separator();
            
            if (ImGui::Button("Locate File...", ImVec2(120, 0))) {
                    std::string newPath = Utils::PickFileDialog("Images/Pat (*.png, *.jpg, *.pat)\0*.png;*.jpg;*.jpeg;*.pat\0All Files\0*.*\0");
                    if (!newPath.empty()) {
                        // Update State
                        g_OriginalPatternPath = newPath;
                        g_PatternPath = newPath; // Temporary assumption
                        
                        // Treat as if loaded from file
                        std::string ext = std::filesystem::path(newPath).extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        
                        if (ext == ".pat") {
                            // Load PAT
                            g_LoadedPatterns = PatLoader::Load(newPath);
                            if (!g_LoadedPatterns.empty()) {
                                // Try to match index
                                if (g_SelectedPatternIndex < 0 || g_SelectedPatternIndex >= (int)g_LoadedPatterns.size()) {
                                    g_SelectedPatternIndex = 0;
                                }
                                
                                // thumbnails
                                for(auto& t : g_PatternThumbnails) glDeleteTextures(1, &t);
                                g_PatternThumbnails.clear();
                                for (const auto& pat : g_LoadedPatterns) {
                                    GLuint tex=0; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
                                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
                                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                                    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,pat.width,pat.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pat.pixels.data());
                                    g_PatternThumbnails.push_back(tex);
                                }
                                
                                const auto& pat = g_LoadedPatterns[g_SelectedPatternIndex];
                                std::string configDir = Utils::GetConfigDir();
                                std::string tempPath = (std::filesystem::path(configDir) / "temp_pattern.png").string();

                                if (BitmapUtils::SaveImage(tempPath, pat.width, pat.height, pat.pixels)) {
                                    BitmapUtils::ClearPatternCache();
                                    g_PatternPath = std::filesystem::absolute(tempPath).string();
                                }
                            }
                        } else {
                            // Simple image
                            g_LoadedPatterns.clear();
                            for(auto& t : g_PatternThumbnails) glDeleteTextures(1, &t);
                            g_PatternThumbnails.clear();
                            g_SelectedPatternIndex = -1;
                        }
                        
                        if (!g_PatternPath.empty()) {
                            UpdatePatternPreviewTexture();
                            UpdatePreview(g_InputText);
                        }
                        ImGui::CloseCurrentPopup();
                    }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Effect", ImVec2(120, 0))) {
                g_EnablePattern = false;
                g_PatternPath = "";
                g_OriginalPatternPath = "";
                g_SelectedPatternIndex = -1;
                UpdatePreview(g_InputText);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
            
            const char* ssaaOptions[] = { "Standard (1x)", "High Quality (2x)", "Ultra Quality (4x)" };
            int ssaaIdx = 0;
            if (g_SSAAFactor == 2) ssaaIdx = 1;
            else if (g_SSAAFactor == 4) ssaaIdx = 2;

            if (ImGui::Combo("Default Preview Quality", &ssaaIdx, ssaaOptions, IM_ARRAYSIZE(ssaaOptions))) {
                if (ssaaIdx == 0) g_SSAAFactor = 1;
                else if (ssaaIdx == 1) g_SSAAFactor = 2;
                else if (ssaaIdx == 2) g_SSAAFactor = 4;
                UpdatePreview(g_InputText);
            }
            
            ImGui::Checkbox("Show Font Previews in List", &g_ShowFontPreview);
            
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ExportDialog::RenderDialog();
        ExportDialog::RenderSuccessNotification();
        FontInfoDialog::RenderDialog(g_FontManager);

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
    Utils::SaveWindowConfig(curX, curY, curW, curH, g_SSAAFactor);

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
