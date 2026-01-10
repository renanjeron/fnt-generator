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
    if (m_fallbackFace) {
        FT_Done_Face(m_fallbackFace);
        m_fallbackFace = nullptr;
    }
    if (m_library) {
        FT_Done_FreeType(m_library);
        m_library = nullptr;
    }
}

bool FontManager::LoadFont(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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

bool FontManager::LoadFallbackFont(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_fallbackFace) {
        FT_Done_Face(m_fallbackFace);
        m_fallbackFace = nullptr;
    }

    FT_Face tempFace = nullptr;
    if (FT_New_Face(m_library, path.c_str(), 0, &tempFace)) {
        std::cerr << "ERROR::FREETYPE: Failed to load fallback font: " << path << std::endl;
        return false;
    }

    m_fallbackFace = tempFace;
    m_fallbackPath = path;
    FT_Set_Pixel_Sizes(m_fallbackFace, 0, (FT_UInt)m_currentSize);
    return true;
}

void FontManager::ClearFallbackFont() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_fallbackFace) {
        FT_Done_Face(m_fallbackFace);
        m_fallbackFace = nullptr;
    }
    m_fallbackPath = "";
}

void FontManager::SetSize(int size) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_currentSize = size;
    if (m_face) {
        FT_Set_Pixel_Sizes(m_face, 0, (FT_UInt)size);
    }
    if (m_fallbackFace) {
        FT_Set_Pixel_Sizes(m_fallbackFace, 0, (FT_UInt)size);
    }
}

std::string FontManager::GetFontName() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_face) {
        return std::string(m_face->family_name ? m_face->family_name : "Unknown") + " " + 
               std::string(m_face->style_name ? m_face->style_name : "Unknown");
    }
    return "None";
}

bool FontManager::HasGlyphs(const std::vector<uint32_t>& charCodes) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_face) return false;
    
    for (uint32_t code : charCodes) {
        if (FT_Get_Char_Index(m_face, code) == 0) {
            if (!m_fallbackFace || FT_Get_Char_Index(m_fallbackFace, code) == 0) {
                return false;
            }
        }
    }
    return true;
}

bool FontManager::HasAnyGlyph(uint32_t start, uint32_t end) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_face) return false;

    // Fast check: iterate range and find at least one
    for (uint32_t code = start; code <= end; code++) {
        if (FT_Get_Char_Index(m_face, code) != 0) return true;
        if (m_fallbackFace && FT_Get_Char_Index(m_fallbackFace, code) != 0) return true;
    }
    return false;
}

GlyphBitmap FontManager::RenderGlyph(uint32_t charCode, FT_Int32 loadFlags) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GlyphBitmap result = { {}, 0, 0, 0, 0, 0 };

    if (!m_face) return result;

    FT_Face faceToUse = m_face;
    if (FT_Get_Char_Index(m_face, charCode) == 0 && m_fallbackFace && FT_Get_Char_Index(m_fallbackFace, charCode) != 0) {
        faceToUse = m_fallbackFace;
    }

    // Use passed flags
    if (FT_Load_Char(faceToUse, charCode, loadFlags)) {
        return result;
    }

    FT_GlyphSlot slot = faceToUse->glyph;

    result.width = slot->bitmap.width;
    result.height = slot->bitmap.rows;
    result.bearingX = slot->bitmap_left;
    result.bearingY = slot->bitmap_top;
    result.advance = slot->advance.x >> 6;

    // Copy bitmap data
    int numPixels = result.width * result.height;
    if (numPixels > 0 && slot->bitmap.buffer) {
        result.buffer.resize(numPixels);
        if (slot->bitmap.pitch == result.width) {
            memcpy(result.buffer.data(), slot->bitmap.buffer, numPixels);
        } else {
            for (int i = 0; i < result.height; i++) {
                memcpy(result.buffer.data() + i * result.width, 
                       slot->bitmap.buffer + i * slot->bitmap.pitch, 
                       result.width);
            }
        }
    }

    return result;
}

