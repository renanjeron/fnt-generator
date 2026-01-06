#include "FontManager.h"
#include <iostream>
#include <cstdint>
#include FT_SFNT_NAMES_H
#include <sstream>

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

    m_currentPath = path;

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

bool FontManager::HasGlyphs(const std::vector<uint32_t>& charCodes) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_face) return false;
    
    for (uint32_t code : charCodes) {
        if (FT_Get_Char_Index(m_face, code) == 0) {
            return false;
        }
    }
    return true;
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

GlyphBitmap FontManager::RenderGlyphStroke(uint32_t charCode, float strokeWidth, FT_Stroker_LineJoin joinStyle, float miterLimit, FT_Int32 loadFlags) {
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
    FT_Fixed mLimit = (FT_Fixed)(miterLimit * 65536.0f);
    
    FT_Stroker_Set(m_stroker, radius, FT_STROKER_LINECAP_ROUND, joinStyle, mLimit);
    
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

bool FontManager::GetGlyphBounds(uint32_t charCode, GlyphBitmap& outMetrics, FT_Int32 loadFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_face) return false;
    
    // Use NO_BITMAP to avoid rendering, but load the glyph slot
    FT_Int32 flags = loadFlags | FT_LOAD_NO_BITMAP;
    if (FT_Load_Char(m_face, charCode, flags)) return false;
    
    // Use FT_Get_Glyph to get a standalone glyph object
    FT_Glyph glyph;
    if (FT_Get_Glyph(m_face->glyph, &glyph)) return false;

    // Get the exact bounding box in pixels (floors min, ceils max)
    FT_BBox bbox;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bbox);
    
    outMetrics.width = bbox.xMax - bbox.xMin;
    outMetrics.height = bbox.yMax - bbox.yMin;
    outMetrics.bearingX = bbox.xMin;
    outMetrics.bearingY = bbox.yMax;
    outMetrics.advance = m_face->glyph->advance.x >> 6;
    outMetrics.buffer.clear(); // No data
    
    FT_Done_Glyph(glyph);
    return true;
}

bool FontManager::GetGlyphStrokeBounds(uint32_t charCode, float strokeWidth, FT_Stroker_LineJoin joinStyle, float miterLimit, GlyphBitmap& outMetrics, FT_Int32 loadFlags) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_face || !m_stroker) return false;
    
    FT_UInt glyphIndex = FT_Get_Char_Index(m_face, charCode);
    if (glyphIndex == 0) return false;

    FT_Int32 flags = loadFlags & ~FT_LOAD_RENDER;
    flags |= FT_LOAD_NO_BITMAP;

    if (FT_Load_Glyph(m_face, glyphIndex, flags)) return false;

    FT_Glyph glyph;
    if (FT_Get_Glyph(m_face->glyph, &glyph)) return false;

    FT_Fixed radius = (FT_Fixed)(strokeWidth * 64.0f);
    FT_Fixed mLimit = (FT_Fixed)(miterLimit * 65536.0f);
    FT_Stroker_Set(m_stroker, radius, FT_STROKER_LINECAP_ROUND, joinStyle, mLimit);

    // Stroke the glyph but no bitmap conversion yet
    if (FT_Glyph_Stroke(&glyph, m_stroker, 1)) {
        FT_Done_Glyph(glyph);
        return false;
    }
    
    // Get Control Box (Bounding Box)
    FT_BBox bbox;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bbox);
    
    outMetrics.width = bbox.xMax - bbox.xMin;
    outMetrics.height = bbox.yMax - bbox.yMin;
    outMetrics.bearingX = bbox.xMin;
    outMetrics.bearingY = bbox.yMax;
    outMetrics.advance = m_face->glyph->advance.x >> 6; // Use original advance? Yes mostly.
    outMetrics.buffer.clear();

    FT_Done_Glyph(glyph);
    return true;
}

bool FontManager::HasGlyph(uint32_t charCode) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_face) return false;
    return FT_Get_Char_Index(m_face, charCode) != 0;
}

