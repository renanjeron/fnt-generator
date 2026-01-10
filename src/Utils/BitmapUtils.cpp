#include "BitmapUtils.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>
#include <map>
#include <mutex>

#include "stb_image.h"
#include "stb_image.h"
#include "stb_image_write.h"

namespace BitmapUtils {

    // Simple cache for pattern images
    struct CachedImage {
        std::vector<uint8_t> pixels;
        int w, h, channels;
    };
    static std::map<std::string, CachedImage> g_PatternCache;
    static std::mutex g_CacheMutex;

    // Gamma Correction Utilities
    inline float sRGBToLinear(float v) {
        return (v <= 0.04045f) ? (v / 12.92f) : powf((v + 0.055f) / 1.055f, 2.4f);
    }
    inline float LinearToSRGB(float v) {
        return (v <= 0.0031308f) ? (v * 12.92f) : (1.055f * powf(v, 1.0f / 2.4f) - 0.055f);
    }

    // Blend Helper: Normal blend of two RGBA colors (straight alpha) using Linear Light
    inline void BlendPixels(uint8_t* dst, uint8_t r, uint8_t g, uint8_t b, float sa) {
        if (sa <= 0.001f) return;
        if (sa >= 0.999f) {
            dst[0] = r; dst[1] = g; dst[2] = b;
            return;
        }

        float dr = sRGBToLinear(dst[0] / 255.0f);
        float dg = sRGBToLinear(dst[1] / 255.0f);
        float db = sRGBToLinear(dst[2] / 255.0f);

        float sr = sRGBToLinear(r / 255.0f);
        float sg = sRGBToLinear(g / 255.0f);
        float sb = sRGBToLinear(b / 255.0f);

        float resR = sr * sa + dr * (1.0f - sa);
        float resG = sg * sa + dg * (1.0f - sa);
        float resB = sb * sa + db * (1.0f - sa);

        dst[0] = (uint8_t)(LinearToSRGB(resR) * 255.0f);
        dst[1] = (uint8_t)(LinearToSRGB(resG) * 255.0f);
        dst[2] = (uint8_t)(LinearToSRGB(resB) * 255.0f);
    }

    // Blend Modes Math (0-1 floats)
    float BlendMultiply(float b, float s) { return b * s; }
    float BlendScreen(float b, float s) { return b + s - b * s; }
    float BlendOverlay(float b, float s) { return (b < 0.5f) ? (2.0f * b * s) : (1.0f - 2.0f * (1.0f - b) * (1.0f - s)); }
    float BlendSoftLight(float b, float s) { return (s < 0.5f) ? (b - (1.0f - 2.0f * s) * b * (1.0f - b)) : (b + (2.0f * s - 1.0f) * (sqrtf(b) - b)); }
    float BlendHardLight(float b, float s) { return BlendOverlay(s, b); }
    float BlendDarken(float b, float s) { return std::min(b, s); }
    float BlendLighten(float b, float s) { return std::max(b, s); }
    float BlendColorDodge(float b, float s) { return (s >= 1.0f) ? 1.0f : std::min(1.0f, b / (1.0f - s)); }
    float BlendColorBurn(float b, float s) { return (s <= 0.0f) ? 0.0f : 1.0f - std::min(1.0f, (1.0f - b) / s); }

