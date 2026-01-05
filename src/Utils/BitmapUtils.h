#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include "../Font/FontManager.h"

struct GradientStop {
    float position;
    float color[4];
};

enum class GradientType {
    Linear = 0,
    Radial = 1
};

struct GradientData {
    bool enabled = false;
    GradientType type = GradientType::Linear;
    float angle = 0.0f;
    std::vector<GradientStop> stops;
};

enum class BlendMode {
    Normal = 0,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Subtract,
    Divide
};

enum class PatternMapping {
    Glyph = 0,
    Global
};

struct PatternData {
    bool enabled = false;
    std::string imagePath;
    float opacity = 1.0f;
    float angle = 0.0f;
    float scale = 1.0f;
    BlendMode blendMode = BlendMode::Normal;
    PatternMapping mappingMode = PatternMapping::Glyph;
};

namespace BitmapUtils {

    // Applies a separable Gaussian blur to a single-channel (grayscale) image.
    // Returns a new buffer of the same size.
    std::vector<uint8_t> ApplyGaussianBlur(const std::vector<uint8_t>& src, int w, int h, float radius);

    // Get color from gradient at position t (0..1)
    void SampleGradient(const std::vector<GradientStop>& stops, float t, uint8_t* outColor);

    // Blends a glyph bitmap onto a 4-channel RGBA destination buffer.
    // Supports solid color or gradient (Linear/Radial) via GradientData.
    // colorTop: Solid color (fallback)
    // gradient: Optional gradient data
    void BlitGlyph(std::vector<uint8_t>& dest, int destW, int destH, 
                   int x, int y, 
                   const GlyphBitmap& glyph, 
                   const uint8_t colorTop[4], 
                   const GradientData* gradient = nullptr,
                   bool maskToDest = false);

    // Renders an inner glow effect for a glyph onto the destination buffer.
    // The glow is calculated based on the glyph's alpha shape.
    void DrawInnerGlow(std::vector<uint8_t>& dest, int destW, int destH, 
                       int x, int y,
                       const GlyphBitmap& glyph,
                       float radius, float choke, const uint8_t color[4]);

    // Renders an advanced lighting-based bevel effect
    void DrawBevel(std::vector<uint8_t>& dest, int destW, int destH,
                   int x, int y,
                   const GlyphBitmap& glyph,
                   float distance, float angle, float spread, float strength, int type,
                   const uint8_t highlightColor[4], const uint8_t shadowColor[4]);

    // Renders a pattern overlay effect
    void ApplyPatternOverlay(std::vector<uint8_t>& dest, int destW, int destH,
                             int x, int y,
                             const GlyphBitmap& glyph,
                             const PatternData& pattern);

    // Returns the pixels of a cached pattern or loads it. Returns true if successful.
    bool GetPatternPixels(const std::string& path, std::vector<uint8_t>& outPixels, int& outW, int& outH);

    // Saves an RGBA image buffer to a PNG file.
    bool SaveImage(const std::string& path, int width, int height, const std::vector<uint8_t>& pixels);
    
    // Clears the internal pattern cache
    void ClearPatternCache();

    // Fills a rectangle with a semi-transparent color.
    void FillRect(std::vector<uint8_t>& dest, int destW, int destH, 
                  int rx, int ry, int rw, int rh, const uint8_t color[4]);

}
