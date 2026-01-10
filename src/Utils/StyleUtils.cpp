#include "StyleUtils.h"
#include "PlatformUtils.h"
#include "SettingsManager.h"
#include "SharedState.h"
#include "BitmapUtils.h"
#include "PatLoader.h"
#include "JsonUtils.h"
#include <nlohmann/json.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <GLFW/glfw3.h> 
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iostream>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#include "UIUtils.h"

// Helper implementation
GradientFingerprint GetFingerprint(const ImGG::GradientWidget& w) {
    GradientFingerprint fp;
    for (const auto& m : w.gradient().get_marks()) {
        fp.marks.push_back({ m.position.get(), m.color.x, m.color.y, m.color.z, m.color.w });
    }
    return fp;
}

namespace Utils {

// Favorites
std::string GetFavoritesPath() { 
    return GetConfigDir() + "/font_favorites.txt"; 
}

void LoadFavorites(std::set<std::string>& favorites) {
    favorites = SettingsManager::Get().GetFavorites();
}

void SaveFavorites(const std::set<std::string>& favorites) {
    SettingsManager::Get().SetFavorites(favorites);
}

// Recents
std::string GetRecentsPath() { 
    return GetConfigDir() + "/style_recents.txt"; 
}

void LoadRecentStyles(std::vector<std::string>& recents) {
    recents = SettingsManager::Get().GetRecents();
}

void SaveRecentStyles(const std::vector<std::string>& recents) {
    SettingsManager::Get().SetRecents(recents);
}

void AddRecentStyle(std::vector<std::string>& recents, const std::string& path) {
    SettingsManager::Get().AddRecent(path);
    recents = SettingsManager::Get().GetRecents();
}

void RemoveRecentStyle(std::vector<std::string>& recents, const std::string& path) {
    SettingsManager::Get().RemoveRecent(path);
    recents = SettingsManager::Get().GetRecents();
}

// Custom Glyphs Presets
std::string GetCustomGlyphsDir() {
    std::string dir = GetConfigDir() + "/customglyphs";
    if (!std::filesystem::exists(dir)) std::filesystem::create_directories(dir);
    return dir;
}

std::vector<std::string> GetCustomGlyphsPresets() {
    std::vector<std::string> presets;
    std::string dir = GetCustomGlyphsDir();
    if (!std::filesystem::exists(dir)) return presets;
    
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            presets.push_back(entry.path().stem().string());
        }
    }
    return presets;
}

void SaveCustomGlyphsPreset(const std::string& name, const std::string& content) {
    if (name.empty()) return;
    std::string path = GetCustomGlyphsDir() + "/" + name + ".txt";
    std::ofstream out(path);
    if (out.is_open()) {
        out << content;
    }
}

void DeleteCustomGlyphsPreset(const std::string& name) {
    if (name.empty()) return;
    std::string path = GetCustomGlyphsDir() + "/" + name + ".txt";
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
}

std::string LoadCustomGlyphsPreset(const std::string& name) {
    std::string path = GetCustomGlyphsDir() + "/" + name + ".txt";
    std::ifstream in(path);
    if (!in.is_open()) return "";
    
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Window Config
void SaveWindowConfig(int x, int y, int w, int h, int ssaaFactor) {
    std::string path = GetConfigDir() + "/window.cfg";
    std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath);
    if (out.is_open()) {
        out << w << " " << h << " " << ssaaFactor << " " << x << " " << y;
        out.close();
        std::remove(path.c_str());
        std::rename(tmpPath.c_str(), path.c_str());
    }
}

void LoadWindowConfig(int& x, int& y, int& w, int& h, int& ssaaFactor) {
    x = 100; y = 100; w = 1280; h = 720; ssaaFactor = 1;
    std::ifstream in(GetConfigDir() + "/window.cfg");
    if (in.is_open()) {
        in >> w >> h >> ssaaFactor;
        if (!(in >> x >> y)) {
            x = 100; y = 100;
        }
    }
    if (w < 800) w = 800;
    if (h < 600) h = 600;
    if (x <= -32000 || y <= -32000) { x = 100; y = 100; }
    if (ssaaFactor != 1 && ssaaFactor != 2 && ssaaFactor != 4) ssaaFactor = 1;
}