    // Helper to apply blend mode
    static void ApplyBlend(uint8_t& dr, uint8_t& dg, uint8_t& db, uint8_t& da,
                           uint8_t sr, uint8_t sg, uint8_t sb, uint8_t sa,
                           BlendMode mode) 
    {
        float bR = sRGBToLinear(dr / 255.0f);
        float bG = sRGBToLinear(dg / 255.0f);
        float bB = sRGBToLinear(db / 255.0f);
        // float bA = da / 255.0f; // Unused in blend calc?
        float bA = da / 255.0f;

        float sR = sRGBToLinear(sr / 255.0f);
        float sG = sRGBToLinear(sg / 255.0f);
        float sB = sRGBToLinear(sb / 255.0f);
        float sA = sa / 255.0f;

        float rR = bR, rG = bG, rB = bB;

        switch (mode) {
            case BlendMode::Multiply: rR = BlendMultiply(bR, sR); rG = BlendMultiply(bG, sG); rB = BlendMultiply(bB, sB); break;
            case BlendMode::Screen: rR = BlendScreen(bR, sR); rG = BlendScreen(bG, sG); rB = BlendScreen(bB, sB); break;
            case BlendMode::Overlay: rR = BlendOverlay(bR, sR); rG = BlendOverlay(bG, sG); rB = BlendOverlay(bB, sB); break;
            case BlendMode::Darken: rR = BlendDarken(bR, sR); rG = BlendDarken(bG, sG); rB = BlendDarken(bB, sB); break;
            case BlendMode::Lighten: rR = BlendLighten(bR, sR); rG = BlendLighten(bG, sG); rB = BlendLighten(bB, sB); break;
            case BlendMode::ColorDodge: rR = BlendColorDodge(bR, sR); rG = BlendColorDodge(bG, sG); rB = BlendColorDodge(bB, sB); break;
            case BlendMode::ColorBurn: rR = BlendColorBurn(bR, sR); rG = BlendColorBurn(bG, sG); rB = BlendColorBurn(bB, sB); break;
            case BlendMode::HardLight: rR = BlendHardLight(bR, sR); rG = BlendHardLight(bG, sG); rB = BlendHardLight(bB, sB); break;
            case BlendMode::SoftLight: rR = BlendSoftLight(bR, sR); rG = BlendSoftLight(bG, sG); rB = BlendSoftLight(bB, sB); break;
            case BlendMode::Difference: rR = std::abs(bR - sR); rG = std::abs(bG - sG); rB = std::abs(bB - sB); break; // Simple diff
            case BlendMode::Exclusion: rR = bR + sR - 2.0f * bR * sR; rG = bG + sG - 2.0f * bG * sG; rB = bB + sB - 2.0f * bB * sB; break;
            case BlendMode::Subtract: rR = std::max(0.0f, bR - sR); rG = std::max(0.0f, bG - sG); rB = std::max(0.0f, bB - sB); break;
            case BlendMode::Divide: rR = (sR > 0.0f) ? std::min(1.0f, bR / sR) : 1.0f; rG = (sG > 0.0f) ? std::min(1.0f, bG / sG) : 1.0f; rB = (sB > 0.0f) ? std::min(1.0f, bB / sB) : 1.0f; break;
            default: rR = sR; rG = sG; rB = sB; break; // Normal (replaces if alpha=1)
        }

        // Output Alpha (Union)
        float outA = sA + bA * (1.0f - sA);
        if (outA <= 0.0f) {
            da = 0; dr = 0; dg = 0; db = 0;
            return;
        }

        // Mix blended color with original based on Source Alpha (sA) (standard compositing)
        // Dest = Dest * (1-sa) + Blended * sa
        float fR = (1.0f - sA) * bR + sA * rR;
        float fG = (1.0f - sA) * bG + sA * rG;
        float fB = (1.0f - sA) * bB + sA * rB;
        
        dr = (uint8_t)(LinearToSRGB(fR) * 255.0f);
        dg = (uint8_t)(LinearToSRGB(fG) * 255.0f);
        db = (uint8_t)(LinearToSRGB(fB) * 255.0f);
        da = (uint8_t)(outA * 255.0f);
    }

    bool GetPatternPixels(const std::string& path, std::vector<uint8_t>& outPixels, int& outW, int& outH) {
        if (path.empty()) return false;
        
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        if (g_PatternCache.count(path)) {
            const auto& img = g_PatternCache[path];
            outPixels = img.pixels;
            outW = img.w; outH = img.h;
            return true;
        }

        // Try load
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> fileBuf(size);
        if (fread(fileBuf.data(), 1, size, f) != size) {
            // Handle error or just ignore but silence warning
        }
        fclose(f);

        int w, h, channels;
        unsigned char* data = stbi_load_from_memory(fileBuf.data(), (int)size, &w, &h, &channels, 4);
        if (data) {
            CachedImage ci;
            ci.w = w; ci.h = h; ci.channels = 4;
            ci.pixels.assign(data, data + (w * h * 4));
            stbi_image_free(data);
            
            outPixels = ci.pixels;
            outW = ci.w; outH = ci.h;
            
            g_PatternCache[path] = std::move(ci);
            return true;
        }
        return false;
    }

