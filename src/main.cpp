#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <set>

#include "Utils/PlatformUtils.h"
#include "Font/FontManager.h"
#include "Atlas/TextureGenerator.h"
#include "Utils/Exporter.h"
#include "Utils/UnicodeBlocks.h"

// --- State Variables ---
static std::vector<Utils::FontInfo> g_SystemFonts;
static FontManager g_FontManager;
static int g_SelectedFontIndex = -1;
static char g_InputText[1024] = "Entrar";
static int g_FontSize = 72;
static int g_Padding = 5;

// New: Atlas Size
static int g_AtlasSize = 1024;

// Fill
static float g_FillColor[3] = {1.0f, 1.0f, 1.0f};
static bool g_EnableGradient = false;
static float g_GradientStart[3] = {1.0f, 1.0f, 1.0f};
static float g_GradientEnd[3] = {0.5f, 0.5f, 0.5f};

// Stroke
static bool g_EnableStroke = false;
static bool g_EnableStrokeGradient = false;
static float g_StrokeWidth = 2.0f;
static float g_StrokeColor[3] = {0.0f, 0.0f, 0.0f};
static float g_StrokeColorBottom[3] = {0.0f, 0.0f, 0.0f};

// Shadow
static bool g_EnableShadow = false;
static int g_ShadowOffsetX = 5;
static int g_ShadowOffsetY = 5;
static int g_ShadowBlur = 0;
static float g_ShadowColor[4] = {0.0f, 0.0f, 0.0f, 0.5f}; // RGBA

// Bevel
static bool g_EnableBevel = false;
static int g_BevelAngle = 135;
static float g_BevelDistance = 2.0f;
static int g_BevelBlur = 0;
static float g_BevelColor[3] = {1.0f, 1.0f, 1.0f}; // RGB

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
static bool g_UseExtendedCharset = false;
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

// Favorite fonts system
static std::set<std::string> g_FavoriteFonts;
std::string GetFavoritesPath() { return Utils::GetConfigDir() + "/font_favorites.txt"; }

// Load favorites from file
void LoadFavorites() {
    std::ifstream file(GetFavoritesPath());
    if (file.is_open()) {
        std::set<std::string> temp;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) temp.insert(line);
        }
        if (!temp.empty()) g_FavoriteFonts = temp;
        file.close();
    }
}

// Save favorites to file
void SaveFavorites() {
    std::string path = GetFavoritesPath();
    std::string tmpPath = path + ".tmp";
    std::ofstream file(tmpPath);
    if (file.is_open()) {
        for (const auto& fontPath : g_FavoriteFonts) {
            file << fontPath << "\n";
        }
        file.close();
        
        // Atomic-like swap
        std::remove(path.c_str());
        std::rename(tmpPath.c_str(), path.c_str());
    }
}

// --- Recent Styles ---
static std::vector<std::string> g_RecentStyles;
std::string GetRecentsPath() { return Utils::GetConfigDir() + "/style_recents.txt"; }
static bool g_ShowRecentError = false;
static std::string g_RecentNotFoundPath = "";

void LoadRecentStyles() {
    std::ifstream in(GetRecentsPath());
    if (in.is_open()) {
        std::vector<std::string> temp;
        std::string line;
        while(std::getline(in, line)) {
            if(!line.empty()) temp.push_back(line);
        }
        if (!temp.empty()) g_RecentStyles = temp;
        in.close();
    }
}

void SaveRecentStyles() {
    std::string path = GetRecentsPath();
    std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath);
    if (out.is_open()) {
        for(const auto& s : g_RecentStyles) out << s << "\n";
        out.close();

        // Atomic-like swap
        std::remove(path.c_str());
        std::rename(tmpPath.c_str(), path.c_str());
    }
}

void AddRecentStyle(std::string path) {
    if (path.empty()) return;
    // Remove if already exists to move to top
    auto it = std::remove(g_RecentStyles.begin(), g_RecentStyles.end(), path);
    g_RecentStyles.erase(it, g_RecentStyles.end());
    // Insert at front
    g_RecentStyles.insert(g_RecentStyles.begin(), path);
    if (g_RecentStyles.size() > 10) g_RecentStyles.resize(10);
    SaveRecentStyles();
}

