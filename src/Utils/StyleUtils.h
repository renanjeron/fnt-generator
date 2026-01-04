#pragma once
#include <string>
#include <vector>
#include <set>
#include <cstdint>

namespace Utils {
    // Favorites
    std::string GetFavoritesPath();
    void LoadFavorites(std::set<std::string>& favorites);
    void SaveFavorites(const std::set<std::string>& favorites);

    // Recents
    std::string GetRecentsPath();
    void LoadRecentStyles(std::vector<std::string>& recents);
    void SaveRecentStyles(const std::vector<std::string>& recents);
    void AddRecentStyle(std::vector<std::string>& recents, const std::string& path);
    void RemoveRecentStyle(std::vector<std::string>& recents, const std::string& path);

    // Custom Glyphs Presets
    std::string GetCustomGlyphsDir();
    std::vector<std::string> GetCustomGlyphsPresets();
    void SaveCustomGlyphsPreset(const std::string& name, const std::string& content);
    void DeleteCustomGlyphsPreset(const std::string& name);
    std::string LoadCustomGlyphsPreset(const std::string& name);

    // Window Config
    void SaveWindowConfig(int x, int y, int w, int h, int ssaaFactor);
    void LoadWindowConfig(int& x, int& y, int& w, int& h, int& ssaaFactor);
}