    void ApplyPatternOverlay(std::vector<uint8_t>& dest, int destW, int destH,
                             int x, int y,
                             const GlyphBitmap& glyph,
                             const PatternData& pattern)
    {
        if (glyph.buffer.empty() || glyph.width == 0 || glyph.height == 0) return;
        if (pattern.imagePath.empty() || !pattern.enabled) return;

        // 1. Get/Cache image
        std::vector<uint8_t> imgPixels;
        int imgW, imgH;
        if (!GetPatternPixels(pattern.imagePath, imgPixels, imgW, imgH)) return;

        // 2. Prep transformations
        float angRad = pattern.angle * 3.14159265f / 180.0f;
        float cosA = std::cos(angRad);
        float sinA = std::sin(angRad);
        float invScale = 1.0f / (std::max(0.01f, pattern.scale));

        for (int gy = 0; gy < glyph.height; gy++) {
            for (int gx = 0; gx < glyph.width; gx++) {
                int dx = x + gx;
                int dy = y + gy;
                if (dx < 0 || dx >= destW || dy < 0 || dy >= destH) continue;

                uint8_t alpha = glyph.buffer[gy * glyph.width + gx];
                if (alpha == 0) continue;

                // 3. Coordinate Transformation (World -> Pattern Space)
                float rx, ry;
                if (pattern.mappingMode == PatternMapping::Glyph) {
                    rx = (float)gx * invScale;
                    ry = (float)gy * invScale;
                } else {
                    rx = (float)dx * invScale;
                    ry = (float)dy * invScale;
                }

                // Rotate coordinates
                float px = rx * cosA + ry * sinA;
                float py = -rx * sinA + ry * cosA;

                // Tiling (Wrap)
                int tx = ((int)px % imgW + imgW) % imgW;
                int ty = ((int)py % imgH + imgH) % imgH;

                int pIdx = (ty * imgW + tx) * 4;
                uint8_t pr = imgPixels[pIdx + 0];
                uint8_t pg = imgPixels[pIdx + 1];
                uint8_t pb = imgPixels[pIdx + 2];
                uint8_t pa_raw = imgPixels[pIdx + 3];

                float sa = (pa_raw / 255.0f) * pattern.opacity * (alpha / 255.0f);
                if (sa <= 0.001f) continue;

                int dIdx = (dy * destW + dx) * 4;
                uint8_t dr = dest[dIdx + 0];
                uint8_t dg = dest[dIdx + 1];
                uint8_t db = dest[dIdx + 2];

                // 4. Blend Modes
                if (pattern.blendMode != BlendMode::Normal) {
                    ApplyBlend(dest[dIdx], dest[dIdx+1], dest[dIdx+2], dest[dIdx+3],
                               pr, pg, pb, (uint8_t)(sa * 255.0f),
                               pattern.blendMode);
                } else {
                    BlendPixels(&dest[dIdx], pr, pg, pb, sa);
                }
            }
        }
    }