void RemoveRecentStyle(std::string path) {
    auto it = std::remove(g_RecentStyles.begin(), g_RecentStyles.end(), path);
    g_RecentStyles.erase(it, g_RecentStyles.end());
    SaveRecentStyles();
}

// --- Window & Style Persistence ---
void SaveWindowConfig(int w, int h) {
    std::string path = Utils::GetConfigDir() + "/window.cfg";
    std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath);
    if (out.is_open()) {
        out << w << " " << h << " " << (g_PreviewSSAA ? 1 : 0);
        out.close();
        std::remove(path.c_str());
        std::rename(tmpPath.c_str(), path.c_str());
    }
}

void LoadWindowConfig(int& w, int& h) {
    std::ifstream in(Utils::GetConfigDir() + "/window.cfg");
    if (in.is_open()) {
        in >> w >> h;
        if (w < 800) w = 800; 
        if (h < 600) h = 600;
        int ssaa = 0;
        if (in >> ssaa) g_PreviewSSAA = (ssaa == 1);
    }
}

// Simple JSON helpers
void SaveStyle(const std::string& path);
void LoadStyle(const std::string& path);
void UpdatePreview(const char* text); // Forward checking

// Implementation of Style Save/Load (simplified)
// Implementation of Style Save/Load (simplified)
void SaveStyle(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) return;
    
    out << "{\n";
    out << "  \"fontSize\": " << g_FontSize << ",\n";
    out << "  \"padding\": " << g_Padding << ",\n";
    out << "  \"atlasSize\": " << g_AtlasSize << ",\n";
    out << "  \"fillColor\": [" << g_FillColor[0] << ", " << g_FillColor[1] << ", " << g_FillColor[2] << "],\n";
    out << "  \"enableGradient\": " << (g_EnableGradient ? "true" : "false") << ",\n";
    out << "  \"gradientStart\": [" << g_GradientStart[0] << ", " << g_GradientStart[1] << ", " << g_GradientStart[2] << "],\n";
    out << "  \"gradientEnd\": [" << g_GradientEnd[0] << ", " << g_GradientEnd[1] << ", " << g_GradientEnd[2] << "],\n";
    
    out << "  \"enableStroke\": " << (g_EnableStroke ? "true" : "false") << ",\n";
    out << "  \"enableStrokeGradient\": " << (g_EnableStrokeGradient ? "true" : "false") << ",\n";
    out << "  \"strokeWidth\": " << g_StrokeWidth << ",\n";
    out << "  \"strokeColor\": [" << g_StrokeColor[0] << ", " << g_StrokeColor[1] << ", " << g_StrokeColor[2] << "],\n";
    out << "  \"strokeColorBottom\": [" << g_StrokeColorBottom[0] << ", " << g_StrokeColorBottom[1] << ", " << g_StrokeColorBottom[2] << "],\n";
    
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
    out << "  \"bevelColor\": [" << g_BevelColor[0] << ", " << g_BevelColor[1] << ", " << g_BevelColor[2] << "],\n";

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

// Naive parser helper
std::string ParseStringValue(const std::string& content, const std::string& key) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    size_t start = content.find("\"", pos + key.length() + 2); // after "key":
    if (start == std::string::npos) return "";
    start++;
    size_t end = content.find("\"", start);
    
    std::string val = content.substr(start, end - start);
    // Unescape basic
    std::string unescaped;
    for(size_t i=0; i<val.length(); i++) {
        if(val[i] == '\\' && i+1 < val.length() && val[i+1] == '\\') {
            unescaped += '\\';
            i++;
        } else {
            unescaped += val[i];
        }
    }
    return unescaped;
}
float ParseFloatValue(const std::string& content, const std::string& key, float def) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    size_t start = content.find(":", pos);
    if (start == std::string::npos) return def;
    
    size_t end = content.find_first_of(",}\n", start);
    if (end == std::string::npos) end = content.length();
    
    try {
        return std::stof(content.substr(start + 1, end - (start + 1)));
    } catch (...) {
        return def;
    }
}
int ParseIntValue(const std::string& content, const std::string& key, int def) {
    return (int)ParseFloatValue(content, key, (float)def);
}
bool ParseBoolValue(const std::string& content, const std::string& key, bool def) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    size_t start = content.find(":", pos);
    if (start == std::string::npos) return def;
    
    size_t end = content.find_first_of(",}\n", start);
    if (end == std::string::npos) end = content.length();
    
    std::string val = content.substr(start + 1, end - (start + 1));
    return val.find("true") != std::string::npos;
}
void ParseColor3(const std::string& content, const std::string& key, float* col) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    if(start == std::string::npos) return;
    sscanf(content.c_str() + start + 1, "%f, %f, %f", &col[0], &col[1], &col[2]);
}
void ParseColor4(const std::string& content, const std::string& key, float* col) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    if(start == std::string::npos) return;
    sscanf(content.c_str() + start + 1, "%f, %f, %f, %f", &col[0], &col[1], &col[2], &col[3]);
}
void ParseIntArray(const std::string& content, const std::string& key, std::set<uint32_t>& outSet) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    size_t end = content.find("]", start);
    if(start == std::string::npos || end == std::string::npos) return;
    
    std::string arrStr = content.substr(start+1, end-start-1);
    std::stringstream ss(arrStr);
    std::string segment;
    while(std::getline(ss, segment, ',')) {
        try { outSet.insert((uint32_t)std::stoul(segment)); } catch(...) {}
    }
}

