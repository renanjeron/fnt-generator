#pragma once
#include <string>
// #include <GL/gl.h> -- Removed, handled by GLFW
#ifdef _WIN32
#include <windows.h>
#endif
#include <GLFW/glfw3.h> // Ensure GLuint is available 

namespace Utils {
    // Generates a small preview texture for the font at the given path.
    // Returns the OpenGL texture ID.
    GLuint GenerateFontPreview(const std::string& fontPath);

    // Cleans up all generated preview textures and frees FreeType resources.
    void CleanupFontPreviews();
}