    void SampleGradient(const std::vector<GradientStop>& stops, float t, uint8_t* outColor) {
        if (stops.empty()) {
            outColor[0] = 255; outColor[1] = 255; outColor[2] = 255; outColor[3] = 255;
            return;
        }
        if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

        const GradientStop* s1 = &stops.front();
        if (t <= s1->position) {
            outColor[0] = (uint8_t)(s1->color[0] * 255); outColor[1] = (uint8_t)(s1->color[1] * 255);
            outColor[2] = (uint8_t)(s1->color[2] * 255); outColor[3] = (uint8_t)(s1->color[3] * 255);
            return;
        }
        const GradientStop* s2 = &stops.back();
        if (t >= s2->position) {
            outColor[0] = (uint8_t)(s2->color[0] * 255); outColor[1] = (uint8_t)(s2->color[1] * 255);
            outColor[2] = (uint8_t)(s2->color[2] * 255); outColor[3] = (uint8_t)(s2->color[3] * 255);
            return;
        }
        for (size_t i = 0; i < stops.size() - 1; ++i) {
            if (t >= stops[i].position && t <= stops[i+1].position) {
                s1 = &stops[i]; s2 = &stops[i+1];
                float den = s2->position - s1->position;
                float f = (den <= 1e-5f) ? 0.0f : (t - s1->position) / den;
                for(int k=0;k<4;k++) outColor[k] = (uint8_t)((s1->color[k]*(1.0f-f) + s2->color[k]*f) * 255.0f);
                return;
            }
        }
    }

std::vector<uint8_t> ApplyGaussianBlur(const std::vector<uint8_t>& src, int w, int h, float radius) {
    if (radius <= 0.1f || src.empty()) return src;
    
    std::vector<uint8_t> temp(src.size());
    std::vector<uint8_t> dest(src.size());
    
    float sigma = std::max(1.0f, radius / 2.0f);
    int kSize = (int)ceil(2.0f * 3.0f * sigma) | 1; 
    if (kSize < 3) kSize = 3;
    
    std::vector<float> kernel(kSize);
    float sum = 0.0f;
    int half = kSize / 2;
    for (int i = 0; i < kSize; i++) {
        float x = (float)(i - half);
        kernel[i] = exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < kSize; i++) kernel[i] /= sum;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float val = 0.0f;
            for (int k = -half; k <= half; k++) {
                int px = std::min(std::max(x + k, 0), w - 1);
                val += src[y * w + px] * kernel[k + half];
            }
            temp[y * w + x] = (uint8_t)std::min(255.0f, val);
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float val = 0.0f;
            for (int k = -half; k <= half; k++) {
                int py = std::min(std::max(y + k, 0), h - 1);
                val += temp[py * w + x] * kernel[k + half];
            }
            dest[y * w + x] = (uint8_t)std::min(255.0f, val);
        }
    }
    return dest;
}

