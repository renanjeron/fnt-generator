#pragma once

#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <nlohmann/json.hpp>

namespace Utils {

    struct SettingsData {
        std::string theme = "Dark";
        std::set<std::string> favorites;
        std::vector<std::string> recents;
    };

    class SettingsManager {
    public:
        static SettingsManager& Get();

        void Load();
        void Save();

        // Theme
        std::string GetTheme() const;
        void SetTheme(const std::string& theme);

        // Favorites
        std::set<std::string> GetFavorites() const;
        void SetFavorites(const std::set<std::string>& favorites);
        void AddFavorite(const std::string& fontPath);
        void RemoveFavorite(const std::string& fontPath);
        bool IsFavorite(const std::string& fontPath) const;

        // Recents
        std::vector<std::string> GetRecents() const;
        void SetRecents(const std::vector<std::string>& recents);
        void AddRecent(const std::string& stylePath);
        void RemoveRecent(const std::string& stylePath);

    private:
        SettingsManager();
        ~SettingsManager() = default;
        
        // No copy
        SettingsManager(const SettingsManager&) = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;

        std::string GetSettingsPath() const;
        void MigrateOldFiles();

        SettingsData m_Data;
        mutable std::mutex m_Mutex;
    };

}