// Global functions moved from main.cpp
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

    // Replaced Glyphs
    out << "  \"replacedGlyphs\": [\n";
    {
        bool first = true;
        for (const auto& pair : g_ReplacedGlyphs) {
            if (!first) out << ",\n";
            std::string escapedPath;
            for(char c : pair.second.imagePath) {
                if(c == '\\') escapedPath += "\\\\";
                else escapedPath += c;
            }
            out << "    { \"code\": " << pair.first << ", \"path\": \"" << escapedPath << "\", \"width\": " << pair.second.width << ", \"height\": " << pair.second.height 
                << ", \"xOffset\": " << pair.second.xOffset << ", \"yOffset\": " << pair.second.yOffset << ", \"advance\": " << pair.second.advance
                << ", \"applyEffects\": " << (pair.second.applyEffects ? "true" : "false")  
                << ", \"applyShadow\": " << (pair.second.applyShadow ? "true" : "false") 
                << ", \"applyStroke\": " << (pair.second.applyStroke ? "true" : "false") 
                << ", \"applyBevel\": " << (pair.second.applyBevel ? "true" : "false") 
                << ", \"applyInnerGlow\": " << (pair.second.applyInnerGlow ? "true" : "false") 

                << ", \"applyPattern\": " << (pair.second.applyPattern ? "true" : "false") 
                << ", \"applyFill\": " << (pair.second.applyFill ? "true" : "false") 
                << " }";
            first = false;
        }
    }
    out << "\n  ],\n";

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

    // Fallback Font Path
    if (g_SelectedFallbackFontIndex >= 0 && g_SelectedFallbackFontIndex < (int)g_SystemFonts.size()) {
        std::string ffp = g_SystemFonts[g_SelectedFallbackFontIndex].path;
        std::string ffescaped;
        for(char c : ffp) {
            if(c == '\\') ffescaped += "\\\\";
            else ffescaped += c;
        }
        out << "  \"fallbackFontPath\": \"" << ffescaped << "\",\n";
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
                      // Save temp
                      std::string configDir = Utils::GetConfigDir();
                      std::string tempPath = (std::filesystem::path(configDir) / "temp_pattern.png").string();
                      if (BitmapUtils::SaveImage(tempPath, pat.width, pat.height, pat.pixels)) {
                          BitmapUtils::ClearPatternCache();
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

    // Replaced Glyphs
    g_ReplacedGlyphs.clear();
    try {
        auto j = nlohmann::json::parse(c, nullptr, false);
        if (!j.is_discarded() && j.contains("replacedGlyphs") && j["replacedGlyphs"].is_array()) {
            for (const auto& item : j["replacedGlyphs"]) {
                 if (item.contains("code") && item.contains("path")) {
                     uint32_t code = item["code"];
                     ReplacedGlyph rg;
                     rg.imagePath = item["path"];
                     rg.width = item.value("width", 0);
                     rg.height = item.value("height", 0);
                     rg.xOffset = item.value("xOffset", 0);
                     rg.yOffset = item.value("yOffset", 0);
                     rg.advance = item.value("advance", 0);
                     rg.applyEffects = item.value("applyEffects", false);
                     rg.applyShadow = item.value("applyShadow", true);
                     rg.applyStroke = item.value("applyStroke", true);
                     rg.applyBevel = item.value("applyBevel", true);
                     rg.applyInnerGlow = item.value("applyInnerGlow", true);
                     rg.applyPattern = item.value("applyPattern", true);
                     rg.applyFill = item.value("applyFill", false);
                     g_ReplacedGlyphs[code] = rg;
                 }
            }
        }
    } catch (...) {}

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

    // Fallback Font Path
    std::string fbfp = Utils::ParseStringValue(c, "fallbackFontPath");
    g_SelectedFallbackFontIndex = -1;
    g_FontManager.ClearFallbackFont();
    if(!fbfp.empty()) {
        for(size_t i=0; i<g_SystemFonts.size(); i++) {
            if(g_SystemFonts[i].path == fbfp) {
                g_SelectedFallbackFontIndex = (int)i;
                g_FontManager.LoadFallbackFont(fbfp);
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

} // namespace Utils
