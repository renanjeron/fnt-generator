#include "FontManager.h"
#include <iostream>
#include <cstdint>

FontManager::FontManager() {
}

FontManager::~FontManager() {
    Shutdown();
}

bool FontManager::Initialize() {
    if (FT_Init_FreeType(&m_library)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return false;
    }
    // Initialize Stroker
    if (FT_Stroker_New(m_library, &m_stroker)) {
        std::cerr << "ERROR::FREETYPE: Could not init Stroker" << std::endl;
        FT_Done_FreeType(m_library);
        m_library = nullptr;
        return false;
    }
    return true;
}

void FontManager::Shutdown() {
    if (m_stroker) {
        FT_Stroker_Done(m_stroker);
        m_stroker = nullptr;
    }
    if (m_face) {
        FT_Done_Face(m_face);
        m_face = nullptr;
    }
    if (m_library) {
        FT_Done_FreeType(m_library);
        m_library = nullptr;
    }
}

bool FontManager::LoadFont(const std::string& path) {
    // Clean up previous face if exists
    if (m_face) {
        FT_Done_Face(m_face);
        m_face = nullptr;
    }

    if (FT_New_Face(m_library, path.c_str(), 0, &m_face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font: " << path << std::endl;
        return false;
    }

    // Set default size
    SetSize(m_currentSize);
    return true;
}

void FontManager::SetSize(int size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentSize = size;
    if (m_face) {
        FT_Set_Pixel_Sizes(m_face, 0, size);
    }
}

std::string FontManager::GetFontName() const {
    if (m_face) {
        return std::string(m_face->family_name) + " " + std::string(m_face->style_name);
    }
    return "None";
}

GlyphBitmap FontManager::RenderGlyph(uint32_t charCode, FT_Int32 loadFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    GlyphBitmap result = { {}, 0, 0, 0, 0, 0 };

    if (!m_face) return result;

    // Use passed flags
    if (FT_Load_Char(m_face, charCode, loadFlags)) {
        std::cerr << "ERROR::FREETYTPE: Failed to load Glyph " << charCode << std::endl;
        return result;
    }

    FT_GlyphSlot slot = m_face->glyph;

    result.width = slot->bitmap.width;
    result.height = slot->bitmap.rows;
    result.bearingX = slot->bitmap_left;
    result.bearingY = slot->bitmap_top;
    result.advance = slot->advance.x >> 6; // Bitshift by 6 to get value in pixels (2^6 = 64)

    // Copy bitmap data
    int numPixels = result.width * result.height;
    if (numPixels > 0) {
        result.buffer.resize(numPixels);
        memcpy(result.buffer.data(), slot->bitmap.buffer, numPixels);
    }

    return result;
}

GlyphBitmap FontManager::RenderGlyphStroke(uint32_t charCode, float strokeWidth, FT_Int32 loadFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    GlyphBitmap result = { {}, 0, 0, 0, 0, 0 };

    if (!m_face || !m_stroker) return result;

    FT_UInt glyphIndex = FT_Get_Char_Index(m_face, charCode);
    if (glyphIndex == 0) return result;

    // Remove RENDER flag if present, we need outline
    FT_Int32 flags = loadFlags & ~FT_LOAD_RENDER;
    flags |= FT_LOAD_NO_BITMAP;

    if (FT_Load_Glyph(m_face, glyphIndex, flags)) return result;

    FT_Glyph glyph;
    if (FT_Get_Glyph(m_face->glyph, &glyph)) return result;

    // Convert float pixels to 26.6 fixed point (radius)
    // For outside border, radius is thickness.
    FT_Fixed radius = (FT_Fixed)(strokeWidth * 64.0f);
    
    FT_Stroker_Set(m_stroker, radius, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    
    // Stroke (using the more robust FT_Glyph_Stroke)
    if (FT_Glyph_Stroke(&glyph, m_stroker, 1)) {
        FT_Done_Glyph(glyph);
        return result; 
    }
    
    // Render to bitmap
    if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, 1)) {
        FT_Done_Glyph(glyph);
        return result;
    }
    
    FT_BitmapGlyph bg = (FT_BitmapGlyph)glyph;
    
    result.width = bg->bitmap.width;
    result.height = bg->bitmap.rows;
    result.bearingX = bg->left;
    result.bearingY = bg->top;
    result.advance = (m_face->glyph->advance.x >> 6); // Original advance

    // Copy buffer dealing with pitch
    int numPixels = result.width * result.height;
    if (numPixels > 0) {
        result.buffer.resize(numPixels);
        for (int i = 0; i < result.height; i++) {
             const unsigned char* srcRow = bg->bitmap.buffer + (i * bg->bitmap.pitch);
             unsigned char* dstRow = result.buffer.data() + (i * result.width);
             
             // Copy row (width bytes for 8-bit/pixel)
             // Safety check on width?
             if (result.width > 0) memcpy(dstRow, srcRow, result.width);
        }
    }
    
    FT_Done_Glyph(glyph);
    return result;
}

bool FontManager::HasGlyph(uint32_t charCode) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_face) return false;
    return FT_Get_Char_Index(m_face, charCode) != 0;
}

int FontManager::GetAscender() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_face ? (m_face->size->metrics.ascender >> 6) : 0;
}

int FontManager::GetDescender() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_face ? (m_face->size->metrics.descender >> 6) : 0;
}

int FontManager::GetLineHeight() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_face ? (m_face->size->metrics.height >> 6) : 0;
}

int FontManager::GetKerning(uint32_t left, uint32_t right) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_face || !FT_HAS_KERNING(m_face)) return 0;
    FT_Vector kerning;
    FT_Get_Kerning(m_face, FT_Get_Char_Index(m_face, left), FT_Get_Char_Index(m_face, right), FT_KERNING_DEFAULT, &kerning);
    return kerning.x >> 6;
}
