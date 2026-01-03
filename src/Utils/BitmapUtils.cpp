#include "BitmapUtils.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>

namespace BitmapUtils {

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
               bool isGradient,
               const uint8_t colorBottom[3]) 
{
    if (glyph.buffer.empty() || glyph.width == 0 || glyph.height == 0) return;
    if (dest.empty() || destW == 0) return;
    
    for (int gy = 0; gy < glyph.height; gy++) {
        for (int gx = 0; gx < glyph.width; gx++) {
            int destX = x + gx;
            int destY = y + gy;
            
            if (destX < 0 || destX >= destW) continue;
            if (destY < 0 || destY >= destH) continue;
             
            int idx = (destY * destW + destX) * 4;
            if (idx >= (int)dest.size() || idx < 0) continue;
            
            int glyphIdx = gy * glyph.width + gx;
            if (glyphIdx >= (int)glyph.buffer.size()) continue;

            unsigned char srcAlpha = glyph.buffer[glyphIdx];
            if (srcAlpha == 0) continue;
            
            uint8_t finalR = colorTop[0];
            uint8_t finalG = colorTop[1];
            uint8_t finalB = colorTop[2];
            uint8_t globalAlpha = colorTop[3];
            
            if (isGradient && colorBottom) {
                float ratio = (float)gy / (float)glyph.height;
                if (ratio > 1.0f) ratio = 1.0f;
                finalR = (uint8_t)(colorTop[0] * (1.0f - ratio) + colorBottom[0] * ratio);
                finalG = (uint8_t)(colorTop[1] * (1.0f - ratio) + colorBottom[1] * ratio);
                finalB = (uint8_t)(colorTop[2] * (1.0f - ratio) + colorBottom[2] * ratio);
            }
            
            float sa = (srcAlpha / 255.0f) * (globalAlpha / 255.0f);
            
            // Check if destination is fully transparent (or very close)
            if (dest[idx + 3] == 0) {
                // Straight Alpha replacement (Cleaner edges)
                dest[idx + 0] = finalR;
                dest[idx + 1] = finalG;
                dest[idx + 2] = finalB;
                dest[idx + 3] = (uint8_t)(sa * 255.0f);
            } else {
                // Standard Over blending for overlapping parts
                // Note: dest RGB is assumed to be premultiplied or we treat it as straight.
                // For simplicity in this specialized tool, we treat dest as straight too since we just wrote it.
                float da = dest[idx + 3] / 255.0f;
                
                // Result Alpha
                float outA = sa + da * (1.0f - sa);
                if (outA <= 0.0f) continue;

                // Result RGB (Straight)
                // Formula: (Src * Sa + Dst * Da * (1-Sa)) / OutA
                float r = (finalR * sa + dest[idx + 0] * da * (1.0f - sa)) / outA;
                float g = (finalG * sa + dest[idx + 1] * da * (1.0f - sa)) / outA;
                float b = (finalB * sa + dest[idx + 2] * da * (1.0f - sa)) / outA;

                dest[idx + 0] = (uint8_t)std::min(255.0f, r);
                dest[idx + 1] = (uint8_t)std::min(255.0f, g);
                dest[idx + 2] = (uint8_t)std::min(255.0f, b);
                dest[idx + 3] = (uint8_t)(outA * 255.0f);
            }
        }
    }
}

void DrawInnerGlow(std::vector<uint8_t>& dest, int destW, int destH, 
                   int x, int y,
                   const GlyphBitmap& glyph,
                   float radius, float choke, const uint8_t color[4])
{
    if (radius <= 0) return;
    int glowRadius = (int)radius;
    float chokePercent = choke / 100.0f;
    
    for (int gy = 0; gy < glyph.height; gy++) {
        for (int gx = 0; gx < glyph.width; gx++) {
            if (gy * glyph.width + gx >= (int)glyph.buffer.size()) continue;
            if (glyph.buffer[gy * glyph.width + gx] == 0) continue;
            
            int minDist = glowRadius + 1;
            for (int dy = -glowRadius; dy <= glowRadius; dy++) {
                for (int dx = -glowRadius; dx <= glowRadius; dx++) {
                    int checkY = gy + dy, checkX = gx + dx;
                    bool isTransparent = false;
                    if (checkX < 0 || checkX >= glyph.width || checkY < 0 || checkY >= glyph.height) isTransparent = true;
                    else isTransparent = (glyph.buffer[checkY * glyph.width + checkX] < 128);
                    
                    if (isTransparent) {
                        int dist = (int)sqrt(dx*dx + dy*dy);
                        if (dist < minDist) minDist = dist;
                    }
                }
            }
            float eff = minDist + (glowRadius * chokePercent);
            if (eff <= glowRadius) {
                float norm = 1.0f - (eff / (float)glowRadius);
                uint8_t ga = (uint8_t)(color[3] * norm);
                int dx = x + gx, dy = y + gy;
                if (dx >= 0 && dx < destW && dy >= 0 && dy < destH) {
                    int idx = (dy * destW + dx) * 4;
                    float sa = ga / 255.0f;
                    dest[idx + 0] = (uint8_t)(color[0] * sa + dest[idx + 0] * (1.0f - sa));
                    dest[idx + 1] = (uint8_t)(color[1] * sa + dest[idx + 1] * (1.0f - sa));
                    dest[idx + 2] = (uint8_t)(color[2] * sa + dest[idx + 2] * (1.0f - sa));
                }
            }
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

}
