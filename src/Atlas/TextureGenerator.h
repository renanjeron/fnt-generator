#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "../Font/FontManager.h"
#include "../Utils/BitmapUtils.h"

struct GlyphPlacement {
    uint32_t charCode;
    int x, y, width, height;
    int xoffset, yoffset;
    int advance;
    int pageIndex = 0; // Added for multi-page support
};

struct AtlasPage {
    int id;
    int width;
    int height;
    std::vector<unsigned char> pixels; // RGBA buffer
};

struct AtlasResult {
    std::vector<AtlasPage> pages; // Multiple pages support
    // int width;  // Deprecated/Moved to Page
    // int height; // Deprecated/Moved to Page
    
    // Logical Atlas size (for out-of-bounds coloring)
    int atlasWidth = 0;
    int atlasHeight = 0;

    std::vector<GlyphPlacement> glyphs;
    
    // Error tracking
    bool hasErrors = false;
    int skippedGlyphs = 0;
    std::string errorMessage;

    // Font Metrics (Metadata for Export)
    int fontSize = 0;
    int lineHeight = 0;
    int base = 0; // Ascender
    
    std::string fontName; // Added for export

    struct KerningPair {
        uint32_t first;
        uint32_t second;
        int amount;
    };
    std::vector<KerningPair> kernings;
};

// --- Structures ---
// GradientStop, GradientType, GradientData are now in BitmapUtils.h

struct AtlasSettings {
    int fontSize = 72;
    int padding = 5;
    int atlasWidth = 1024;
    int atlasHeight = 1024;

    // Hinting / Anti-aliasing
    // 0 = No Hinting (Smooth shapes), 1 = Light Hinting (Sharp), 2 = Normal (Crisp)
    int hintingMode = 0;

    // Fill
    uint8_t fillColor[4] = { 255, 255, 255, 255 }; // Solid color fallback
    GradientData fillGradient;

    // Stroke
    bool enableStroke = false;
    float strokeWidth = 0.0f;
    int strokePosition = 0; // 0=Outside, 1=Center, 2=Inside
    uint8_t strokeColor[4] = { 0, 0, 0, 255 }; // Solid stroke
    GradientData strokeGradient;

    // Shadow
    bool enableShadow = false;
    int shadowDistance = 0; // Legacy unused? kept just in case but we use offset
    int shadowAngle = 45;
    int shadowOffsetX = 5;
    int shadowOffsetY = 5;
    int shadowBlur = 0; // Gaussian blur radius
    uint8_t shadowColor[4] = { 0, 0, 0, 255 }; // R,G,B,A
    
    // Bevel (Advanced Lighting Effect)
    bool enableBevel = false;
    int bevelAngle = 135;        // Light angle in degrees
    float bevelDistance = 4.0f;  // Size of the bevel
    float bevelSpread = 4.0f;    // Softness/Blur of the bevel
    float bevelStrength = 1.0f;  // Shading intensity
    int bevelType = 0;           // 0 = Inner, 1 = Outer
    uint8_t bevelHighlightColor[4] = { 255, 255, 255, 255 };
    uint8_t bevelShadowColor[4] = { 0, 0, 0, 255 };
    
    // Inner Glow
    bool enableInnerGlow = false;
    float innerGlowSize = 5.0f;
    float innerGlowChoke = 0.0f; // 0-100, contracts the glow toward center
    uint8_t innerGlowColor[4] = { 255, 255, 255, 128 }; // R,G,B,A
    BlendMode innerGlowBlendMode = BlendMode::Normal;
    
    // Pattern Overlay
    PatternData pattern;

    // High Quality Export
    bool useSuperSampling = false;    // Renders at 2x and downsamples (Legacy)
    int superSamplingFactor = 1;      // 1=None, 2=SSAA 2x, 4=SSAA 4x
    
    // Char Adjustments
    int globalXAdvance = 0;
    int globalXOffset = 0;
    int globalYOffset = 0;
    
    // Multi-Page Control
    bool allowMultiPage = true;
    
    // Sorting
    bool keepInputOrder = false;

    // Kerning Control
    bool enableKerning = true;
};

class TextureGenerator {
public:
    // Generates a packed texture from the given text
    static AtlasResult GenerateAtlas(FontManager& fontManager, const std::string& text, const AtlasSettings& settings);
    
    // Generates a packed texture from a charset
    static AtlasResult GenerateAtlas(FontManager& fontManager, const std::vector<uint32_t>& charset, const AtlasSettings& settings);

    // Generates a proper text preview (linear)
    static AtlasResult GenerateTextPreview(FontManager& fontManager, const std::string& text, const AtlasSettings& settings);

private:
private:
    // Helper to blend: Removed/Deprecated. Use BitmapUtils::BlitGlyph directly.
};
