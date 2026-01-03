#include "PlatformUtils.h"
#if !defined(_WIN32) && !defined(__APPLE__)
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <pwd.h>
#include <unistd.h>

namespace Utils {

    std::vector<FontInfo> GetSystemFonts() {
        std::vector<FontInfo> fonts;
        std::vector<std::string> searchPaths = {
            "/usr/share/fonts",
            "/usr/local/share/fonts"
        };

        const char* home = getenv("HOME");
        if (home) {
            searchPaths.push_back(std::string(home) + "/.local/share/fonts");
            searchPaths.push_back(std::string(home) + "/.fonts");
        }

        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                try {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".ttf" || ext == ".otf") {
                            fonts.push_back({ entry.path().stem().string(), entry.path().string() });
                        }
                    }
                }
                } catch(...) {}
            }
        }
        
        std::sort(fonts.begin(), fonts.end(), [](const FontInfo& a, const FontInfo& b) {
            return a.name < b.name;
        });
        
        return fonts;
    }

    std::string RunZenity(const std::string& args) {
        std::string command = "zenity " + args;
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";
        char buffer[1024];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        // Remove trailing newline
        if (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }

    std::string PickFolderDialog() {
        return RunZenity("--file-selection --directory --title=\"Select Folder\"");
    }

    std::string PickFileDialog(const char* filter) {
        return RunZenity("--file-selection --title=\"Select File\"");
    }

    std::string SaveFileDialog(const char* filter, const char* defaultName) {
        std::string args = "--file-selection --save --confirm-overwrite --title=\"Save Style\"";
        if (defaultName) args += " --filename=\"" + std::string(defaultName) + "\"";
        return RunZenity(args);
    }

    std::string GetConfigDir() {
        const char* configHome = getenv("XDG_CONFIG_HOME");
        std::string path;
        if (configHome && configHome[0] != '\0') {
            path = std::string(configHome) + "/FntGenerator";
        } else {
            const char* home = getenv("HOME");
            if (home) {
                path = std::string(home) + "/.config/FntGenerator";
            } else {
                path = "./.config/FntGenerator";
            }
        }
        std::filesystem::create_directories(path);
        return path;
    }
}
#endif
