#include "SettingsManager.h"
#include "StyleUtils.h" // For GetConfigDir
#include "PlatformUtils.h" // For GetConfigDir if needed explicitly, but StyleUtils wraps it
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Utils {

    SettingsManager& SettingsManager::Get() {
        static SettingsManager instance;
        return instance;
    }

    SettingsManager::SettingsManager() {
        // Construct
    }

    std::string SettingsManager::GetSettingsPath() const {
        return GetConfigDir() + "/settings.json";
    }

    void SettingsManager::Load() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        std::string path = GetSettingsPath();
        
        if (!std::filesystem::exists(path)) {
            MigrateOldFiles();
            return;
        }

        std::ifstream file(path);
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;

                if (j.contains("theme")) {
                    m_Data.theme = j["theme"];
                }
                
                if (j.contains("favorites") && j["favorites"].is_array()) {
                    m_Data.favorites.clear();
                    for (const auto& item : j["favorites"]) {
                        m_Data.favorites.insert(item.get<std::string>());
                    }
                }

                if (j.contains("recents") && j["recents"].is_array()) {
                    m_Data.recents.clear();
                    for (const auto& item : j["recents"]) {
                        m_Data.recents.push_back(item.get<std::string>());
                    }
                }
            } catch (...) {
                // Ignore parsing errors, stick to defaults or partial load
            }
        }
    }

    void SettingsManager::Save() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        std::string path = GetSettingsPath();
        
        nlohmann::json j;
        j["theme"] = m_Data.theme;
        j["favorites"] = m_Data.favorites; // Set implicitly converts to array
        j["recents"] = m_Data.recents;

        std::ofstream file(path);
        if (file.is_open()) {
            file << j.dump(4);
        }
    }

    void SettingsManager::MigrateOldFiles() {
        // This is called when settings.json doesn't exist.
        // Try to load from old txt files.
        bool needsSave = false;

        // Favorites
        std::string favPath = GetConfigDir() + "/font_favorites.txt";
        if (std::filesystem::exists(favPath)) {
            std::ifstream file(favPath);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (!line.empty()) m_Data.favorites.insert(line);
                }
                file.close();
                needsSave = true;
            }
        }

        // Recents
        std::string recPath = GetConfigDir() + "/style_recents.txt";
        if (std::filesystem::exists(recPath)) {
            std::ifstream file(recPath);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (!line.empty()) m_Data.recents.push_back(line);
                }
                file.close();
                needsSave = true;
            }
        }

        if (needsSave) {
            // We migrated something, so save immediately.
            // Using internal save method logic (but we already hold the lock effectively since this is private called from Load)
            // Actually Load holds the lock, this is private helper.
            // But wait, MigrateOldFiles IS called from Load which holds lock.
            // BUT Save also locks. Recursive mutex? Or just implement save logic here?
            // Let's implement save logic here to avoid recursive lock issues if mutex isn't recursive.
            // Using m_Mutex which is std::mutex (non-recursive).
            
            std::string path = GetSettingsPath();
            nlohmann::json j;
            j["theme"] = m_Data.theme;
            j["favorites"] = m_Data.favorites;
            j["recents"] = m_Data.recents;

            std::ofstream file(path);
            if (file.is_open()) {
                file << j.dump(4);
            }
        }
    }

    // Theme
    std::string SettingsManager::GetTheme() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Data.theme;
    }

    void SettingsManager::SetTheme(const std::string& theme) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Data.theme == theme) return;
            m_Data.theme = theme;
        }
        Save();
    }

    // Favorites
    std::set<std::string> SettingsManager::GetFavorites() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Data.favorites;
    }

    void SettingsManager::SetFavorites(const std::set<std::string>& favorites) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Data.favorites = favorites;
        }
        Save();
    }

    void SettingsManager::AddFavorite(const std::string& fontPath) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Data.favorites.insert(fontPath);
        }
        Save();
    }

    void SettingsManager::RemoveFavorite(const std::string& fontPath) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Data.favorites.erase(fontPath);
        }
        Save();
    }

    bool SettingsManager::IsFavorite(const std::string& fontPath) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Data.favorites.count(fontPath) > 0;
    }

    // Recents
    std::vector<std::string> SettingsManager::GetRecents() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Data.recents;
    }

    void SettingsManager::SetRecents(const std::vector<std::string>& recents) {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Data.recents = recents;
        }
        Save();
    }

    void SettingsManager::AddRecent(const std::string& stylePath) {
        if (stylePath.empty()) return;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = std::remove(m_Data.recents.begin(), m_Data.recents.end(), stylePath);
            m_Data.recents.erase(it, m_Data.recents.end());
            m_Data.recents.insert(m_Data.recents.begin(), stylePath);
            if (m_Data.recents.size() > 10) m_Data.recents.resize(10);
        }
        Save();
    }
    
    void SettingsManager::RemoveRecent(const std::string& stylePath) {
         {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = std::remove(m_Data.recents.begin(), m_Data.recents.end(), stylePath);
            m_Data.recents.erase(it, m_Data.recents.end());
        }
        Save();
    }

}