void BlitGlyph(std::vector<uint8_t>& dest, int destW, int destH, 
               int x, int y, 
               const GlyphBitmap& glyph, 
               const uint8_t colorTop[4], 
               const GradientData* gradient,
               bool maskToDest) 
{
    if (glyph.buffer.empty() || glyph.width == 0 || glyph.height == 0) return;
    if (dest.empty() || destW == 0) return;
    
    // Validate gradient
    bool useGrad = (gradient && gradient->enabled && !gradient->stops.empty());

    for (int gy = 0; gy < glyph.height; gy++) {
        for (int gx = 0; gx < glyph.width; gx++) {
            int destX = x + gx;
            int destY = y + gy;
            
            if (destX < 0 || destX >= destW) continue;
            if (destY < 0 || destY >= destH) continue;
             
            int idx = (destY * destW + destX) * 4;
            if (idx >= (int)dest.size() || idx < 0) continue;
            
            if (maskToDest && dest[idx+3] == 0) continue;

            int glyphIdx = gy * glyph.width + gx;
            if (glyphIdx >= (int)glyph.buffer.size()) continue;

            unsigned char srcAlpha = glyph.buffer[glyphIdx];
            if (srcAlpha == 0) continue;
            
            uint8_t finalR, finalG, finalB, globalAlpha;

            if (useGrad) {
                float t = 0.0f;
                if (gradient->type == GradientType::Linear) {
                     float angRad = gradient->angle * 3.14159265f / 180.0f;
                     float c = std::cos(angRad);
                     float s = std::sin(angRad);
                     
                     float w = (float)glyph.width;
                     float h = (float)glyph.height;
                     float cx = w * 0.5f;
                     float cy = h * 0.5f;
                     
                     float px = gx - cx;
                     float py = gy - cy;
                     float t_curr = px * c + py * s;
                     
                     float corners[4][2] = { {-cx, -cy}, {w-cx, -cy}, {w-cx, h-cy}, {-cx, h-cy} };
                     float minT = 1e10f, maxT = -1e10f;
                     for(int i=0; i<4; i++) {
                         float proj = corners[i][0] * c + corners[i][1] * s;
                         if(proj < minT) minT = proj;
                         if(proj > maxT) maxT = proj;
                     }
                     
                     if (std::abs(maxT - minT) < 0.001f) t = 0.0f;
                     else t = (t_curr - minT) / (maxT - minT);
                } else {
                     // Radial (Elliptical): From center, rotated
                     float angRad = gradient->angle * 3.14159265f / 180.0f;
                     float c = std::cos(angRad); 
                     float s = std::sin(angRad);
                     
                     float cx = (glyph.width - 1) / 2.0f;
                     float cy = (glyph.height - 1) / 2.0f;
                     float dx = gx - cx;
                     float dy = gy - cy;
                     
                     // Rotate point into gradient space (Inverse rotation of shape)
                     // Rotate by -angle
                     float rx = dx * c + dy * s;
                     float ry = -dx * s + dy * c;
                     
                     float radiusX = (float)glyph.width / 2.0f;
                     float radiusY = (float)glyph.height / 2.0f;
                     if (radiusX < 0.1f) radiusX = 0.1f;
                     if (radiusY < 0.1f) radiusY = 0.1f;
                     
                     // Elliptical distance
                     t = std::sqrt(std::pow(rx / radiusX, 2.0f) + std::pow(ry / radiusY, 2.0f));
                }
                
                uint8_t gradCol[4];
                SampleGradient(gradient->stops, t, gradCol);
                finalR = gradCol[0];
                finalG = gradCol[1];
                finalB = gradCol[2];
                globalAlpha = gradCol[3];
            } else {
                finalR = colorTop[0];
                finalG = colorTop[1];
                finalB = colorTop[2];
                globalAlpha = colorTop[3];
            }
            
            float sa = (srcAlpha / 255.0f) * (globalAlpha / 255.0f);
            
            float sr = sRGBToLinear(finalR / 255.0f);
            float sg = sRGBToLinear(finalG / 255.0f);
            float sb = sRGBToLinear(finalB / 255.0f);

            if (dest[idx + 3] == 0) {
                // Straight Alpha replacement (Cleaner edges)
                dest[idx + 0] = finalR;
                dest[idx + 1] = finalG;
                dest[idx + 2] = finalB;
                dest[idx + 3] = (uint8_t)(sa * 255.0f);
            } else {
                float da = dest[idx + 3] / 255.0f;
                float dr = sRGBToLinear(dest[idx + 0] / 255.0f);
                float dg = sRGBToLinear(dest[idx + 1] / 255.0f);
                float db = sRGBToLinear(dest[idx + 2] / 255.0f);

                // Result Alpha
                float outA = sa + da * (1.0f - sa);
                if (outA <= 0.0f) continue;

                // Result RGB (Balanced Linear)
                float r = (sr * sa + dr * da * (1.0f - sa)) / outA;
                float g = (sg * sa + dg * da * (1.0f - sa)) / outA;
                float b = (sb * sa + db * da * (1.0f - sa)) / outA;

                dest[idx + 0] = (uint8_t)(LinearToSRGB(r) * 255.0f);
                dest[idx + 1] = (uint8_t)(LinearToSRGB(g) * 255.0f);
                dest[idx + 2] = (uint8_t)(LinearToSRGB(b) * 255.0f);
                dest[idx + 3] = (uint8_t)(outA * 255.0f);
            }
        }
    }
}

// Helper to apply blend mode - REMOVED (Moved to top)

