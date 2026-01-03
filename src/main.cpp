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

// --- State Variables ---
static std::vector<Utils::FontInfo> g_SystemFonts;
static FontManager g_FontManager;
static int g_SelectedFontIndex = -1;
static char g_InputText[1024] = "Hello Everyone";
static int g_FontSize = 72;
static int g_Padding = 5;

// Atlas Size
static int g_AtlasWidth = 1024;
static int g_AtlasHeight = 1024;

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
static int g_ExportFormat = 0; // 0=XML, 1=Text, 2=Binary
static bool g_UseExtendedCharset = false;
static bool g_UseCustomGlyphs = false;
static char g_CustomGlyphsText[4096] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!№;%:?*()_+-=.,/|\"'@#$^&{}[]";
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

// --- Custom Glyphs Presets ---
std::string GetCustomGlyphsDir() {
    std::string dir = Utils::GetConfigDir() + "/customglyphs";
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
// --- Window & Style Persistence ---
void SaveWindowConfig(int x, int y, int w, int h, bool ssaa) {
    std::string path = Utils::GetConfigDir() + "/window.cfg";
    std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath);
    if (out.is_open()) {
        out << w << " " << h << " " << (ssaa ? 1 : 0) << " " << x << " " << y;
        out.close();
        std::remove(path.c_str());
        std::rename(tmpPath.c_str(), path.c_str());
    }
}

void LoadWindowConfig(int& x, int& y, int& w, int& h, bool& ssaa) {
    // Defaults matching center-ish usually, or rely on OS
    x = 100; y = 100; w = 1280; h = 720; ssaa = false;

    std::ifstream in(Utils::GetConfigDir() + "/window.cfg");
    if (in.is_open()) {
        int tempSSAA = 0;
        in >> w >> h >> tempSSAA;
        ssaa = (tempSSAA == 1);
        
        // Try reading pos if available (backward compat: check valid read)
        if (!(in >> x >> y)) {
            x = 100; y = 100; // Reset if not found
        }
    }
    
    if (w < 800) w = 800;
    if (h < 600) h = 600;
    
    // Safety check for minimized/off-screen coordinates
    if (x <= -32000 || y <= -32000) {
        x = 100; 
        y = 100;
    }
}

// Simple JSON helpers
void SaveStyle(const std::string& path);
void LoadStyle(const std::string& path);
// Custom Knob Widget
bool KnobAngle(const char* label, float* p_value, float min_v, float max_v) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    
    float radius_outer = 12.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(pos.x + radius_outer, pos.y + radius_outer);
    float line_height = ImGui::GetTextLineHeight();
    
    ImGui::PushID(label);

    // Interaction size
    ImGui::InvisibleButton("##knob", ImVec2(radius_outer*2, radius_outer*2));
    
    bool value_changed = false;
    bool is_active = ImGui::IsItemActive();
    bool is_hovered = ImGui::IsItemHovered();
    
    if (is_active) {
        ImVec2 d = ImVec2(io.MousePos.x - center.x, io.MousePos.y - center.y);
        if (d.x*d.x + d.y*d.y > 0) {
            float angle = std::atan2(d.y, d.x) * 180.0f / 3.14159265f;
            *p_value = angle;
            value_changed = true;
        }
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircleFilled(center, radius_outer, ImGui::GetColorU32(ImGuiCol_FrameBg), 16);
    draw_list->AddCircle(center, radius_outer, ImGui::GetColorU32(is_active ? ImGuiCol_FrameBgActive : is_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_Border), 16);
    
    float angle_rad = (*p_value) * 3.14159265f / 180.0f;
    ImVec2 indicator_end = ImVec2(center.x + std::cos(angle_rad) * (radius_outer - 3), center.y + std::sin(angle_rad) * (radius_outer - 3));
    draw_list->AddLine(center, indicator_end, ImGui::GetColorU32(ImGuiCol_Text), 2.0f);

    // Label and Value to the Right
    const char* label_end = strstr(label, "##");
    std::string text_display = (label_end) ? std::string(label, label_end) : std::string(label);
    ImVec2 text_size = ImGui::CalcTextSize(text_display.c_str());
    
    char val_buf[32]; 
    sprintf(val_buf, "%.0f deg", *p_value);
    
    ImVec2 text_pos = ImVec2(pos.x + radius_outer*2 + style.ItemSpacing.x, pos.y);
    draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), text_display.c_str());
    
    // Buttons (Next to Label)
    ImVec2 btn_pos = ImVec2(text_pos.x + text_size.x + 20, text_pos.y - 3);
    ImGui::SetCursorScreenPos(btn_pos);
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
    
    if (ImGui::Button("0", ImVec2(24, 19))) { *p_value = 0.0f; value_changed = true; } ImGui::SameLine();
    if (ImGui::Button("90", ImVec2(24, 19))) { *p_value = 90.0f; value_changed = true; } ImGui::SameLine();
    if (ImGui::Button("180", ImVec2(32, 19))) { *p_value = 180.0f; value_changed = true; } ImGui::SameLine();
    if (ImGui::Button("-90", ImVec2(32, 19))) { *p_value = -90.0f; value_changed = true; }

    ImGui::PopStyleVar(2);
    ImGui::PopID();
    
    // Value (Below Label)
    ImVec2 val_pos = ImVec2(text_pos.x, text_pos.y + line_height + 4);
    draw_list->AddText(val_pos, ImGui::GetColorU32(ImGuiCol_Text), val_buf);
    
    // Reserve total height
    // Button row ~ 16px. Value row ~ 13px. Total ~ 30px.
    float total_height = 34.0f; // Safe margin
    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(0, total_height + style.ItemSpacing.y));
    
    return value_changed;
}