GlyphBitmap FontManager::RenderGlyphStroke(uint32_t charCode, float strokeWidth, FT_Stroker_LineJoin joinStyle, float miterLimit, FT_Int32 loadFlags) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GlyphBitmap result = { {}, 0, 0, 0, 0, 0 };

    if (!m_face || !m_stroker) return result;

    FT_Face faceToUse = m_face;
    FT_UInt glyphIndex = FT_Get_Char_Index(m_face, charCode);
    if (glyphIndex == 0 && m_fallbackFace) {
        glyphIndex = FT_Get_Char_Index(m_fallbackFace, charCode);
        if (glyphIndex != 0) faceToUse = m_fallbackFace;
    }
    
    if (glyphIndex == 0) return result;

    FT_Int32 flags = loadFlags & ~FT_LOAD_RENDER;
    flags |= FT_LOAD_NO_BITMAP;

    if (FT_Load_Glyph(faceToUse, glyphIndex, flags)) return result;

    FT_Glyph glyph;
    if (FT_Get_Glyph(faceToUse->glyph, &glyph)) return result;

    FT_Fixed radius = (FT_Fixed)(strokeWidth * 64.0f);
    FT_Fixed mLimit = (FT_Fixed)(miterLimit * 65536.0f);
    
    FT_Stroker_Set(m_stroker, radius, FT_STROKER_LINECAP_ROUND, joinStyle, mLimit);
    
    if (FT_Glyph_Stroke(&glyph, m_stroker, 1)) {
        FT_Done_Glyph(glyph);
        return result; 
    }
    
    if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, 1)) {
        FT_Done_Glyph(glyph);
        return result;
    }
    
    FT_BitmapGlyph bg = (FT_BitmapGlyph)glyph;
    
    result.width = bg->bitmap.width;
    result.height = bg->bitmap.rows;
    result.bearingX = bg->left;
    result.bearingY = bg->top;
    result.advance = (faceToUse->glyph->advance.x >> 6);

    int numPixels = result.width * result.height;
    if (numPixels > 0 && bg->bitmap.buffer) {
        result.buffer.resize(numPixels);
        for (int i = 0; i < result.height; i++) {
             memcpy(result.buffer.data() + (i * result.width), 
                    bg->bitmap.buffer + (i * bg->bitmap.pitch), 
                    result.width);
        }
    }
    
    FT_Done_Glyph(glyph);
    return result;
}

bool FontManager::GetGlyphBounds(uint32_t charCode, GlyphBitmap& outMetrics, FT_Int32 loadFlags) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_face) return false;
    
    FT_Face faceToUse = m_face;
    if (FT_Get_Char_Index(m_face, charCode) == 0 && m_fallbackFace && FT_Get_Char_Index(m_fallbackFace, charCode) != 0) {
        faceToUse = m_fallbackFace;
    }

    FT_Int32 flags = loadFlags | FT_LOAD_NO_BITMAP;
    if (FT_Load_Char(faceToUse, charCode, flags)) return false;
    
    FT_Glyph glyph;
    if (FT_Get_Glyph(faceToUse->glyph, &glyph)) return false;

    FT_BBox bbox;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bbox);
    
    outMetrics.width = bbox.xMax - bbox.xMin;
    outMetrics.height = bbox.yMax - bbox.yMin;
    outMetrics.bearingX = bbox.xMin;
    outMetrics.bearingY = bbox.yMax;
    outMetrics.advance = faceToUse->glyph->advance.x >> 6;
    outMetrics.buffer.clear();
    
    FT_Done_Glyph(glyph);
    return true;
}

bool FontManager::GetGlyphStrokeBounds(uint32_t charCode, float strokeWidth, FT_Stroker_LineJoin joinStyle, float miterLimit, GlyphBitmap& outMetrics, FT_Int32 loadFlags) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_face || !m_stroker) return false;
    
    FT_Face faceToUse = m_face;
    FT_UInt glyphIndex = FT_Get_Char_Index(m_face, charCode);
    if (glyphIndex == 0 && m_fallbackFace) {
        glyphIndex = FT_Get_Char_Index(m_fallbackFace, charCode);
        if (glyphIndex != 0) faceToUse = m_fallbackFace;
    }

    if (glyphIndex == 0) return false;

    FT_Int32 flags = loadFlags & ~FT_LOAD_RENDER;
    flags |= FT_LOAD_NO_BITMAP;

    if (FT_Load_Glyph(faceToUse, glyphIndex, flags)) return false;

    FT_Glyph glyph;
    if (FT_Get_Glyph(faceToUse->glyph, &glyph)) return false;

    FT_Fixed radius = (FT_Fixed)(strokeWidth * 64.0f);
    FT_Fixed mLimit = (FT_Fixed)(miterLimit * 65536.0f);
    FT_Stroker_Set(m_stroker, radius, FT_STROKER_LINECAP_ROUND, joinStyle, mLimit);

    if (FT_Glyph_Stroke(&glyph, m_stroker, 1)) {
        FT_Done_Glyph(glyph);
        return false;
    }
    
    FT_BBox bbox;
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bbox);
    
    outMetrics.width = bbox.xMax - bbox.xMin;
    outMetrics.height = bbox.yMax - bbox.yMin;
    outMetrics.bearingX = bbox.xMin;
    outMetrics.bearingY = bbox.yMax;
    outMetrics.advance = faceToUse->glyph->advance.x >> 6;
    outMetrics.buffer.clear();

    FT_Done_Glyph(glyph);
    return true;
}

bool FontManager::HasGlyph(uint32_t charCode) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_face) return false;
    if (FT_Get_Char_Index(m_face, charCode) != 0) return true;
    return m_fallbackFace && FT_Get_Char_Index(m_fallbackFace, charCode) != 0;
}

