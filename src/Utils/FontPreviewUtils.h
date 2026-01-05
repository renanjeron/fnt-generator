#pragma once
#include <string>

namespace Utils {
    // Generates a small preview texture for the font at the given path.
    // Returns the OpenGL texture ID.
    unsigned int GenerateFontPreview(const std::string& fontPath);

    // Cleans up all generated preview textures and frees FreeType resources.
    void CleanupFontPreviews();
}
