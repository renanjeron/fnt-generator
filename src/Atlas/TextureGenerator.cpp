#include "TextureGenerator.h"
#include "Utils/BitmapUtils.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdint>
#include <future>
#include <thread>

// Wrapper
AtlasResult TextureGenerator::GenerateAtlas(FontManager& fontManager, const std::string& text, const AtlasSettings& settings) {
    std::vector<uint32_t> charset;
    for (char c : text) charset.push_back((uint8_t)c);
    std::sort(charset.begin(), charset.end());
    charset.erase(std::unique(charset.begin(), charset.end()), charset.end());
    return GenerateAtlas(fontManager, charset, settings);
}

// Core Implementation
AtlasResult TextureGenerator::GenerateAtlas(FontManager& fontManager, const std::vector<uint32_t>& charset, const AtlasSettings& settings) {
    
    // 1. Super Sampling
    int factor = settings.superSamplingFactor;
    if (factor <= 0) factor = 1;

    if (factor > 1) {
        AtlasSettings highSettings = settings;
        highSettings.superSamplingFactor = 1; // Don't recurse
        highSettings.useSuperSampling = false;
        
        highSettings.fontSize *= factor;
        highSettings.padding *= factor;
        highSettings.atlasWidth *= factor;
        highSettings.atlasHeight *= factor;
        highSettings.strokeWidth *= (float)factor;
        highSettings.shadowOffsetX *= factor;
        highSettings.shadowOffsetY *= factor;
        highSettings.shadowBlur *= factor;
        highSettings.bevelDistance *= (float)factor;
        highSettings.bevelSpread *= (float)factor;
        highSettings.innerGlowSize *= (float)factor;
        highSettings.pattern.scale *= (float)factor; // Ensure patterns scale too

        AtlasResult highRes = GenerateAtlas(fontManager, charset, highSettings);
        
        if (highRes.hasErrors && highRes.skippedGlyphs == 0) return highRes;
        
        AtlasResult result;
        result.width = highRes.width / factor;
        result.height = highRes.height / factor;
        result.atlasWidth = highRes.atlasWidth / factor;
        result.atlasHeight = highRes.atlasHeight / factor;
        result.hasErrors = highRes.hasErrors;
        result.errorMessage = highRes.errorMessage;
        result.skippedGlyphs = highRes.skippedGlyphs;
        
        // Downsample Metrics
        result.fontSize = highRes.fontSize / factor;
        result.lineHeight = highRes.lineHeight / factor;
        result.base = highRes.base / factor;

        // Downsample Pixels (Box Filter with Alpha Weighting to prevent dark edges)
        result.pixels.resize(result.width * result.height * 4);
        for(int y=0; y<result.height; y++) {
            for(int x=0; x<result.width; x++) {
                int baseDest = (y*result.width + x)*4;
                
                int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                int totalAlpha = 0;

                for (int sy = 0; sy < factor; sy++) {
                    for (int sx = 0; sx < factor; sx++) {
                        int srcX = x * factor + sx;
                        int srcY = y * factor + sy;
                        int baseSrc = (srcY * highRes.width + srcX) * 4;
                        
                        uint8_t a = highRes.pixels[baseSrc + 3];
                        sumR += highRes.pixels[baseSrc + 0] * a;
                        sumG += highRes.pixels[baseSrc + 1] * a;
                        sumB += highRes.pixels[baseSrc + 2] * a;
                        totalAlpha += a;
                        sumA += a;
                    }
                }
                
                if (totalAlpha == 0) {
                    result.pixels[baseDest + 0] = 0;
                    result.pixels[baseDest + 1] = 0;
                    result.pixels[baseDest + 2] = 0;
                    result.pixels[baseDest + 3] = 0;
                } else {
                    result.pixels[baseDest + 0] = (uint8_t)(sumR / totalAlpha);
                    result.pixels[baseDest + 1] = (uint8_t)(sumG / totalAlpha);
                    result.pixels[baseDest + 2] = (uint8_t)(sumB / totalAlpha);
                    result.pixels[baseDest + 3] = (uint8_t)(sumA / (factor * factor));
                }
            }
        }
        
        // Downsample Glyphs
        for(const auto& g : highRes.glyphs) {
            GlyphPlacement ng = g;
            ng.x /= factor; ng.y /= factor; ng.width /= factor; ng.height /= factor;
            ng.xoffset /= factor; ng.yoffset /= factor; ng.advance /= factor;
            result.glyphs.push_back(ng);
        }
        return result;
    }

    AtlasResult result;
    if (!fontManager.IsLoaded()) return result;

    // Ensure size is set correctly before anything else
    int targetFontSize = (settings.fontSize > 0) ? settings.fontSize : 72;
    fontManager.SetSize(targetFontSize);

    struct RenderedChar {
        uint32_t code;
        GlyphBitmap body;
        GlyphBitmap outline;
        int packingWidth, packingHeight;
        int penOffsetX, penOffsetY;
        int atlasX = 0, atlasY = 0;
        bool skip = false;
    };
    
    std::vector<RenderedChar> chars;
    size_t limit = 4096; 
    if (charset.size() > limit) {
        result.hasErrors = true;
        result.errorMessage = "Too many characters selected! Limiting to " + std::to_string(limit) + ".";
    }

    // 2. Measure & Render
    for (uint32_t code : charset) {
        if (chars.size() >= limit) break;
        if (!fontManager.HasGlyph(code)) continue;

        RenderedChar rc;
        rc.code = code;
        
        // Determine flags based on hinting mode
        FT_Int32 flags = FT_LOAD_RENDER;
        if (settings.hintingMode == 0) flags |= (FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT); // Smooth
        else if (settings.hintingMode == 1) flags |= FT_LOAD_FORCE_AUTOHINT;                // Sharp
        else flags |= FT_LOAD_TARGET_NORMAL;                                                // Crisp/Default

        rc.body = fontManager.RenderGlyph(code, flags);
        
        if (rc.body.width == 0 && rc.body.height == 0 && rc.body.advance == 0 && code != 32 && code != 160) continue;

        if (settings.enableStroke && settings.strokeWidth > 0) {
            float effWidth = settings.strokeWidth;
            if (settings.strokePosition == 1) effWidth *= 0.5f; // Center: Half width (drawn on top/bottom effective)
            rc.outline = fontManager.RenderGlyphStroke(code, effWidth, flags);
        } else {
            rc.outline = rc.body; 
        }

        int minX = rc.outline.bearingX;
        int maxX = rc.outline.bearingX + rc.outline.width;
        int minY = -rc.outline.bearingY;
        int maxY = -rc.outline.bearingY + rc.outline.height;
        
        if (settings.enableShadow) {
            int blur = settings.shadowBlur;
            int sx = settings.shadowOffsetX;
            int sy = settings.shadowOffsetY;
            
            int sMinX = rc.outline.bearingX + sx - blur;
            int sMaxX = rc.outline.bearingX + rc.outline.width + sx + blur;
            int sMinY = -rc.outline.bearingY + sy - blur;
            int sMaxY = -rc.outline.bearingY + rc.outline.height + sy + blur;
            
            if (sMinX < minX) minX = sMinX;
            if (sMaxX > maxX) maxX = sMaxX;
            if (sMinY < minY) minY = sMinY;
            if (sMaxY > maxY) maxY = sMaxY;
        }
        
        rc.packingWidth = (maxX - minX) + 2; 
        rc.packingHeight = (maxY - minY) + 2;
        rc.penOffsetX = -minX + 1;
        rc.penOffsetY = -minY + 1;
        
        chars.push_back(rc);
    }
    
    std::sort(chars.begin(), chars.end(), [](const RenderedChar& a, const RenderedChar& b) {
        return a.code < b.code;
    });

    // 4. Pack
    int atlasWidth = settings.atlasWidth;
    int atlasHeight = settings.atlasHeight;

    // Handle Auto-size (0 means Auto)
    if (atlasWidth <= 0 || atlasHeight <= 0) {
        if (atlasWidth <= 0 && atlasHeight <= 0) {
            // Both auto: Find smallest square POT
            int currentS = 128;
            bool fits = false;
            while (!fits && currentS <= 4096) {
                int vX = settings.padding;
                int vY = settings.padding;
                int vRowH = 0;
                fits = true;
                for (const auto& rc : chars) {
                    if (vX + rc.packingWidth + settings.padding > currentS) {
                        vX = settings.padding;
                        vY += vRowH + settings.padding;
                        vRowH = 0;
                        if (vY + rc.packingHeight + settings.padding > currentS) {
                            fits = false;
                            break;
                        }
                    }
                    if (rc.packingHeight > vRowH) vRowH = rc.packingHeight;
                    vX += rc.packingWidth + settings.padding;
                    if (vX > currentS || vY + vRowH + settings.padding > currentS) {
                        fits = false;
                        break;
                    }
                }
                if (!fits) currentS *= 2;
            }
            atlasWidth = currentS;
            
            // Now minimize height for this width
            int vX = settings.padding;
            int vY = settings.padding;
            int vRowH = 0;
            for (const auto& rc : chars) {
                if (vX + rc.packingWidth + settings.padding > atlasWidth) {
                    vX = settings.padding;
                    vY += vRowH + settings.padding;
                    vRowH = 0;
                }
                if (rc.packingHeight > vRowH) vRowH = rc.packingHeight;
                vX += rc.packingWidth + settings.padding;
            }
            int neededH = vY + vRowH + settings.padding;
            atlasHeight = 128;
            while (atlasHeight < neededH && atlasHeight < 4096) atlasHeight *= 2;
            
            // Safety: ensure it doesn't exceed 4096 and is at least POT of 128
            if (atlasHeight > 4096) atlasHeight = 4096;
        } else if (atlasHeight <= 0) {
            // Only Height is auto: Find smallest POT height for fixed width
            int vX = settings.padding;
            int vY = settings.padding;
            int vRowH = 0;
            for (const auto& rc : chars) {
                if (vX + rc.packingWidth + settings.padding > atlasWidth) {
                    vX = settings.padding;
                    vY += vRowH + settings.padding;
                    vRowH = 0;
                }
                if (rc.packingHeight > vRowH) vRowH = rc.packingHeight;
                vX += rc.packingWidth + settings.padding;
            }
            int neededH = vY + vRowH + settings.padding;
            atlasHeight = 128;
            while (atlasHeight < neededH && atlasHeight < 4096) atlasHeight *= 2;
        } else if (atlasWidth <= 0) {
            // Only Width is auto: Find smallest POT width for fixed height
            int currentW = 128;
            bool fits = false;
            while (!fits && currentW <= 4096) {
                int vX = settings.padding;
                int vY = settings.padding;
                int vRowH = 0;
                fits = true;
                for (const auto& rc : chars) {
                    if (vX + rc.packingWidth + settings.padding > currentW) {
                        vX = settings.padding;
                        vY += vRowH + settings.padding;
                        vRowH = 0;
                    }
                    if (vY + rc.packingHeight + settings.padding > atlasHeight) {
                        fits = false;
                        break;
                    }
                    if (rc.packingHeight > vRowH) vRowH = rc.packingHeight;
                    vX += rc.packingWidth + settings.padding;
                }
                if (!fits) currentW *= 2;
            }
            atlasWidth = currentW;
        }
    }
    
    // Virtual Packing: We might need a larger buffer to show out-of-bounds glyphs
    // But we limit it to prevent crashes.
    int packedWidth = atlasWidth;
    int packedHeight = atlasHeight;
    
    // First pass: Calculate required height for ALL glyphs (up to a limit)
    int vX = settings.padding;
    int vY = settings.padding;
    int vRowH = 0;
    int maxVHeight = 8192; // Safety limit
    
    for (const auto& rc : chars) {
        if (vX + rc.packingWidth + settings.padding > atlasWidth) {
            vX = settings.padding;
            vY += vRowH + settings.padding;
            vRowH = 0;
        }
        if (vY + rc.packingHeight + settings.padding > maxVHeight) break;
        if (rc.packingHeight > vRowH) vRowH = rc.packingHeight;
        vX += rc.packingWidth + settings.padding;
    }
    packedHeight = std::max(atlasHeight, vY + vRowH + settings.padding);
    if (packedHeight > maxVHeight) packedHeight = maxVHeight;

    result.width = packedWidth;
    result.height = packedHeight;
    result.atlasWidth = atlasWidth;
    result.atlasHeight = atlasHeight;
    
    // Fill Metrics
    result.fontSize = settings.fontSize;
    result.lineHeight = fontManager.GetLineHeight();
    result.base = fontManager.GetAscender();

    try {
        result.pixels.assign(result.width * result.height * 4, 0);
    } catch (const std::bad_alloc&) {
        result.hasErrors = true;
        result.errorMessage = "Memory allocation failed! Reduce font size or atlas size.";
        result.width = 0; result.height = 0;
        return result;
    }

    // 5. Build Final Result List and Calculate Positions
    int currentX = settings.padding;
    int currentY = settings.padding;
    int currentRowHeight = 0;
    int skippedCount = 0;

    for (auto& rc : chars) {
        if (currentX + rc.packingWidth + settings.padding > atlasWidth) {
            currentX = settings.padding;
            currentY += currentRowHeight + settings.padding;
            currentRowHeight = 0;
        }
        
        if (currentY + rc.packingHeight + settings.padding > result.height) {
            rc.skip = true;
            skippedCount++;
            continue; 
        }

        rc.atlasX = currentX;
        rc.atlasY = currentY;
        
        GlyphPlacement gp;
        gp.charCode = rc.code;
        gp.x = rc.atlasX;
        gp.y = rc.atlasY;
        gp.width = rc.packingWidth;
        gp.height = rc.packingHeight;
        gp.xoffset = -rc.penOffsetX + settings.globalXOffset; 
        gp.yoffset = (fontManager.GetAscender() - rc.penOffsetY) + settings.globalYOffset;
        gp.advance = rc.body.advance + settings.globalXAdvance;
        result.glyphs.push_back(gp);
        
        currentX += rc.packingWidth + settings.padding;
        if (rc.packingHeight > currentRowHeight) currentRowHeight = rc.packingHeight;
    }

    // 6. Parallel Composition
    int numGlyphs = (int)chars.size();
    int numHardwareThreads = std::thread::hardware_concurrency();
    int numThreads = std::max(1, std::min(numHardwareThreads, (numGlyphs / 5) + 1));

    int chunkSize = (numGlyphs + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;

    for (int t = 0; t < numThreads; t++) {
        int startIdx = t * chunkSize;
        int endIdx = std::min(startIdx + chunkSize, numGlyphs);
        if (startIdx >= endIdx) break;

        futures.push_back(std::async(std::launch::async, [&, startIdx, endIdx]() {
            for (int i = startIdx; i < endIdx; i++) {
                const auto& rc = chars[i];
                if (rc.skip) continue;

                int penX = rc.atlasX + rc.penOffsetX; 
                int penY = rc.atlasY + rc.penOffsetY;
                
                // A. Shadow
                if (settings.enableShadow) {
                     int blur = settings.shadowBlur;
                     int drawRefX = penX + rc.outline.bearingX + settings.shadowOffsetX;
                     int drawRefY = penY - rc.outline.bearingY + settings.shadowOffsetY;

                     if (blur > 0) {
                         int sw = rc.outline.width + blur*2;
                         int sh = rc.outline.height + blur*2;
                         if (sw > 0 && sh > 0) {
                            std::vector<uint8_t> shadowBuf(sw * sh, 0);
                            for(int y=0; y<rc.outline.height; y++) {
                                int srcOffset = y * rc.outline.width;
                                int dstOffset = (y+blur)*sw + blur;
                                if(srcOffset < (int)rc.outline.buffer.size() && dstOffset + rc.outline.width <= (int)shadowBuf.size())
                                    memcpy(shadowBuf.data() + dstOffset, rc.outline.buffer.data() + srcOffset, rc.outline.width);
                            }
                            std::vector<uint8_t> blurred = BitmapUtils::ApplyGaussianBlur(shadowBuf, sw, sh, (float)blur);
                            GlyphBitmap shRes; shRes.buffer = blurred; shRes.width = sw; shRes.height = sh;
                            
                            BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, 
                                                   drawRefX - blur, drawRefY - blur, shRes, 
                                                   settings.shadowColor, nullptr);
                         }
                     } else {
                         BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, 
                                                drawRefX, drawRefY, rc.outline, 
                                                settings.shadowColor, nullptr);
                     }
                }

                // composition lambda definitions
                auto DrawStroke = [&]() {
                    if (settings.enableStroke && settings.strokeWidth > 0) {
                         bool mask = (settings.strokePosition == 2); // Inside
                         BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, 
                                               penX + rc.outline.bearingX, penY - rc.outline.bearingY, rc.outline, 
                                               settings.strokeColor, &settings.strokeGradient, mask);
                    }
                };

                auto DrawBody = [&]() {
                    int bodyX = penX + rc.body.bearingX;
                    int bodyY = penY - rc.body.bearingY;
                    
                    if (settings.enableInnerGlow && settings.innerGlowSize > 0) {
                         BitmapUtils::DrawInnerGlow(result.pixels, result.width, result.height, 
                                                    bodyX, bodyY, rc.body, settings.innerGlowSize, settings.innerGlowChoke, settings.innerGlowColor);
                    }
                    
                    BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, 
                                           bodyX, bodyY, rc.body, 
                                           settings.fillColor, &settings.fillGradient);

                    if (settings.pattern.enabled) {
                        BitmapUtils::ApplyPatternOverlay(result.pixels, result.width, result.height,
                                                         bodyX, bodyY, rc.body,
                                                         settings.pattern);
                    }
                };

                if (settings.strokePosition == 0) { // Outside
                    DrawStroke();
                    DrawBody();
                } else { // Center or Inside
                    DrawBody();
                    DrawStroke();
                }

                // B. Bevel
                if (settings.enableBevel && settings.bevelDistance > 0) {
                    BitmapUtils::DrawBevel(result.pixels, result.width, result.height,
                                           penX + rc.body.bearingX, penY - rc.body.bearingY,
                                           rc.body,
                                           settings.bevelDistance, (float)settings.bevelAngle, 
                                           settings.bevelSpread, settings.bevelStrength, settings.bevelType,
                                           settings.bevelHighlightColor, settings.bevelShadowColor);
                }
            }
        }));
    }
    for (auto& f : futures) f.wait();
    
    // Apply Red Tint to out-of-bounds areas
    if (result.height > atlasHeight) {
        uint8_t redTint[4] = {255, 0, 0, 60}; // Semi-transparent red
        BitmapUtils::FillRect(result.pixels, result.width, result.height, 0, atlasHeight, result.width, result.height - atlasHeight, redTint);
    }
    
    if (skippedCount > 0) {
        result.hasErrors = true;
        result.skippedGlyphs = skippedCount;
        result.errorMessage = "Some glyphs were skipped (buffer limit reached).";
    }
    
    return result;
}