void UpdatePreview(const char* text); // Forward checking

// Implementation of Style Save/Load (simplified)
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
    
    // Custom Glyphs
    out << "  \"useCustomGlyphs\": " << (g_UseCustomGlyphs ? "true" : "false") << ",\n";
    std::string escCustom;
    for(char c : std::string(g_CustomGlyphsText)) {
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
    size_t end = content.find("]", start);
    if(start == std::string::npos || end == std::string::npos) return;
    std::string arr = content.substr(start+1, end-start-1);
    std::stringstream ss(arr);
    std::string seg;
    int idx=0;
    while(std::getline(ss, seg, ',') && idx < 3) {
        try { col[idx++] = std::stof(seg); } catch(...) {}
    }
}
void ParseColor4(const std::string& content, const std::string& key, float* col) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    size_t end = content.find("]", start);
    if(start == std::string::npos || end == std::string::npos) return;
    std::string arr = content.substr(start+1, end-start-1);
    std::stringstream ss(arr);
    std::string seg;
    int idx=0;
    while(std::getline(ss, seg, ',') && idx < 4) {
        try { col[idx++] = std::stof(seg); } catch(...) {}
    }
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
        float p = ParseFloatValue(item, "p", 0.0f);
        float c[4] = {1,1,1,1};
        ParseColor4(item, "c", c);
        
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
    
    g_FontSize = ParseIntValue(c, "fontSize", g_FontSize);
    g_Padding = ParseIntValue(c, "padding", g_Padding);
    g_AtlasWidth = ParseIntValue(c, "atlasWidth", ParseIntValue(c, "atlasSize", 1024));
    g_AtlasHeight = ParseIntValue(c, "atlasHeight", ParseIntValue(c, "atlasSize", 1024));
    
    ParseColor4(c, "fillColor", g_FillColor);
    g_EnableGradient = ParseBoolValue(c, "enableGradient", g_EnableGradient);
    
    g_FillGradientType = ParseIntValue(c, "fillGradientType", 0);
    g_FillGradientAngle = ParseFloatValue(c, "fillGradientAngle", 90.0f);
    ParseGradientStops(c, "fillGradientStops", g_FillGradientWidget);
    
    g_EnableStroke = ParseBoolValue(c, "enableStroke", false);
    g_StrokeWidth = ParseFloatValue(c, "strokeWidth", 2.0f);
    ParseColor4(c, "strokeColor", g_StrokeColor);
    
    g_EnableStrokeGradient = ParseBoolValue(c, "enableStrokeGradient", false);
    g_StrokeGradientType = ParseIntValue(c, "strokeGradientType", 0);
    g_StrokeGradientAngle = ParseFloatValue(c, "strokeGradientAngle", 90.0f);
    ParseGradientStops(c, "strokeGradientStops", g_StrokeGradientWidget);
    
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
    
    g_UseCustomGlyphs = ParseBoolValue(c, "useCustomGlyphs", false);
    std::string customText = ParseStringValue(c, "customGlyphsText");
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
        strncpy(g_CustomGlyphsText, unesc.c_str(), sizeof(g_CustomGlyphsText));
    }
    
    g_ExportFormat = ParseIntValue(c, "exportFormat", 0);

    g_GlobalXAdvance = ParseIntValue(c, "globalXAdvance", 0);
    g_GlobalXOffset = ParseIntValue(c, "globalXOffset", 0);
    g_GlobalYOffset = ParseIntValue(c, "globalYOffset", 0);

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
        std::vector<uint32_t> charset;
        
        if (g_UseCustomGlyphs) {
            // From custom text
            std::string text(g_CustomGlyphsText);
            // Convert to utf32 (simple check for now, assumes ascii/basic or use helper)
            // But FontManager expects unicode code points.
            // Let's use a simple iterator. For advanced unicode, need a proper library or helper.
            // Assuming the util helper handles UTF-8 strings:
            // Actually let's use a helper if available, or just iterate chars for now (ASCII/Latin-1)
            // For full unicode support in custom text logic, we need to decode UTF-8 `g_CustomGlyphsText`
            
            // Minimal UTF-8 decoder lambda
            auto decodeUtf8 = [](const char* p) -> std::vector<uint32_t> {
                std::vector<uint32_t> res;
                while (*p) {
                    uint32_t c = 0;
                    if ((*p & 0x80) == 0) { c = *p++; }
                    else if ((*p & 0xE0) == 0xC0) { c = (*p++ & 0x1F) << 6; c |= (*p++ & 0x3F); }
                    else if ((*p & 0xF0) == 0xE0) { c = (*p++ & 0x0F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
                    else if ((*p & 0xF8) == 0xF0) { c = (*p++ & 0x07) << 18; c |= (*p++ & 0x3F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
                    else { p++; continue; } // Invalid
                    res.push_back(c);
                }
                return res;
            };
            
            charset = decodeUtf8(g_CustomGlyphsText);
            
            // Remove duplicates
            std::sort(charset.begin(), charset.end());
            charset.erase(std::unique(charset.begin(), charset.end()), charset.end());
            
        } else {
             charset = GenerateCharsetFromBlocks(g_UnicodeBlocks);
        }
        
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

    // Load Window Config from imgui.ini (geometry) and window.cfg (custom)
    int winW = 1280, winH = 720, winX = 100, winY = 100;
    LoadWindowConfig(winX, winY, winW, winH, g_PreviewSSAA);

    GLFWwindow* window = glfwCreateWindow(winW, winH, "Fnt Generator", NULL, NULL);
    if (window == NULL)
        return 1;
        
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
                if (ImGui::MenuItem("Import Project...")) {
                     std::string file = Utils::PickFileDialog("JSON Project\0*.json\0");
                     if (!file.empty()) {
                         LoadStyle(file);
                         AddRecentStyle(file);
                     }
                }
                if (ImGui::MenuItem("Save Project...")) {
                     std::string file = Utils::SaveFileDialog("JSON Project\0*.json\0", "project.json");
                     if (!file.empty()) {
                         if(file.find(".json") == std::string::npos) file += ".json";
                         SaveStyle(file);
                         AddRecentStyle(file);
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
                
                // Unicode Blocks & Custom Glyphs
                ImGui::Separator();
                if (ImGui::Checkbox("Custom Glyphs", &g_UseCustomGlyphs)) {
                     UpdatePreview(g_InputText);
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    strncpy(g_CustomGlyphsText, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!№;%:?*()_+-=.,/|\"'@#$^&{}[]", sizeof(g_CustomGlyphsText) - 1);
                    UpdatePreview(g_InputText);
                }
                ImGui::SameLine();
                
                // Save Button (Icon)
                ImVec2 p = ImGui::GetCursorScreenPos();
                if (ImGui::Button("##SavePreset", ImVec2(24, 24))) {
                    ImGui::OpenPopup("SavePresetPopup");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save Preset");
                
                // Draw Save Icon (Floppy)
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                float x = p.x + 4, y = p.y + 4; // Margin
                ImU32 iconCol = ImGui::GetColorU32(ImGuiCol_Text);
                draw_list->AddRect(ImVec2(x, y), ImVec2(x+16, y+16), iconCol, 1.0f); // Body
                draw_list->AddRectFilled(ImVec2(x+4, y), ImVec2(x+12, y+5), iconCol); // Top shutter
                draw_list->AddRectFilled(ImVec2(x+3, y+10), ImVec2(x+13, y+16), iconCol); // Sticker
                
                ImGui::SameLine();
                
                // Load Button (Icon)
                p = ImGui::GetCursorScreenPos();
                if (ImGui::Button("##LoadPreset", ImVec2(24, 24))) {
                     ImGui::OpenPopup("LoadPresetPopup");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Load Preset");

                // Draw Load Icon (Folder)
                x = p.x + 4; y = p.y + 4;
                draw_list->AddRectFilled(ImVec2(x, y+2), ImVec2(x+16, y+14), iconCol, 1.0f); // Body
                draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x+6, y+3), iconCol, 1.0f); // Tab
                draw_list->AddLine(ImVec2(x, y+4), ImVec2(x+16, y+4), ImGui::GetColorU32(ImGuiCol_WindowBg), 1.0f); // Gap hint

                if (ImGui::BeginPopup("SavePresetPopup")) {
                    static char presetName[64] = "";
                    ImGui::InputText("Name", presetName, 64);
                    if (ImGui::Button("Save")) {
                        SaveCustomGlyphsPreset(presetName, g_CustomGlyphsText);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                if (ImGui::BeginPopup("LoadPresetPopup")) {
                    // Ensure minimum width
                    ImGui::Dummy(ImVec2(200, 0));
                    
                    std::vector<std::string> presets = GetCustomGlyphsPresets();
                    if (presets.empty()) {
                        ImGui::TextDisabled("No presets found");
                    } else {
                        float winWidth = ImGui::GetWindowSize().x;
                        float btnSize = 20.0f;
                        float spacing = ImGui::GetStyle().ItemSpacing.x;
                        
                        for (int i = 0; i < presets.size(); ++i) {
                            ImGui::PushID(i);
                            std::string p = presets[i];
                            
                            // Selectable width = Window Width - Button Size - Scrollbar margin(approx)
                            // Actually GetContentRegionAvail works well if window is sized.
                            float avail = ImGui::GetContentRegionAvail().x;
                            float btnOnlyWidth = 20.0f;
                            float spacing = ImGui::GetStyle().ItemSpacing.x;
                            float selWidth = avail - btnOnlyWidth - spacing;
                            
                            // Ensure selectable has a minimum width
                            if (selWidth < 10) selWidth = 10;
                            
                            // Match height to FrameHeight for consistent alignment
                            float itemHeight = ImGui::GetFrameHeight();

                            if (ImGui::Selectable(p.c_str(), false, 0, ImVec2(selWidth, itemHeight))) {
                                std::string content = LoadCustomGlyphsPreset(p);
                                if (!content.empty()) {
                                    strncpy(g_CustomGlyphsText, content.c_str(), sizeof(g_CustomGlyphsText)-1);
                                    UpdatePreview(g_InputText);
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                            
                            ImGui::SameLine();
                            
                            // Delete ("X") - sized to match height
                            if (ImGui::Button("X", ImVec2(btnOnlyWidth, itemHeight))) {
                                DeleteCustomGlyphsPreset(p);
                                ImGui::PopID();
                                break; 
                            }
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete Preset");
                            
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndPopup();
                }
                
                if (g_UseCustomGlyphs) {
                    // Only show editor if custom is checked
                    ImGui::InputTextMultiline("##CustomGlyphs", g_CustomGlyphsText, IM_ARRAYSIZE(g_CustomGlyphsText), ImVec2(-1, 200));
                    if(ImGui::IsItemDeactivatedAfterEdit()) UpdatePreview(g_InputText);
                    ImGui::TextDisabled("(Newlines are ignored. You can press Enter to wrap text manually)");
                } else {
                    // Scrollable list of blocks
                    if (ImGui::Button("Select All")) {
                        for(auto& b : g_UnicodeBlocks) b.enabled = true;
                        UpdatePreview(g_InputText);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Deselect All")) {
                        for(auto& b : g_UnicodeBlocks) b.enabled = false;
                        UpdatePreview(g_InputText);
                    }
                    
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
                if (KnobAngle("Angle##Fill", &g_FillGradientAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);
                
                ImGui::Dummy(ImVec2(0, 5));
                if (g_FillGradientWidget.widget("Fill Gradient")) {
                    UpdatePreview(g_InputText);
                }
                ImGui::Dummy(ImVec2(0, 10));
            }

            // Effects
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Effects: Outline")) {
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
                         if (KnobAngle("Angle##Stroke", &g_StrokeGradientAngle, -180.0f, 180.0f)) UpdatePreview(g_InputText);

                         ImGui::Dummy(ImVec2(0, 5));
                         if (g_StrokeGradientWidget.widget("Stroke Gradient")) {
                             UpdatePreview(g_InputText);
                         }
                         ImGui::Dummy(ImVec2(0, 10));
                    }
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
                         std::vector<uint32_t> charset;
                         if (g_UseCustomGlyphs) {
                            auto decodeUtf8 = [](const char* p) -> std::vector<uint32_t> {
                                std::vector<uint32_t> res;
                                while (*p) {
                                    uint32_t c = 0;
                                    if ((*p & 0x80) == 0) { c = *p++; }
                                    else if ((*p & 0xE0) == 0xC0) { c = (*p++ & 0x1F) << 6; c |= (*p++ & 0x3F); }
                                    else if ((*p & 0xF0) == 0xE0) { c = (*p++ & 0x0F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
                                    else if ((*p & 0xF8) == 0xF0) { c = (*p++ & 0x07) << 18; c |= (*p++ & 0x3F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
                                    else { p++; continue; }
                                    if (c < 32) continue; // Ignore control characters (newlines, tabs, etc)
                                    res.push_back(c);
                                }
                                return res;
                            };
                            charset = decodeUtf8(g_CustomGlyphsText);
                            std::sort(charset.begin(), charset.end());
                            charset.erase(std::unique(charset.begin(), charset.end()), charset.end());
                         } else {
                            charset = GenerateCharsetFromBlocks(g_UnicodeBlocks);
                         }

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
                                    if (g_UseCustomGlyphs) {
                                        // Remove from custom text string directly
                                        std::string s(g_CustomGlyphsText);
                                        // Simple remove all instances of this char code (utf-8 aware would be better but for now naive remove)
                                        // We need to re-encode the char to UTF-8 to find it? Or just iterate.
                                        
                                        // Better approach: Reconstruct string without this char.
                                        std::string newS;
                                        // Use the decode helper again to iterate properly? Or just manual.
                                        // Let's iterate byte by byte and decode, skipping the target char.
                                        const char* p = g_CustomGlyphsText;
                                        while (*p) {
                                            const char* start = p;
                                            uint32_t c = 0;
                                            if ((*p & 0x80) == 0) { c = *p++; }
                                            else if ((*p & 0xE0) == 0xC0) { c = (*p++ & 0x1F) << 6; c |= (*p++ & 0x3F); }
                                            else if ((*p & 0xF0) == 0xE0) { c = (*p++ & 0x0F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
                                            else if ((*p & 0xF8) == 0xF0) { c = (*p++ & 0x07) << 18; c |= (*p++ & 0x3F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
                                            else { p++; continue; } 
                                            
                                            if (c != g.charCode) {
                                                // Append original bytes
                                                newS.append(start, p - start);
                                            }
                                        }
                                        
                                        strncpy(g_CustomGlyphsText, newS.c_str(), sizeof(g_CustomGlyphsText) - 1);
                                        g_CustomGlyphsText[sizeof(g_CustomGlyphsText) - 1] = '\0'; // Ensure null term

                                    } else {
                                        g_ExcludedGlyphs.insert(g.charCode);
                                    }
                                    
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
    int curX, curY;
    glfwGetWindowSize(window, &curW, &curH);
    glfwGetWindowPos(window, &curX, &curY);
    SaveWindowConfig(curX, curY, curW, curH, g_PreviewSSAA);

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