void LoadStyle(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string c = buffer.str();
    
    g_FontSize = ParseIntValue(c, "fontSize", g_FontSize);
    g_Padding = ParseIntValue(c, "padding", g_Padding);
    g_AtlasSize = ParseIntValue(c, "atlasSize", g_AtlasSize);
    
    ParseColor3(c, "fillColor", g_FillColor);
    g_EnableGradient = ParseBoolValue(c, "enableGradient", g_EnableGradient);
    ParseColor3(c, "gradientStart", g_GradientStart);
    ParseColor3(c, "gradientEnd", g_GradientEnd);
    
    g_EnableStroke = ParseBoolValue(c, "enableStroke", false);
    g_EnableStrokeGradient = ParseBoolValue(c, "enableStrokeGradient", false);
    g_StrokeWidth = ParseFloatValue(c, "strokeWidth", 2.0f);
    ParseColor3(c, "strokeColor", g_StrokeColor);
    ParseColor3(c, "strokeColorBottom", g_StrokeColorBottom);
    
    g_EnableShadow = ParseBoolValue(c, "enableShadow", g_EnableShadow);
    g_ShadowOffsetX = ParseIntValue(c, "shadowOffsetX", g_ShadowOffsetX);
    g_ShadowOffsetY = ParseIntValue(c, "shadowOffsetY", g_ShadowOffsetY);
    g_ShadowBlur = ParseIntValue(c, "shadowBlur", g_ShadowBlur);
    ParseColor4(c, "shadowColor", g_ShadowColor);

    g_EnableInnerGlow = ParseBoolValue(c, "enableInnerGlow", g_EnableInnerGlow);
    g_InnerGlowSize = ParseFloatValue(c, "innerGlowSize", g_InnerGlowSize);
    g_InnerGlowChoke = ParseFloatValue(c, "innerGlowChoke", g_InnerGlowChoke);
    ParseColor4(c, "innerGlowColor", g_InnerGlowColor);

    g_EnableBevel = ParseBoolValue(c, "enableBevel", g_EnableBevel);
    g_BevelDistance = ParseFloatValue(c, "bevelDistance", g_BevelDistance);
    g_BevelAngle = ParseIntValue(c, "bevelAngle", g_BevelAngle);
    ParseColor3(c, "bevelColor", g_BevelColor);
    
    std::string fname = ParseStringValue(c, "exportFilename");
    if(!fname.empty()) strncpy(g_ExportFilename, fname.c_str(), sizeof(g_ExportFilename));
    
    // Font Path
    std::string fp = ParseStringValue(c, "fontPath");
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
    ParseIntArray(c, "excludedGlyphs", g_ExcludedGlyphs);
    
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
    SaveFavorites();
}

// Check if font is favorite
bool IsFavorite(const std::string& fontPath) {
    return g_FavoriteFonts.count(fontPath) > 0;
}