// Legacy Preview
AtlasResult TextureGenerator::GenerateTextPreview(FontManager& fontManager, const std::string& text, const AtlasSettings& settings) {
    
    // 1. Super Sampling
    int factor = settings.superSamplingFactor;
    if (factor <= 0) factor = 1;

    if (factor > 1) {
        AtlasSettings highSettings = settings;
        highSettings.superSamplingFactor = 1; 
        
        highSettings.fontSize *= factor;
        highSettings.padding *= factor;
        highSettings.strokeWidth *= (float)factor;
        highSettings.shadowOffsetX *= factor;
        highSettings.shadowOffsetY *= factor;
        highSettings.shadowBlur *= factor;
        highSettings.bevelDistance *= (float)factor;
        highSettings.bevelSpread *= (float)factor;
        highSettings.innerGlowSize *= (float)factor;
        highSettings.pattern.scale *= (float)factor;

        AtlasResult highRes = GenerateTextPreview(fontManager, text, highSettings);
        
        if (highRes.hasErrors) return highRes;
        
        AtlasResult result;
        result.width = highRes.width / factor;
        result.height = highRes.height / factor;
        result.hasErrors = highRes.hasErrors;
        result.errorMessage = highRes.errorMessage;
        
        // Downsample Metrics
        result.fontSize = highRes.fontSize / factor;
        result.lineHeight = highRes.lineHeight / factor;
        result.base = highRes.base / factor;

        // Downsample Pixels (Box Filter with Alpha Weighting to prevent dark edges)
        result.pixels.resize(result.width * result.height * 4);
        for(int y=0; y<result.height; y++) {
            for(int x=0; x<result.width; x++) {
                int baseDest = (y*result.width + x)*4;
                
                int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                int totalAlpha = 0;

                for (int sy = 0; sy < factor; sy++) {
                    for (int sx = 0; sx < factor; sx++) {
                        int srcX = x * factor + sx;
                        int srcY = y * factor + sy;
                        int baseSrc = (srcY * highRes.width + srcX) * 4;
                        
                        uint8_t a = highRes.pixels[baseSrc + 3];
                        sumR += highRes.pixels[baseSrc + 0] * a;
                        sumG += highRes.pixels[baseSrc + 1] * a;
                        sumB += highRes.pixels[baseSrc + 2] * a;
                        totalAlpha += a;
                        sumA += a;
                    }
                }
                
                if (totalAlpha == 0) {
                    result.pixels[baseDest + 0] = 0;
                    result.pixels[baseDest + 1] = 0;
                    result.pixels[baseDest + 2] = 0;
                    result.pixels[baseDest + 3] = 0;
                } else {
                    result.pixels[baseDest + 0] = (uint8_t)(sumR / totalAlpha);
                    result.pixels[baseDest + 1] = (uint8_t)(sumG / totalAlpha);
                    result.pixels[baseDest + 2] = (uint8_t)(sumB / totalAlpha);
                    result.pixels[baseDest + 3] = (uint8_t)(sumA / (factor * factor));
                }
            }
        }
        return result;
    }
    
    AtlasResult result;
    if (!fontManager.IsLoaded()) return result;
    fontManager.SetSize(settings.fontSize);

    // Reuse RenderedChar struct locally or make common? Local is fine.
    struct RenderedChar {
        uint32_t code;
        GlyphBitmap body;
        GlyphBitmap outline; // Added for Real Stroke
        int width, height; 
        int bearingY, bearingX;
        long advance;
        int outlineX, outlineY;
        int bodyX, bodyY;
        int currentPenX;
    };
    std::vector<RenderedChar> chars;
    
    // Bounds Measure (Legacy)
    // ... Implement using same logic as Step 1076 ...
    int minX = 100000, minY = 100000, maxX = -100000, maxY = -100000;
    int currentPenX = 0; 
    int strokeOff = settings.enableStroke ? (int)settings.strokeWidth : 0;
    
    for (char c : text) {
        RenderedChar rc;
        rc.code = (uint32_t)c;
        
        // Determine flags based on hinting mode
        FT_Int32 flags = FT_LOAD_RENDER;
        if (settings.hintingMode == 0) flags |= (FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT); // Smooth
        else if (settings.hintingMode == 1) flags |= FT_LOAD_FORCE_AUTOHINT;                // Sharp
        else flags |= FT_LOAD_TARGET_NORMAL;                                                // Crisp/Default
        
        rc.body = fontManager.RenderGlyph(rc.code, flags);
        
        if (settings.enableStroke && settings.strokeWidth > 0) {
            rc.outline = fontManager.RenderGlyphStroke(rc.code, settings.strokeWidth, flags);
            // Use outline metrics
            rc.width = rc.outline.width;
            rc.height = rc.outline.height;
            rc.bearingY = rc.outline.bearingY; // Top
            rc.bearingX = rc.outline.bearingX; // Left
        } else {
            rc.outline = rc.body;
            int sExt = 0; // No manual expansion needed if using Outline or Body directly
            rc.width = rc.body.width;
            rc.height = rc.body.height;
            rc.bearingY = rc.body.bearingY; 
            rc.bearingX = rc.body.bearingX;
        }
        rc.advance = rc.body.advance;
        
        // 1. Reference Position (No Offset)
        int refLeft = currentPenX + rc.bearingX;
        int refTop = -rc.bearingY;
        int refRight = refLeft + rc.width;
        int refBottom = refTop + rc.height;

        // 2. Shifted Position (With Offset)
        int sLeft = refLeft + settings.globalXOffset;
        int sTop = refTop + settings.globalYOffset;
        int sRight = sLeft + rc.width; 
        int sBottom = sTop + rc.height;

        // Shadow Bounds (applied to Shifted)
        if (settings.enableShadow) {
            int shLeft = sLeft + settings.shadowOffsetX;
            int shTop = sTop + settings.shadowOffsetY;
            int shRight = shLeft + rc.width; 
            int shBottom = shTop + rc.height;
            // Add shadow to bounds
            if (shLeft < minX) minX = shLeft;
            if (shTop < minY) minY = shTop;
            if (shRight > maxX) maxX = shRight;
            if (shBottom > maxY) maxY = shBottom;
        }

        // Union Bounds: Include BOTH Reference and Shifted
        // This anchors the view so offsets create visible movement
        int bMinX = std::min(refLeft, sLeft);
        int bMinY = std::min(refTop, sTop);
        int bMaxX = std::max(refRight, sRight);
        int bMaxY = std::max(refBottom, sBottom);

        if (bMinX < minX) minX = bMinX;
        if (bMinY < minY) minY = bMinY;
        if (bMaxX > maxX) maxX = bMaxX;
        if (bMaxY > maxY) maxY = bMaxY;
        
        chars.push_back(rc);
        currentPenX += rc.advance + settings.globalXAdvance;
    }
    if (currentPenX > maxX) maxX = currentPenX;
    if (chars.empty()) { minX=0; minY=0; maxX=10; maxY=10; }
    if (maxX < minX) maxX = minX + 10;
    if (maxY < minY) maxY = minY + 10;

    int totalWidth = (maxX - minX) + (settings.padding * 2);
    int totalHeight = (maxY - minY) + (settings.padding * 2);
    
    result.width = totalWidth;
    result.height = totalHeight;
    try {
        result.pixels.assign(result.width * result.height * 4, 0);
    } catch (const std::bad_alloc&) {
        result.hasErrors = true;
        result.errorMessage = "Memory allocation failed for preview! Font size too large.";
        result.width = 0; result.height = 0;
        return result;
    }

    int baselineInTex = -minY + settings.padding;
    int startPenXInTex = -minX + settings.padding;
    currentPenX = startPenXInTex;

    for (auto& rc : chars) {
        int drawX = currentPenX + rc.bearingX + settings.globalXOffset;
        int drawY = baselineInTex - rc.bearingY + settings.globalYOffset;

        rc.outlineX = drawX; 
        rc.outlineY = drawY;
        rc.bodyX = currentPenX + rc.body.bearingX + settings.globalXOffset;
        rc.bodyY = baselineInTex - rc.body.bearingY + settings.globalYOffset;
        rc.currentPenX = currentPenX;

        currentPenX += rc.advance + settings.globalXAdvance;
    }

    // Parallel Composition for Preview
    int pNumGlyphs = (int)chars.size();
    int pNumThreads = std::min((int)std::thread::hardware_concurrency(), (pNumGlyphs / 5) + 1);
    if (pNumThreads < 1) pNumThreads = 1;
    
    int pChunkSize = (pNumGlyphs + pNumThreads - 1) / pNumThreads;
    std::vector<std::future<void>> pFutures;

    for (int t = 0; t < pNumThreads; t++) {
        int startIdx = t * pChunkSize;
        int endIdx = std::min(startIdx + pChunkSize, pNumGlyphs);
        if (startIdx >= endIdx) break;

        pFutures.push_back(std::async(std::launch::async, [&, startIdx, endIdx]() {
            for (int i = startIdx; i < endIdx; i++) {
                const auto& rc = chars[i];
                
                // Shadow 
                if (settings.enableShadow) {
                    int shX = rc.outlineX + settings.shadowOffsetX;
                    int shY = rc.outlineY + settings.shadowOffsetY;
                    int blurLayers = settings.shadowBlur > 0 ? settings.shadowBlur : 1;
                    
                    for (int blur = 0; blur < blurLayers; blur++) {
                        float blurAlpha = settings.shadowColor[3] / (float)(blurLayers);
                        uint8_t shCol[4] = {settings.shadowColor[0], settings.shadowColor[1], settings.shadowColor[2], (uint8_t)blurAlpha};
                        int off = (blur - blurLayers/2);
                        BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, shX + off, shY + off, rc.outline, shCol, nullptr);
                    }
                }
                
                if (settings.strokePosition == 0) { // Outside
                    if (settings.enableStroke && settings.strokeWidth > 0) {
                        BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, rc.outlineX, rc.outlineY, rc.outline, settings.strokeColor, &settings.strokeGradient);
                    }
                    if (settings.enableInnerGlow && settings.innerGlowSize > 0) {
                        BitmapUtils::DrawInnerGlow(result.pixels, result.width, result.height, rc.bodyX, rc.bodyY, rc.body, settings.innerGlowSize, settings.innerGlowChoke, settings.innerGlowColor);
                    }
                    BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, rc.bodyX, rc.bodyY, rc.body, settings.fillColor, &settings.fillGradient);
                } else {
                    if (settings.enableInnerGlow && settings.innerGlowSize > 0) {
                        BitmapUtils::DrawInnerGlow(result.pixels, result.width, result.height, rc.bodyX, rc.bodyY, rc.body, settings.innerGlowSize, settings.innerGlowChoke, settings.innerGlowColor);
                    }
                    BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, rc.bodyX, rc.bodyY, rc.body, settings.fillColor, &settings.fillGradient);
                    if (settings.enableStroke && settings.strokeWidth > 0) {
                        BitmapUtils::BlitGlyph(result.pixels, result.width, result.height, rc.outlineX, rc.outlineY, rc.outline, settings.strokeColor, &settings.strokeGradient, (settings.strokePosition == 2));
                    }
                }

                if (settings.pattern.enabled) {
                    BitmapUtils::ApplyPatternOverlay(result.pixels, result.width, result.height, rc.bodyX, rc.bodyY, rc.body, settings.pattern);
                }

                if (settings.enableBevel && settings.bevelDistance > 0) {
                    BitmapUtils::DrawBevel(result.pixels, result.width, result.height,
                                           rc.bodyX, rc.bodyY, rc.body,
                                           settings.bevelDistance, (float)settings.bevelAngle,
                                           settings.bevelSpread, settings.bevelStrength, settings.bevelType,
                                           settings.bevelHighlightColor, settings.bevelShadowColor);
                }
            }
        }));
    }
    for (auto& f : pFutures) f.wait();
    return result;
}

// End