bool FontManager::HasKerning() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_face && FT_HAS_KERNING(m_face);
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

static std::string Utf16BEToUtf8(const uint8_t* data, size_t len) {
    std::string res;
    // Iterate over 16-bit units
    for (size_t i = 0; i + 1 < len; i += 2) {
        // Read Big Endian
        uint16_t c = (data[i] << 8) | data[i+1];
        
        // Simple UTF-16 to UTF-8 (handling only BMP for now)
        if (c < 0x80) {
            res += (char)c;
        } else if (c < 0x800) {
            res += (char)(0xC0 | (c >> 6));
            res += (char)(0x80 | (c & 0x3F));
        } else {
            res += (char)(0xE0 | (c >> 12));
            res += (char)(0x80 | ((c >> 6) & 0x3F));
            res += (char)(0x80 | (c & 0x3F));
        }
    }
    return res;
}

static std::string GetNameIDString(FT_UShort name_id) {
    switch (name_id) {
        case 0: return "Copyright";
        case 1: return "Font Family";
        case 2: return "Font Subfamily";
        case 3: return "Unique ID";
        case 4: return "Full Name";
        case 5: return "Version";
        case 6: return "PostScript Name";
        case 7: return "Trademark";
        case 8: return "Manufacturer";
        case 9: return "Designer";
        case 10: return "Description";
        case 11: return "Manufacturer URL";
        case 12: return "Designer URL";
        case 13: return "License";
        case 14: return "License URL";
        case 16: return "Typographic Family";
        case 17: return "Typographic Subfamily";
        case 21: return "WWS Family";
        case 22: return "WWS Subfamily";
        default: {
            std::stringstream ss;
            ss << "ID " << name_id;
            return ss.str();
        }
    }
}

static std::string GetLanguageString(FT_UShort platform_id, FT_UShort language_id) {
    if (platform_id == 3) { // Windows
        // Mask 0xFF to get primary language
        uint8_t primary = language_id & 0xFF;
        switch (primary) {
            case 0x09: return "en";
            case 0x0A: return "es";
            case 0x0C: return "fr";
            case 0x07: return "de";
            case 0x10: return "it";
            case 0x16: return "pt";
            case 0x19: return "ru";
            case 0x11: return "ja";
            case 0x12: return "ko";
            case 0x04: return "zh";
            default: break;
        }
    } else if (platform_id == 1) { // Mac
        if (language_id == 0) return "en";
    }
    std::stringstream ss;
    ss << platform_id << ":" << language_id;
    return ss.str();
}

FontMetadata FontManager::GetMetadata() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    FontMetadata meta;
    if (!m_face) return meta;

    FT_UInt count = FT_Get_Sfnt_Name_Count(m_face);
    for (FT_UInt i = 0; i < count; ++i) {
        FT_SfntName entry;
        if (FT_Get_Sfnt_Name(m_face, i, &entry) != 0) continue;

        // Only interested in Platform 1 (Mac) with Encoding 0 (Roman) 
        // OR Platform 3 (Windows) with Encoding 1 (Unicode) or 10 (UCS-4)
        
        std::string value;
        bool keep = false;

        if (entry.platform_id == 3 && (entry.encoding_id == 1 || entry.encoding_id == 10)) {
            // Windows Unicode (UTF-16BE)
            value = Utf16BEToUtf8(entry.string, entry.string_len);
            keep = true;
        } else if (entry.platform_id == 1 && entry.encoding_id == 0) {
            // Mac Roman - treating as ASCII/Latin-1 for simplicity or raw
            // Most basic Mac Roman matches ASCII for < 127
            value.assign((const char*)entry.string, entry.string_len);
            keep = true;
        }

        if (keep && !value.empty()) {
            FontMetadataEntry item;
            item.nameID = GetNameIDString(entry.name_id);
            item.language = GetLanguageString(entry.platform_id, entry.language_id);
            item.value = value;
            meta.push_back(item);
        }
    }
    return meta;
}