// Helper to create a checkerboard texture
void CreateCheckerTexture() {
    int w = 32, h = 32;
    std::vector<unsigned char> pixels(w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // 8x8 squares
            bool dark = ((x / 8) + (y / 8)) % 2 == 1;
            uint8_t c = dark ? 204 : 255; // Light gray and White
            int idx = (y * w + x) * 4;
            pixels[idx] = c;
            pixels[idx+1] = c;
            pixels[idx+2] = c;
            pixels[idx+3] = 255;
        }
    }

    glGenTextures(1, &g_CheckerTexture);
    glBindTexture(GL_TEXTURE_2D, g_CheckerTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}

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
    settings.atlasWidth = g_AtlasSize;
    settings.atlasHeight = g_AtlasSize;
    settings.useSuperSampling = false;
    settings.hintingMode = g_HintingMode;
    
    // Fill
    settings.enableGradient = g_EnableGradient;
    if (g_EnableGradient) {
        settings.colorTop[0] = (uint8_t)(g_GradientStart[0] * 255);
        settings.colorTop[1] = (uint8_t)(g_GradientStart[1] * 255);
        settings.colorTop[2] = (uint8_t)(g_GradientStart[2] * 255);
        
        settings.colorBottom[0] = (uint8_t)(g_GradientEnd[0] * 255);
        settings.colorBottom[1] = (uint8_t)(g_GradientEnd[1] * 255);
        settings.colorBottom[2] = (uint8_t)(g_GradientEnd[2] * 255);
    } else {
        settings.colorTop[0] = settings.colorBottom[0] = (uint8_t)(g_FillColor[0] * 255);
        settings.colorTop[1] = settings.colorBottom[1] = (uint8_t)(g_FillColor[1] * 255);
        settings.colorTop[2] = settings.colorBottom[2] = (uint8_t)(g_FillColor[2] * 255);
    }

    // Stroke
    settings.enableStroke = g_EnableStroke;
    settings.enableStrokeGradient = g_EnableStrokeGradient;
    settings.strokeWidth = g_StrokeWidth;
    settings.strokeColor[0] = (uint8_t)(g_StrokeColor[0] * 255);
    settings.strokeColor[1] = (uint8_t)(g_StrokeColor[1] * 255);
    settings.strokeColor[2] = (uint8_t)(g_StrokeColor[2] * 255);
    settings.strokeColorBottom[0] = (uint8_t)(g_StrokeColorBottom[0] * 255);
    settings.strokeColorBottom[1] = (uint8_t)(g_StrokeColorBottom[1] * 255);
    settings.strokeColorBottom[2] = (uint8_t)(g_StrokeColorBottom[2] * 255);

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
    settings.bevelBlur = g_BevelBlur;
    settings.bevelColor[0] = (uint8_t)(g_BevelColor[0] * 255);
    settings.bevelColor[1] = (uint8_t)(g_BevelColor[1] * 255);
    settings.bevelColor[2] = (uint8_t)(g_BevelColor[2] * 255);
    
    // Inner Glow
    settings.enableInnerGlow = g_EnableInnerGlow;
    settings.innerGlowSize = g_InnerGlowSize;
    settings.innerGlowChoke = g_InnerGlowChoke;
    settings.innerGlowColor[0] = (uint8_t)(g_InnerGlowColor[0] * 255);
    settings.innerGlowColor[1] = (uint8_t)(g_InnerGlowColor[1] * 255);
    settings.innerGlowColor[2] = (uint8_t)(g_InnerGlowColor[2] * 255);
    settings.innerGlowColor[3] = (uint8_t)(g_InnerGlowColor[3] * 255);

    return settings;
}