bool FontManager::HasKerning() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_face && FT_HAS_KERNING(m_face);
}

int FontManager::GetAscender() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_face ? (m_face->size->metrics.ascender >> 6) : 0;
}

int FontManager::GetDescender() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_face ? (m_face->size->metrics.descender >> 6) : 0;
}

int FontManager::GetLineHeight() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_face ? (m_face->size->metrics.height >> 6) : 0;
}

int FontManager::GetKerning(uint32_t left, uint32_t right) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
            case 0x01: return "ar"; // Arabic
            case 0x02: return "bg"; // Bulgarian
            case 0x03: return "ca"; // Catalan
            case 0x04: return "zh"; // Chinese
            case 0x05: return "cs"; // Czech
            case 0x06: return "da"; // Danish
            case 0x07: return "de"; // German
            case 0x08: return "el"; // Greek
            case 0x09: return "en"; // English
            case 0x0A: return "es"; // Spanish
            case 0x0B: return "fi"; // Finnish
            case 0x0C: return "fr"; // French
            case 0x0D: return "he"; // Hebrew
            case 0x0E: return "hu"; // Hungarian
            case 0x0F: return "is"; // Icelandic
            case 0x10: return "it"; // Italian
            case 0x11: return "ja"; // Japanese
            case 0x12: return "ko"; // Korean
            case 0x13: return "nl"; // Dutch
            case 0x14: return "no"; // Norwegian
            case 0x15: return "pl"; // Polish
            case 0x16: return "pt"; // Portuguese
            case 0x17: return "rm"; // Romansh
            case 0x18: return "ro"; // Romanian
            case 0x19: return "ru"; // Russian
            case 0x1A: return "hr"; // Croatian
            case 0x1B: return "sk"; // Slovak
            case 0x1C: return "sq"; // Albanian
            case 0x1D: return "sv"; // Swedish
            case 0x1E: return "th"; // Thai
            case 0x1F: return "tr"; // Turkish
            case 0x20: return "ur"; // Urdu
            case 0x21: return "id"; // Indonesian
            case 0x22: return "uk"; // Ukrainian
            case 0x23: return "be"; // Belarusian
            case 0x24: return "sl"; // Slovenian
            case 0x25: return "et"; // Estonian
            case 0x26: return "lv"; // Latvian
            case 0x27: return "lt"; // Lithuanian
            case 0x29: return "fa"; // Farsi
            case 0x2A: return "vi"; // Vietnamese
            case 0x2B: return "hy"; // Armenian
            case 0x2C: return "az"; // Azeri
            case 0x2D: return "eu"; // Basque
            case 0x2F: return "mk"; // Macedonian
            case 0x3E: return "ms"; // Malay
            case 0x41: return "sw"; // Swahili
            case 0x49: return "ta"; // Tamil
            case 0x56: return "gl"; // Galician
            default: break;
        }
    } else if (platform_id == 1) { // Mac
        // https://developer.apple.com/library/archive/documentation/KeyboardLayouts/Conceptual/LinguisticsKBLayout/Articles/LanguageIdentifiers.html
        switch (language_id) {
            case 0: return "en";
            case 1: return "fr";
            case 2: return "de";
            case 3: return "it";
            case 4: return "nl";
            case 5: return "sv";
            case 6: return "es";
            case 7: return "da";
            case 8: return "pt";
            case 9: return "no";
            case 10: return "he";
            case 11: return "ja";
            case 12: return "ar";
            case 13: return "fi";
            case 14: return "el";
            case 15: return "is";
            case 16: return "mt";
            case 17: return "tr";
            case 18: return "hr";
            case 19: return "zh-tw";
            case 20: return "ur";
            case 21: return "hi";
            case 22: return "th";
            case 23: return "ko";
            case 24: return "lt";
            case 25: return "pl";
            case 26: return "hu";
            case 27: return "et";
            case 28: return "lv";
            case 29: return "se";
            case 30: return "fo";
            case 31: return "fa";
            case 32: return "ru";
            case 33: return "zh-cn";
            default: break;
        }
    }
    std::stringstream ss;
    ss << platform_id << ":" << language_id;
    return ss.str();
}

FontMetadata FontManager::GetMetadata() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    FontMetadata meta;
    if (!m_face) return meta;

    FT_UInt count = FT_Get_Sfnt_Name_Count(m_face);
    for (FT_UInt i = 0; i < count; ++i) {
        FT_SfntName entry;
        if (FT_Get_Sfnt_Name(m_face, i, &entry) != 0) continue;

        std::string value;
        bool keep = false;

        if (entry.platform_id == 3 && (entry.encoding_id == 1 || entry.encoding_id == 10)) {
            value = Utf16BEToUtf8(entry.string, entry.string_len);
            keep = true;
        } else if (entry.platform_id == 1 && entry.encoding_id == 0) {
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

