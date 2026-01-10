#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include <mutex>


struct GlyphBitmap {
    std::vector<unsigned char> buffer;
    int width;
    int height;
    int bearingX;
    int bearingY;
    int advance;
};

struct FontMetadataEntry {
    std::string nameID;   // e.g. "Copyright", "Family"
    std::string language; // e.g. "en-US"
    std::string value;    // The actual string
};

using FontMetadata = std::vector<FontMetadataEntry>;

class FontManager {
public:
    FontManager();
    ~FontManager();

    bool Initialize();
    void Shutdown();

    // Loads a primary font face from file path
    bool LoadFont(const std::string& path);

    // Loads a fallback font face
    bool LoadFallbackFont(const std::string& path);
    void ClearFallbackFont();

    // Sets the pixel size for the current font
    void SetSize(int size);

    // Returns the loaded font family name
    std::string GetFontName() const;

    // Renders a single character to a bitmap (8-bit grayscale)
    // loadFlags can be used to control hinting (e.g., FT_LOAD_NO_HINTING or FT_LOAD_TARGET_NORMAL)
    GlyphBitmap RenderGlyph(uint32_t charCode, FT_Int32 loadFlags = FT_LOAD_RENDER | FT_LOAD_NO_HINTING);
    
    // Renders the stroke/outline of a glyph
    GlyphBitmap RenderGlyphStroke(uint32_t charCode, float strokeWidth, FT_Stroker_LineJoin joinStyle = FT_STROKER_LINEJOIN_ROUND, float miterLimit = 1.0f, FT_Int32 loadFlags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING);

    // Helper to get kerning between two characters
    int GetKerning(uint32_t left, uint32_t right);

    // Get glyph metrics/bounds without rendering bitmap (buffer will be empty)
    bool GetGlyphBounds(uint32_t charCode, GlyphBitmap& outMetrics, FT_Int32 loadFlags = FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
    
    // Get stroke metrics/bounds without rendering bitmap
    // Get stroke metrics/bounds without rendering bitmap
    bool GetGlyphStrokeBounds(uint32_t charCode, float strokeWidth, FT_Stroker_LineJoin joinStyle, float miterLimit, GlyphBitmap& outMetrics, FT_Int32 loadFlags = FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
    
    // Check if the font validates/contains a specific character code
    bool HasGlyph(uint32_t charCode) const;

    // Check if a font is currently loaded
    bool IsLoaded() const { return m_face != nullptr; }
    bool IsFallbackLoaded() const { return m_fallbackFace != nullptr; }

    // Check if the font has kerning information (legacy kern table)
    bool HasKerning() const;

    std::string GetFilePath() const { return m_currentPath; }
    std::string GetFallbackFilePath() const { return m_fallbackPath; }
    
    // Check if the font has all glyphs in the list
    bool HasGlyphs(const std::vector<uint32_t>& charCodes) const;
    
    // Check if the font has ANY glyph in the range [start, end]
    bool HasAnyGlyph(uint32_t start, uint32_t end) const;
    
    // Get font metrics
    int GetAscender() const;
    int GetDescender() const;
    int GetLineHeight() const;

    // Get all available metadata (SFNT names)
    FontMetadata GetMetadata() const;

private:
    FT_Library m_library = nullptr;
    FT_Face m_face = nullptr;
    FT_Face m_fallbackFace = nullptr;
    FT_Stroker m_stroker = nullptr;
    int m_currentSize = 24;
    std::string m_currentPath = "";
    std::string m_fallbackPath = "";
    mutable std::recursive_mutex m_mutex;
};
