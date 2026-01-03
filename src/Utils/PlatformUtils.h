#pragma once
#include <vector>
#include <string>

namespace Utils {

    struct FontInfo {
        std::string name;
        std::string path;
    };

    // enumerates installed system fonts
    std::vector<FontInfo> GetSystemFonts();

    // Opens a native folder picker dialog and returns the path, or empty string if cancelled
    std::string PickFolderDialog();

    // Opens a native file picker dialog (filter format: "Description\0*.ext\0")
    std::string PickFileDialog(const char* filter);

    // Opens a native save file dialog
    std::string SaveFileDialog(const char* filter, const char* defaultName);
    std::string GetConfigDir();
}
