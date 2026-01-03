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

    // Fills a rectangle with a semi-transparent color.
    void FillRect(std::vector<uint8_t>& dest, int destW, int destH, 
                  int rx, int ry, int rw, int rh, const uint8_t color[4]);

}
