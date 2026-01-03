#include "StyleUtils.h"
#include "PlatformUtils.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace Utils {

// Favorites
std::string GetFavoritesPath() { 
    return GetConfigDir() + "/font_favorites.txt"; 
}

void LoadFavorites(std::set<std::string>& favorites) {
    std::ifstream file(GetFavoritesPath());
    if (file.is_open()) {
        std::set<std::string> temp;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) temp.insert(line);
        }
        if (!temp.empty()) favorites = temp;
        file.close();
    }
}

void SaveFavorites(const std::set<std::string>& favorites) {
    std::string path = GetFavoritesPath();
    std::string tmpPath = path + ".tmp";
    std::ofstream file(tmpPath);
    if (file.is_open()) {
        for (const auto& fontPath : favorites) {
            file << fontPath << "\n";
        }
        file.close();
        std::remove(path.c_str());
        std::rename(tmpPath.c_str(), path.c_str());
    }
}

// Recents
std::string GetRecentsPath() { 
    return GetConfigDir() + "/style_recents.txt"; 
}

void LoadRecentStyles(std::vector<std::string>& recents) {
    std::ifstream in(GetRecentsPath());
    if (in.is_open()) {
        std::vector<std::string> temp;
        std::string line;
        while(std::getline(in, line)) {
            if(!line.empty()) temp.push_back(line);
        }
        if (!temp.empty()) recents = temp;
        in.close();
    }
}

void SaveRecentStyles(const std::vector<std::string>& recents) {
    std::string path = GetRecentsPath();
    std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath);
    if (out.is_open()) {
        for(const auto& s : recents) out << s << "\n";
        out.close();
        std::remove(path.c_str());
        std::rename(tmpPath.c_str(), path.c_str());
    }
}

void AddRecentStyle(std::vector<std::string>& recents, const std::string& path) {
    if (path.empty()) return;
    auto it = std::remove(recents.begin(), recents.end(), path);
    recents.erase(it, recents.end());
    recents.insert(recents.begin(), path);
    if (recents.size() > 10) recents.resize(10);
    SaveRecentStyles(recents);
}

void RemoveRecentStyle(std::vector<std::string>& recents, const std::string& path) {
    auto it = std::remove(recents.begin(), recents.end(), path);
    recents.erase(it, recents.end());
    SaveRecentStyles(recents);
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
void SaveWindowConfig(int x, int y, int w, int h, bool ssaa) {
    std::string path = GetConfigDir() + "/window.cfg";
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
    x = 100; y = 100; w = 1280; h = 720; ssaa = false;
    std::ifstream in(GetConfigDir() + "/window.cfg");
    if (in.is_open()) {
        int tempSSAA = 0;
        in >> w >> h >> tempSSAA;
        ssaa = (tempSSAA == 1);
        if (!(in >> x >> y)) {
            x = 100; y = 100;
        }
    }
    if (w < 800) w = 800;
    if (h < 600) h = 600;
    if (x <= -32000 || y <= -32000) { x = 100; y = 100; }
}

} // namespace Utils