void DrawInnerGlow(std::vector<uint8_t>& dest, int destW, int destH, 
                   int x, int y,
                   const GlyphBitmap& glyph,
                   float radius, float choke, const uint8_t color[4],
                   BlendMode blendMode)
{
    if (radius <= 0) return;
    if (glyph.buffer.empty() || glyph.width == 0 || glyph.height == 0) return;

    // 1. Create Inverted Mask (Inside=0, Outside=255)
    int w = glyph.width;
    int h = glyph.height;
    std::vector<uint8_t> invertedMask(w * h);
    for (size_t i = 0; i < invertedMask.size(); i++) {
        // Glyph Buffer: 255 = Solid (Inside), 0 = Transparent (Outside)
        // Inverted: 0 = Inside, 255 = Outside
        invertedMask[i] = 255 - glyph.buffer[i];
    }

    // 2. Blur the Inverted Mask
    // "Outside" white bleeds into "Inside" black. 
    // The bleed is the glow mask.
    std::vector<uint8_t> blurred = ApplyGaussianBlur(invertedMask, w, h, radius);
    
    float chokeVal = choke / 100.0f; 
    // Remap choke: 0 means normal gradient. 1.0 means hard edge at full distance.
    // However, blurred distance is non-linear.
    // Simple enhancement:
    // If value < choke, it's 0? No, standard choke implies it stays solid longer.
    // Since we are blurring white inwards:
    // High values (near 255) are deep outside.
    // Low values (near 0) are deep inside.
    // Edge is around 128 (if AA) or gradient.
    // Actually, "Glow" is the presence of White inside the Black area.
    // So we use 'blurred' value as intensity.
    
    float opac = color[3] / 255.0f;

    for (int gy = 0; gy < h; gy++) {
        for (int gx = 0; gx < w; gx++) {
            int idx = gy * w + gx;
            
            // Clip to Glyph (Internal Only)
            uint8_t glyphAlpha = glyph.buffer[idx];
            if (glyphAlpha == 0) continue;

            int dx = x + gx;
            int dy = y + gy;
            if (dx < 0 || dx >= destW || dy < 0 || dy >= destH) continue;
            
            // Get Glow Intensity from Blurred Inverted Mask
            uint8_t glowVal = blurred[idx];
        
            
            float gv = glowVal / 255.0f;
            if (chokeVal > 0.0f) {
                 // Improved Choke: Linear Gain
                 // effectively compresses the 0..1 range to 0 .. (1-choke)
                 // e.g. choke 0.5: range 0..0.5 maps to 0..1. Values > 0.5 become 1.0 (clamped)
                 
                 float gain = 1.0f / std::max(0.01f, 1.0f - chokeVal);
                 gv = std::min(1.0f, gv * gain);
            }
            
            float finalAlpha = gv * (glyphAlpha / 255.0f) * opac;
            if (finalAlpha <= 0.005f) continue;
            
            // Blend
            int dstIdx = (dy * destW + dx) * 4;
            ApplyBlend(dest[dstIdx], dest[dstIdx+1], dest[dstIdx+2], dest[dstIdx+3],
                       color[0], color[1], color[2], (uint8_t)(finalAlpha * 255.0f),
                       blendMode);
        }
    }
}

void DrawBevel(std::vector<uint8_t>& dest, int destW, int destH,
               int x, int y,
               const GlyphBitmap& glyph,
               float distance, float angle, float spread, float strength, int type,
               const uint8_t highlightColor[4], const uint8_t shadowColor[4])
{
    if (glyph.buffer.empty() || glyph.width == 0 || glyph.height == 0) return;
    if (distance <= 0.1f) return;

    // 1. Create Heightmap (Blurred alpha)
    // Spread controls the "roundness" of the edges
    float blurRadius = std::max(1.0f, spread);
    std::vector<uint8_t> heightmap = ApplyGaussianBlur(glyph.buffer, glyph.width, glyph.height, blurRadius);

    // 2. Light Direction
    float angRad = angle * 3.14159265f / 180.0f;
    float lx = std::cos(angRad);
    float ly = std::sin(angRad);

    for (int gy = 0; gy < glyph.height; gy++) {
        for (int gx = 0; gx < glyph.width; gx++) {
            int dx = x + gx;
            int dy = y + gy;
            if (dx < 0 || dx >= destW || dy < 0 || dy >= destH) continue;

            // Masking
            uint8_t alpha = glyph.buffer[gy * glyph.width + gx];
            if (type == 0 && alpha == 0) continue; // Inner: only on glyph
            if (type == 1 && alpha > 128) continue; // Outer: only outside (simplified)

            // 3. Normal Calculation (Central Differences)
            float nx = 0, ny = 0;
            if (gx > 0 && gx < glyph.width - 1)
                nx = (heightmap[gy * glyph.width + (gx + 1)] - heightmap[gy * glyph.width + (gx - 1)]) / 255.0f;
            if (gy > 0 && gy < glyph.height - 1)
                ny = (heightmap[(gy + 1) * glyph.width + gx] - heightmap[(gy - 1) * glyph.width + gx]) / 255.0f;

            // 4. Lighting Calculation (Facing light = highlight)
            // Photoshop-style: dot is positive when surface faces light
            float dot = -(nx * lx + ny * ly) * strength * (distance / 5.0f);
            
            if (std::abs(dot) < 0.001f) continue;

            const uint8_t* col = (dot > 0) ? highlightColor : shadowColor;
            float intensity = std::abs(dot);
            if (intensity > 1.0f) intensity = 1.0f;

            // Blend
            int idx = (dy * destW + dx) * 4;
            float sa = (col[3] / 255.0f) * intensity;
            
            if (type == 0) {
                 // Inner: Restricted by glyph alpha
                 sa *= (alpha / 255.0f);
            } else {
                 // Outer: Bevel adds its own alpha
                 uint8_t newA = (uint8_t)std::min(255.0f, dest[idx+3] + sa * 255.0f);
                 dest[idx+3] = newA;
            }

            dest[idx + 0] = (uint8_t)(col[0] * sa + dest[idx + 0] * (1.0f - sa));
            dest[idx + 1] = (uint8_t)(col[1] * sa + dest[idx + 1] * (1.0f - sa));
            dest[idx + 2] = (uint8_t)(col[2] * sa + dest[idx + 2] * (1.0f - sa));
        }
    }
}

