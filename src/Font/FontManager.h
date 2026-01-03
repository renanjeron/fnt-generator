#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

struct GlyphBitmap {
    std::vector<unsigned char> buffer;
    int width;
    int height;
    int bearingX;
    int bearingY;
    int advance;
};

class FontManager {
public:
    FontManager();
    ~FontManager();

    bool Initialize();
    void Shutdown();

    // Loads a font face from file path
    bool LoadFont(const std::string& path);

    // Sets the pixel size for the current font
    void SetSize(int size);

    // Returns the loaded font family name
    std::string GetFontName() const;

    // Renders a single character to a bitmap (8-bit grayscale)
    // loadFlags can be used to control hinting (e.g., FT_LOAD_NO_HINTING or FT_LOAD_TARGET_NORMAL)
    GlyphBitmap RenderGlyph(uint32_t charCode, FT_Int32 loadFlags = FT_LOAD_RENDER | FT_LOAD_NO_HINTING);
    
    // Renders the stroke/outline of a glyph
    GlyphBitmap RenderGlyphStroke(uint32_t charCode, float strokeWidth, FT_Int32 loadFlags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING);

    // Helper to get kerning between two characters
    int GetKerning(uint32_t left, uint32_t right);
    
    // Check if the font validates/contains a specific character code
    bool HasGlyph(uint32_t charCode) const;

    // Check if a font is currently loaded
    bool IsLoaded() const { return m_face != nullptr; }
    
    // Get font metrics
    int GetAscender() const { return m_face ? (m_face->size->metrics.ascender >> 6) : 0; }
    int GetDescender() const { return m_face ? (m_face->size->metrics.descender >> 6) : 0; }
    int GetLineHeight() const { return m_face ? (m_face->size->metrics.height >> 6) : 0; }

private:
    FT_Library m_library = nullptr;
    FT_Face m_face = nullptr;
    FT_Stroker m_stroker = nullptr;
    int m_currentSize = 24;
};
