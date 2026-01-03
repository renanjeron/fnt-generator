#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "../Font/FontManager.h"

struct GlyphPlacement {
    uint32_t charCode;
    int x, y, width, height;
    int xoffset, yoffset;
    int advance;
};

struct AtlasResult {
    std::vector<unsigned char> pixels; // RGBA buffer
    int width;  // Buffer width
    int height; // Buffer height
    
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
};

struct AtlasSettings {
    int fontSize = 72;
    int padding = 5;
    int atlasWidth = 1024;
    int atlasHeight = 1024;

    // Hinting / Anti-aliasing
    // 0 = No Hinting (Smooth shapes), 1 = Light Hinting (Sharp), 2 = Normal (Crisp)
    int hintingMode = 0;

    // Fill
    bool enableGradient = false;
    uint8_t colorTop[3] = { 255, 255, 255 }; // R,G,B
    uint8_t colorBottom[3] = { 255, 255, 255 }; // R,G,B

    // Stroke
    bool enableStroke = false;
    bool enableStrokeGradient = false;
    float strokeWidth = 0.0f;
    uint8_t strokeColor[3] = { 0, 0, 0 };
    uint8_t strokeColorBottom[3] = { 0, 0, 0 };

    // Shadow
    bool enableShadow = false;
    int shadowDistance = 0;
    int shadowAngle = 45; // Degrees? Or just Offset X/Y? Web app has "Angle" + "Distance".
    // Let's use Offset X/Y for simplicity in impl or calculate.
    // Web app: Angle + Distance.
    // We can convert Angle/Dist to OffsetX/OffsetY in Generator.
    // For now let's store OffsetX/OffsetY directly or Angle/Dist. Let's stick to web app params.
    // Actually, let's keep it simple: OffsetX, OffsetY.
    int shadowOffsetX = 5;
    int shadowOffsetY = 5;
    int shadowBlur = 0; // Gaussian blur radius
    uint8_t shadowColor[4] = { 0, 0, 0, 255 }; // R,G,B,A
    
    // Bevel (3D highlight effect)
    bool enableBevel = false;
    int bevelAngle = 135; // Light angle in degrees
    float bevelDistance = 2.0f;
    int bevelBlur = 0;
    uint8_t bevelColor[3] = { 255, 255, 255 }; // Highlight color
    
    // Inner Glow
    bool enableInnerGlow = false;
    float innerGlowSize = 5.0f;
    float innerGlowChoke = 0.0f; // 0-100, contracts the glow toward center
    uint8_t innerGlowColor[4] = { 255, 255, 255, 128 }; // R,G,B,A

    // High Quality Export
    bool useSuperSampling = false; // Renders at 2x and downsamples
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
    // Helper to blend a glyph into the atlas buffer
    static void BlendGlyph(std::vector<unsigned char>& atlasPixels, int atlasWidth, int x, int y, const GlyphBitmap& glyph, uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool useGradient, const AtlasSettings* settings);
};