void FillRect(std::vector<uint8_t>& dest, int destW, int destH, 
              int rx, int ry, int rw, int rh, const uint8_t color[4])
{
    if (dest.empty() || destW == 0 || destH == 0) return;
    
    int startX = std::max(0, rx);
    int startY = std::max(0, ry);
    int endX = std::min(destW, rx + rw);
    int endY = std::min(destH, ry + rh);
    
    float sa = color[3] / 255.0f;
    for (int y = startY; y < endY; y++) {
        for (int x = startX; x < endX; x++) {
            int idx = (y * destW + x) * 4;
            dest[idx + 0] = (uint8_t)(color[0] * sa + dest[idx + 0] * (1.0f - sa));
            dest[idx + 1] = (uint8_t)(color[1] * sa + dest[idx + 1] * (1.0f - sa));
            dest[idx + 2] = (uint8_t)(color[2] * sa + dest[idx + 2] * (1.0f - sa));
            dest[idx + 3] = (uint8_t)std::min(255.0f, (dest[idx + 3] + sa * 255.0f));
        }
    }

    }

    void ResizeImage(const std::vector<uint8_t>& src, int srcW, int srcH, 
                     std::vector<uint8_t>& dest, int destW, int destH) {
         if (src.empty() || srcW <= 0 || srcH <= 0 || destW <= 0 || destH <= 0) return;
         dest.resize(destW * destH * 4);
         
         // Simple Bilinear Scale
         float xRatio = ((float)(srcW - 1)) / destW;
         float yRatio = ((float)(srcH - 1)) / destH;
         
         for (int i = 0; i < destH; i++) {
             for (int j = 0; j < destW; j++) {
                 int x = (int)(xRatio * j);
                 int y = (int)(yRatio * i);
                 float xDiff = (xRatio * j) - x;
                 float yDiff = (yRatio * i) - y;
                 
                 int index = (y * srcW + x) * 4;
                 
                 // Pixel A
                 uint8_t aR = src[index]; 
                 uint8_t aG = src[index+1];
                 uint8_t aB = src[index+2];
                 uint8_t aA = src[index+3];
                 
                 // Pixel B
                 int indexB = index + 4;
                 uint8_t bR = src[indexB];
                 uint8_t bG = src[indexB+1]; 
                 uint8_t bB = src[indexB+2];
                 uint8_t bA = src[indexB+3];
                 
                 // Pixel C
                 int indexC = index + (srcW * 4);
                 uint8_t cR = src[indexC]; 
                 uint8_t cG = src[indexC+1]; 
                 uint8_t cB = src[indexC+2]; 
                 uint8_t cA = src[indexC+3];
                 
                 // Pixel D
                 int indexD = index + (srcW * 4) + 4;
                 uint8_t dR = src[indexD];
                 uint8_t dG = src[indexD+1]; 
                 uint8_t dB = src[indexD+2];
                 uint8_t dA = src[indexD+3];
                 
                 // Interpolate X
                 float col0R = aR * (1 - xDiff) + bR * xDiff;
                 float col0G = aG * (1 - xDiff) + bG * xDiff;
                 float col0B = aB * (1 - xDiff) + bB * xDiff;
                 float col0A = aA * (1 - xDiff) + bA * xDiff;
                 
                 float col1R = cR * (1 - xDiff) + dR * xDiff;
                 float col1G = cG * (1 - xDiff) + dG * xDiff;
                 float col1B = cB * (1 - xDiff) + dB * xDiff;
                 float col1A = cA * (1 - xDiff) + dA * xDiff;
                 
                 // Interpolate Y
                 int destIndex = (i * destW + j) * 4;
                 dest[destIndex]   = (uint8_t)(col0R * (1 - yDiff) + col1R * yDiff);
                 dest[destIndex+1] = (uint8_t)(col0G * (1 - yDiff) + col1G * yDiff);
                 dest[destIndex+2] = (uint8_t)(col0B * (1 - yDiff) + col1B * yDiff);
                 dest[destIndex+3] = (uint8_t)(col0A * (1 - yDiff) + col1A * yDiff);
             }
         }
    }





    std::vector<uint8_t> DilateAlpha(const std::vector<uint8_t>& src, int w, int h, float radius, int& outW, int& outH) {
        int r = (int)std::ceil(radius);
        if (r <= 0) {
            outW = w; outH = h;
            return src;
        }
        
        outW = w + r * 2;
        outH = h + r * 2;
        std::vector<uint8_t> out(outW * outH, 0);
        float rSq = radius * radius;
        
        for (int y = 0; y < outH; y++) {
            // Source-relative center y
            int cy = y - r;
            
            // Optimization: If row is far from any content, skip? 
            // Hard without pre-scan. Just do logic bounds.
            int minKy = std::max(0, cy - r);
            int maxKy = std::min(h - 1, cy + r);
            
            for (int x = 0; x < outW; x++) {
                int cx = x - r;
                int minKx = std::max(0, cx - r);
                int maxKx = std::min(w - 1, cx + r);
                
                uint8_t maxVal = 0;

                for (int sy = minKy; sy <= maxKy; sy++) {
                    float dy = (float)(sy - cy);
                    // Optimization: Check Y distance first
                    if (dy*dy > rSq) continue;

                    for (int sx = minKx; sx <= maxKx; sx++) {
                         float dx = (float)(sx - cx);
                         if (dx*dx + dy*dy <= rSq) {
                             // Correct index logic: src is row-major w x h
                             int val = src[sy * w + sx];
                             if (val > maxVal) {
                                 maxVal = (uint8_t)val;
                                 if (maxVal == 255) goto done_pixel;
                             }
                         }
                    }
                }
                done_pixel:
                out[y * outW + x] = maxVal;
            }
        }
        return out;
    }

    void BlitImage(std::vector<uint8_t>& dest, int destW, int destH, 
                   int x, int y, 
                   const std::vector<uint8_t>& srcPixels, int srcW, int srcH)
    {
        if (srcPixels.empty() || srcW == 0 || srcH == 0) return;
        if (dest.empty() || destW == 0) return;

        for (int sy = 0; sy < srcH; sy++) {
            for (int sx = 0; sx < srcW; sx++) {
                int destX = x + sx;
                int destY = y + sy;

                if (destX < 0 || destX >= destW) continue;
                if (destY < 0 || destY >= destH) continue;

                int srcIdx = (sy * srcW + sx) * 4;
                int destIdx = (destY * destW + destX) * 4;

                uint8_t sr = srcPixels[srcIdx + 0];
                uint8_t sg = srcPixels[srcIdx + 1];
                uint8_t sb = srcPixels[srcIdx + 2];
                uint8_t sa = srcPixels[srcIdx + 3];

                if (sa == 0) continue;

                if (dest[destIdx + 3] == 0) {
                    dest[destIdx + 0] = sr;
                    dest[destIdx + 1] = sg;
                    dest[destIdx + 2] = sb;
                    dest[destIdx + 3] = sa;
                } else {
                    BlendPixels(&dest[destIdx], sr, sg, sb, sa / 255.0f);
                }
            }
        }
    }

    bool SaveImage(const std::string& path, int width, int height, const std::vector<uint8_t>& pixels) {
        if (pixels.empty() || width <= 0 || height <= 0) return false;
        // 4 components for RGBA
        return stbi_write_png(path.c_str(), width, height, 4, pixels.data(), width * 4) != 0;
    }

    void ClearPatternCache() {
        std::lock_guard<std::mutex> lock(g_CacheMutex);
        g_PatternCache.clear();
    }

}


