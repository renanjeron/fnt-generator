#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include "../Font/FontManager.h"

namespace BitmapUtils {

    // Applies a separable Gaussian blur to a single-channel (grayscale) image.
    // Returns a new buffer of the same size.
    std::vector<uint8_t> ApplyGaussianBlur(const std::vector<uint8_t>& src, int w, int h, float radius);

    // Blends a glyph bitmap onto a 4-channel RGBA destination buffer.
    // Supports solid color or vertical gradient.
    // colorTop: RGBA (if solid) or Top Color (if gradient)
    // colorBottom: RGB (only used if isGradient=true)
    void BlitGlyph(std::vector<uint8_t>& dest, int destW, int destH, 
                   int x, int y, 
                   const GlyphBitmap& glyph, 
                   const uint8_t colorTop[4], 
                   bool isGradient = false,
                   const uint8_t colorBottom[3] = nullptr);

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