void UpdatePreview(const char* text) {
    if (!g_FontManager.IsLoaded()) return;

    AtlasSettings settings = ConstructSettings();
    settings.useSuperSampling = g_PreviewSSAA;

    // 1. Generate Atlas (For Export)
    // Always uses selected Unicode blocks
    {
        // Generate charset from selected Unicode blocks
        std::vector<uint32_t> charset = GenerateCharsetFromBlocks(g_UnicodeBlocks);
        
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

// Case insensitive substring search
bool StringContains(const std::string& str, const std::string& sub) {
    if (sub.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );
    return (it != str.end());
}

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

    // Load Window Config
    int winW = 1280, winH = 720;
    LoadWindowConfig(winW, winH);

    GLFWwindow* window = glfwCreateWindow(winW, winH, "Font Exporter Native", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; 

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialization
    if (!g_FontManager.Initialize()) {
        printf("Failed to init FontManager\n");
    }
    g_SystemFonts = Utils::GetSystemFonts();
    LoadFavorites(); // Load favorite fonts
    LoadRecentStyles();
    CreateCheckerTexture();

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
                if (ImGui::MenuItem("Export Style...")) {
                     std::string file = Utils::SaveFileDialog("JSON Style\0*.json\0", "style.json");
                     if (!file.empty()) {
                         if(file.find(".json") == std::string::npos) file += ".json";
                         SaveStyle(file);
                         AddRecentStyle(file);
                     }
                }
                if (ImGui::MenuItem("Import Style...")) {
                     std::string file = Utils::PickFileDialog("JSON Style\0*.json\0");
                     if (!file.empty()) {
                         LoadStyle(file);
                         AddRecentStyle(file);
                     }
                }
                
                ImGui::Separator();
                
                if (ImGui::BeginMenu("Recent Styles")) {
                    if(g_RecentStyles.empty()) {
                        ImGui::MenuItem("No recent files", nullptr, false, false);
                    } else {
                        std::vector<std::string> recentsCopy = g_RecentStyles;
                        for(const auto& s : recentsCopy) {
                            if(ImGui::MenuItem(s.c_str())) {
                                std::ifstream fcheck(s);
                                if(fcheck.good()) {
                                    LoadStyle(s);
                                    AddRecentStyle(s);
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
                    if (g_FontSearch[0] != '\0' && !StringContains(g_SystemFonts[idx].name, g_FontSearch)) {
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
            
            // Unicode Blocks Selection
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Unicode Blocks")) {
                // Quick actions
                if (ImGui::Button("Select All##Blocks")) {
                    for (auto& block : g_UnicodeBlocks) {
                        block.enabled = true;
                    }
                    UpdatePreview(g_InputText);
                }
                ImGui::SameLine();
                if (ImGui::Button("Deselect All##Blocks")) {
                    for (auto& block : g_UnicodeBlocks) {
                        block.enabled = false;
                    }
                    UpdatePreview(g_InputText);
                }
                
                // Excluded Glyphs Management
                if (!g_ExcludedGlyphs.empty()) {
                    ImGui::SameLine();
                    char btnLabel[64];
                    sprintf(btnLabel, "Clear Excluded (%d)###ClearExcluded", (int)g_ExcludedGlyphs.size());
                    if (ImGui::Button(btnLabel)) {
                        g_ExcludedGlyphs.clear();
                        UpdatePreview(g_InputText);
                    }
                }
                
                ImGui::Separator();
                
                // Scrollable list of blocks
                ImGui::BeginChild("BlocksList", ImVec2(0, 200), true);
                for (auto& block : g_UnicodeBlocks) {
                    if (ImGui::Checkbox(block.name.c_str(), &block.enabled)) {
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
                 g_FillColor[0]=1; g_FillColor[1]=1; g_FillColor[2]=1;
                 UpdatePreview(g_InputText);
            }
            if (ImGui::Checkbox("Gradient", &g_EnableGradient)) {
                UpdatePreview(g_InputText);
            }
            if (!g_EnableGradient) {
                if (ImGui::ColorEdit3("Color", g_FillColor)) {
                    UpdatePreview(g_InputText);
                }
            } else {
                if (ImGui::ColorEdit3("Start", g_GradientStart)) UpdatePreview(g_InputText);
                if (ImGui::ColorEdit3("End", g_GradientEnd)) UpdatePreview(g_InputText);
            }

            // Effects
            ImGui::Separator();
            ImGui::Text("Effects: Outline");
            if (ImGui::Checkbox("Outline", &g_EnableStroke)) {
                UpdatePreview(g_InputText);
            }
            if (g_EnableStroke) {
                if (ImGui::SliderFloat("Width", &g_StrokeWidth, 0.0f, 10.0f)) UpdatePreview(g_InputText);
                if (ImGui::Checkbox("Stroke Gradient", &g_EnableStrokeGradient)) UpdatePreview(g_InputText);
                if (!g_EnableStrokeGradient) {
                    if (ImGui::ColorEdit3("Line Color", g_StrokeColor)) UpdatePreview(g_InputText);
                } else {
                    if (ImGui::ColorEdit3("Line Color (Top)", g_StrokeColor)) UpdatePreview(g_InputText);
                    if (ImGui::ColorEdit3("Line Color (Bottom)", g_StrokeColorBottom)) UpdatePreview(g_InputText);
                }
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Effects: Shadow")) {
                if (ImGui::Checkbox("Enable Shadow", &g_EnableShadow)) {
                    UpdatePreview(g_InputText);
                }
                if (g_EnableShadow) {
                    if (ImGui::SliderInt("Offset X##Shadow", &g_ShadowOffsetX, -20, 20)) UpdatePreview(g_InputText);
                    if (ImGui::SliderInt("Offset Y##Shadow", &g_ShadowOffsetY, -20, 20)) UpdatePreview(g_InputText);
                    if (ImGui::SliderInt("Blur##Shadow", &g_ShadowBlur, 0, 10)) UpdatePreview(g_InputText);
                    if (ImGui::ColorEdit4("Color##Shadow", g_ShadowColor)) UpdatePreview(g_InputText);
                }
            }
            
            if (ImGui::CollapsingHeader("Effects: Bevel")) {
                if (ImGui::Checkbox("Enable Bevel", &g_EnableBevel)) {
                    UpdatePreview(g_InputText);
                }
                if (g_EnableBevel) {
                    if (ImGui::SliderInt("Angle##Bevel", &g_BevelAngle, 0, 360)) UpdatePreview(g_InputText);
                    if (ImGui::SliderFloat("Distance##Bevel", &g_BevelDistance, 0.0f, 10.0f)) UpdatePreview(g_InputText);
                    if (ImGui::SliderInt("Blur##Bevel", &g_BevelBlur, 0, 10)) UpdatePreview(g_InputText);
                    if (ImGui::ColorEdit3("Color##Bevel", g_BevelColor)) UpdatePreview(g_InputText);
                }
            }
            
            if (ImGui::CollapsingHeader("Effects: Inner Glow")) {
                if (ImGui::Checkbox("Enable Inner Glow", &g_EnableInnerGlow)) {
                    UpdatePreview(g_InputText);
                }
                if (g_EnableInnerGlow) {
                    if (ImGui::SliderFloat("Size##InnerGlowSize", &g_InnerGlowSize, 0.0f, 20.0f)) UpdatePreview(g_InputText);
                    if (ImGui::SliderFloat("Choke##InnerGlow", &g_InnerGlowChoke, 0.0f, 100.0f)) UpdatePreview(g_InputText);
                    if (ImGui::ColorEdit4("Color##InnerGlow", g_InnerGlowColor)) UpdatePreview(g_InputText);
                }
            }

            // Preview Info & Atlas Settings
            ImGui::Separator();
            ImGui::Text("Atlas Settings");
            const char* atlasSizes[] = { "512", "1024", "2048", "4096" };
            static int currentSizeIdx = 1; // 1024 default
            if (ImGui::Combo("Atlas Size", &currentSizeIdx, atlasSizes, IM_ARRAYSIZE(atlasSizes))) {
                g_AtlasSize = atoi(atlasSizes[currentSizeIdx]);
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
            ImGui::Text("Export Settings");
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
                         std::vector<uint32_t> charset = GenerateCharsetFromBlocks(g_UnicodeBlocks);
                         if(charset.empty()) { for(uint32_t c=0x20; c<=0x7E; c++) charset.push_back(c); }
                         
                         if (!g_ExcludedGlyphs.empty()) {
                            std::vector<uint32_t> filtered;
                            for (uint32_t c : charset) if (g_ExcludedGlyphs.find(c) == g_ExcludedGlyphs.end()) filtered.push_back(c);
                            charset = filtered;
                         }

                         AtlasResult hqAtlas = TextureGenerator::GenerateAtlas(g_FontManager, charset, hqSettings);

                         if (!hqAtlas.hasErrors && hqAtlas.width > 0) {
                            std::string fname = (std::string(g_ExportFilename).empty()) ? "font_export" : std::string(g_ExportFilename);
                            if (Exporter::ExportAtlasToDisk(hqAtlas, folder, fname)) {
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
                                    g_ExcludedGlyphs.insert(g.charCode);
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
                RemoveRecentStyle(g_RecentNotFoundPath);
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
    glfwGetWindowSize(window, &curW, &curH);
    SaveWindowConfig(curW, curH);

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
