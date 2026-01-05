#include "TextureGenerator.h"
#include "Utils/BitmapUtils.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdint>
#include <future>
#include <thread>
#include "Utils/StringUtils.h"
#include <set>

// Wrapper
AtlasResult TextureGenerator::GenerateAtlas(FontManager& fontManager, const std::string& text, const AtlasSettings& settings) {
    // Decode UTF-8 correctly
    std::vector<uint32_t> charset = Utils::DecodeUtf8(text.c_str());
    
    // Remove duplicates but PRESERVE ORDER
    std::vector<uint32_t> uniqueCharset;
    std::set<uint32_t> seen;
    uniqueCharset.reserve(charset.size());
    for(uint32_t c : charset) {
        if(seen.find(c) == seen.end()) {
            seen.insert(c);
            uniqueCharset.push_back(c);
        }
    }
    return GenerateAtlas(fontManager, uniqueCharset, settings);
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
        result.hasErrors = highRes.hasErrors;
        result.errorMessage = highRes.errorMessage;
        result.skippedGlyphs = highRes.skippedGlyphs;
        
        // Downsample Metrics
        result.fontSize = highRes.fontSize / factor;
        result.lineHeight = highRes.lineHeight / factor;
        result.lineHeight = highRes.lineHeight / factor;
        result.base = highRes.base / factor;
        result.fontName = highRes.fontName;

        // Downsample Pages
        for (const auto& highPage : highRes.pages) {
            AtlasPage page;
            page.id = highPage.id;
            page.width = highPage.width / factor;
            page.height = highPage.height / factor;
            
            try {
                page.pixels.resize(page.width * page.height * 4);
            } catch (const std::bad_alloc&) {
                 result.hasErrors = true;
                 result.errorMessage = "Out of memory during downsampling!";
                 return result;
            }

            for(int y=0; y<page.height; y++) {
                for(int x=0; x<page.width; x++) {
                    int baseDest = (y*page.width + x)*4;
                    
                    int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                    int totalAlpha = 0;

                    for (int sy = 0; sy < factor; sy++) {
                        for (int sx = 0; sx < factor; sx++) {
                            int srcX = x * factor + sx;
                            int srcY = y * factor + sy;
                            int baseSrc = (srcY * highPage.width + srcX) * 4;
                            
                            uint8_t a = highPage.pixels[baseSrc + 3];
                            sumR += highPage.pixels[baseSrc + 0] * a;
                            sumG += highPage.pixels[baseSrc + 1] * a;
                            sumB += highPage.pixels[baseSrc + 2] * a;
                            totalAlpha += a;
                            sumA += a;
                        }
                    }
                    
                    if (totalAlpha == 0) {
                        page.pixels[baseDest + 0] = 0;
                        page.pixels[baseDest + 1] = 0;
                        page.pixels[baseDest + 2] = 0;
                        page.pixels[baseDest + 3] = 0;
                    } else {
                        page.pixels[baseDest + 0] = (uint8_t)(sumR / totalAlpha);
                        page.pixels[baseDest + 1] = (uint8_t)(sumG / totalAlpha);
                        page.pixels[baseDest + 2] = (uint8_t)(sumB / totalAlpha);
                        page.pixels[baseDest + 3] = (uint8_t)(sumA / (factor * factor));
                    }
                }
            }
            result.pages.push_back(page);
        }
        
        // Downsample Glyphs
        for(const auto& g : highRes.glyphs) {
            GlyphPlacement ng = g;
            ng.x /= factor; ng.y /= factor; ng.width /= factor; ng.height /= factor;
            ng.xoffset /= factor; ng.yoffset /= factor; ng.advance /= factor;
            result.glyphs.push_back(ng);
        }
        
        // Set info for export (using first page dims or scaled setting)
        // Set info for export (using first page dims or scaled setting)
        if (!result.pages.empty()) {
            if (settings.atlasWidth > 0) result.atlasWidth = settings.atlasWidth;
            else result.atlasWidth = result.pages[0].width;

            if (settings.atlasHeight > 0) result.atlasHeight = settings.atlasHeight;
            else result.atlasHeight = result.pages[0].height;
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
        GlyphBitmap body; // Will hold metrics ONLY (buffer empty)
        GlyphBitmap outline; // Will hold metrics ONLY (buffer empty)
        int packingWidth, packingHeight;
        int penOffsetX, penOffsetY;
        int atlasX = 0, atlasY = 0;
        int pageIndex = 0;
        bool skip = false;
    };
    
    std::vector<RenderedChar> chars;
    size_t limit = 65536; 
    if (charset.size() > limit) {
        result.hasErrors = true;
        result.errorMessage = "Too many characters selected! Limiting to " + std::to_string(limit) + ".";
    }

    // 2. Measure (Phase 1: Metrics Only)
    for (uint32_t code : charset) {
        if (chars.size() >= limit) break;
        if (!fontManager.HasGlyph(code)) continue;

        RenderedChar rc;
        rc.code = code;
        
        // Determine flags based on hinting mode
        FT_Int32 flags = FT_LOAD_RENDER;
        if (settings.hintingMode == 0) flags |= (FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT); 
        else if (settings.hintingMode == 1) flags |= FT_LOAD_FORCE_AUTOHINT;
        else flags |= FT_LOAD_TARGET_NORMAL;

        // Get METRICS only (Deferred Rendering)
        if (!fontManager.GetGlyphBounds(code, rc.body, flags)) continue;
        
        if (rc.body.width == 0 && rc.body.height == 0 && rc.body.advance == 0 && code != 32 && code != 160) continue;

        if (settings.enableStroke && settings.strokeWidth > 0) {
            float effWidth = settings.strokeWidth;
            if (settings.strokePosition == 1) effWidth *= 0.5f; // Center
            if (!fontManager.GetGlyphStrokeBounds(code, effWidth, rc.outline, flags)) {
                rc.outline = rc.body; // Fallback
            }
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
        
        rc.packingWidth = (maxX - minX) + 6; 
        rc.packingHeight = (maxY - minY) + 6;
        rc.penOffsetX = -minX + 3;
        rc.penOffsetY = -minY + 3;
        
        chars.push_back(rc);
    }
    
    if (!settings.keepInputOrder) {
        std::sort(chars.begin(), chars.end(), [](const RenderedChar& a, const RenderedChar& b) {
            // Sort by height descending for better packing
            return a.packingHeight > b.packingHeight;
        });
    }

    // 4. Pack Multi-Page with Retry Logic for Auto-Size
    
    // Initial size calculation
    int currentW = (settings.atlasWidth > 0) ? settings.atlasWidth : 256;
    int currentH = (settings.atlasHeight > 0) ? settings.atlasHeight : 256;
    
    // Heuristic for Auto: Calculate area and start with square
    // Heuristic for Auto: Calculate area and start with square
    if (settings.atlasWidth <= 0) {
        long long totalArea = 0;
        int maxGlyphW = 0;
        int maxGlyphH = 0;
        
        for(const auto& c : chars) {
            int pw = c.packingWidth + settings.padding;
            int ph = c.packingHeight + settings.padding;
            totalArea += (long long)pw * ph;
            if (pw > maxGlyphW) maxGlyphW = pw;
            if (ph > maxGlyphH) maxGlyphH = ph;
        }
        
        // Start larger to account for packing loss (1.5x is usually safe, 2.0x for ordered packing)
        long long safeArea = (long long)(totalArea * 1.5); 
        
        int dim = 128;
        while((long long)dim * dim < safeArea) dim *= 2;
        
        // constraint min dim to largest glyph
        while(dim < maxGlyphW) dim *= 2;
        while(dim < maxGlyphH) dim *= 2;

        currentW = std::max(128, dim);
        
        if (settings.atlasHeight <= 0) {
            // Check if rectangular (Landscape) fits
            // Try W = dim, H = dim/2
            // Must also respect maxGlyphH
            int halfH = currentW / 2;
            if (halfH >= maxGlyphH && (long long)currentW * halfH >= safeArea) {
                currentH = halfH;
            } else {
                currentH = currentW;
            }
        }
    } else if (settings.atlasHeight <= 0) {
        // Width Fixed, Height Auto.
        // Ensure starting H is at least enough for largest glyph
        int maxGlyphH = 0;
        for(const auto& c : chars) {
            int ph = c.packingHeight + settings.padding;
            if (ph > maxGlyphH) maxGlyphH = ph;
        }
        while(currentH < maxGlyphH) currentH *= 2;
    }
    
    int maxPageDim = 4096; // Safe limit
    if (settings.atlasWidth > 0) maxPageDim = settings.atlasWidth; 

    struct PageState {
        int id;
        int width;
        int height;
        int currentX;
        int currentY;
        int rowHeight;
    };
    
    std::vector<PageState> pageStates;

    bool packingComplete = false;
    
    while (!packingComplete) {
        // Reset state for retry
        pageStates.clear();
        pageStates.push_back({0, currentW, currentH, settings.padding, settings.padding, 0});
        
        bool allFit = true;
        
        for (auto& rc : chars) {
            bool fitted = false;
            
            // Try fit in existing pages
            for (auto& page : pageStates) {
                // Check if fits in current row
                if (page.currentX + rc.packingWidth + settings.padding <= page.width) {
                     if (page.currentY + rc.packingHeight + settings.padding <= page.height) {
                         // Fits
                         rc.atlasX = page.currentX;
                         rc.atlasY = page.currentY;
                         rc.pageIndex = page.id;
                         page.currentX += rc.packingWidth + settings.padding;
                         if (rc.packingHeight > page.rowHeight) page.rowHeight = rc.packingHeight;
                         fitted = true;
                         break;
                     }
                } else {
                    // New row?
                    if (page.currentY + page.rowHeight + settings.padding + rc.packingHeight + settings.padding <= page.height) {
                        page.currentX = settings.padding;
                        page.currentY += page.rowHeight + settings.padding;
                        page.rowHeight = 0;
                        
                        if (page.currentX + rc.packingWidth + settings.padding <= page.width) {
                            rc.atlasX = page.currentX;
                            rc.atlasY = page.currentY;
                            rc.pageIndex = page.id;
                            page.currentX += rc.packingWidth + settings.padding;
                            if (rc.packingHeight > page.rowHeight) page.rowHeight = rc.packingHeight;
                            fitted = true;
                            break;
                        }
                    }
                }
            }
            
            if (!fitted) {
                // Failed to fit.
                // If Auto Size is enabled for Width OR Height, we can try to GROW.
                // BUT only if we haven't hit MaxDim.
                bool canGrowW = (settings.atlasWidth <= 0 && currentW < maxPageDim);
                bool canGrowH = (settings.atlasHeight <= 0 && currentH < maxPageDim);
                
                if (canGrowW || canGrowH) {
                    // Strategy: Keep roughly square, or double?
                    // If W <= H, double W.
                    if (currentW <= currentH && canGrowW) currentW *= 2;
                    else if (canGrowH) currentH *= 2;
                    else if (canGrowW) currentW *= 2; // Fallback
                    
                    allFit = false; 
                    break; // BREAK INNER LOOP to restart packing with new size
                } else {
                    // Cannot grow (Max reached or Fixed Size).
                    // Add new Page? Only if allowed.
                    if (settings.allowMultiPage) {
                        // Create new page
                        int nextId = (int)pageStates.size();
                        // Use current sizes for new pages (assuming uniform page size)
                        pageStates.push_back({nextId, currentW, currentH, settings.padding, settings.padding, 0});
                        
                        // Retry fitting this char in the new page
                        auto& page = pageStates.back();
                         if (page.currentX + rc.packingWidth + settings.padding <= page.width && 
                             page.currentY + rc.packingHeight + settings.padding <= page.height) {
                                rc.atlasX = page.currentX;
                                rc.atlasY = page.currentY;
                                rc.pageIndex = page.id;
                                page.currentX += rc.packingWidth + settings.padding;
                                if (rc.packingHeight > page.rowHeight) page.rowHeight = rc.packingHeight;
                                fitted = true;
                         } else {
                             rc.skip = true;
                             // We continue
                         }
                    } else {
                        rc.skip = true;
                        // We continue
                    }
                }
            }
        }
        
        if (allFit) {
            packingComplete = true;
        }
    }

    // Initialize Result Pages
    for (const auto& ps : pageStates) {
        AtlasPage ap;
        ap.id = ps.id;
        ap.width = ps.width;
        ap.height = ps.height;
        // Trim height?
        // If Auto Height was enabled, we might want to trim the page to used height?
        // Let's trim to nearest POT or just used height if allowed. BMFont usually outputs POT.
        // ps.height is already POT.
        
        try {
            ap.pixels.assign(ap.width * ap.height * 4, 0); // Clear to 0
        } catch (const std::bad_alloc&) {
            result.hasErrors = true;
            result.errorMessage = "Memory allocation failed for Page " + std::to_string(ap.id);
            return result;
        }
        result.pages.push_back(ap);
    }

    // Fill Metrics
    result.fontSize = settings.fontSize;
    result.lineHeight = fontManager.GetLineHeight();
    result.base = fontManager.GetAscender();
    result.base = fontManager.GetAscender();
    
    // Set Logical Atlas Width/Height
    // If settings had a fixed size, use that. Otherwise use the generated page size.
    if (settings.atlasWidth > 0) result.atlasWidth = settings.atlasWidth;
    else result.atlasWidth = result.pages.empty() ? 0 : result.pages[0].width;
    
    if (settings.atlasHeight > 0) result.atlasHeight = settings.atlasHeight;
    else result.atlasHeight = result.pages.empty() ? 0 : result.pages[0].height;

    // Add Glyphs to Result
    for (const auto& rc : chars) {
        if (rc.skip) continue;
        GlyphPlacement gp;
        gp.charCode = rc.code;
        gp.x = rc.atlasX;
        gp.y = rc.atlasY;
        gp.width = rc.packingWidth;
        gp.height = rc.packingHeight;
        gp.xoffset = -rc.penOffsetX + settings.globalXOffset; 
        gp.yoffset = (fontManager.GetAscender() - rc.penOffsetY) + settings.globalYOffset;
        gp.advance = rc.body.advance + settings.globalXAdvance;
        gp.pageIndex = rc.pageIndex;
        result.glyphs.push_back(gp);
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
                const auto& rcCoords = chars[i]; // Coords and skip info only
                if (rcCoords.skip) continue;
                
                // Get Page Buffer
                // WARNING: Vector might realloc if we push back?
                // result.pages is sized and fixed now.
                // Thread safety: Each glyph writes to distinct region.
                // Overlap: Padding prevents overlap. Shadow/Stroke?
                // Padding includes room for packingWidth/Height which includes effects.
                // So safe.
                
                // Need pointer to page pixels
                // Vector access is thread safe for reading, and writing DIFFERENT elements.
                // We need to be careful not to resize 'result.pages' (we are not).
                // However, we can't easily access 'result.pages[rcCoords.pageIndex]' efficiently if we don't capture 'result'.
                // 'result' is captured by reference [&].
                
                // We need to look up the correct page.
                // But wait, iterating inside the lambda we need checks.
                // It's safe.
                
                AtlasPage& targetPage = result.pages[rcCoords.pageIndex];

                // ----- DEFERRED RENDERING START -----
                FT_Int32 flags = FT_LOAD_RENDER;
                if (settings.hintingMode == 0) flags |= (FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT); 
                else if (settings.hintingMode == 1) flags |= FT_LOAD_FORCE_AUTOHINT;
                else flags |= FT_LOAD_TARGET_NORMAL;

                GlyphBitmap body = fontManager.RenderGlyph(rcCoords.code, flags);
                GlyphBitmap outline;
                if (settings.enableStroke && settings.strokeWidth > 0) {
                     float effWidth = settings.strokeWidth;
                     if (settings.strokePosition == 1) effWidth *= 0.5f; 
                     outline = fontManager.RenderGlyphStroke(rcCoords.code, effWidth, flags);
                } else {
                     outline = body; 
                }
                // ----- DEFERRED RENDERING END -----

                int penX = rcCoords.atlasX + rcCoords.penOffsetX; 
                int penY = rcCoords.atlasY + rcCoords.penOffsetY;
                
                // A. Shadow
                if (settings.enableShadow) {
                     int blur = settings.shadowBlur;
                     int drawRefX = penX + outline.bearingX + settings.shadowOffsetX;
                     int drawRefY = penY - outline.bearingY + settings.shadowOffsetY;

                     if (blur > 0) {
                         int sw = outline.width + blur*2;
                         int sh = outline.height + blur*2;
                         if (sw > 0 && sh > 0) {
                            std::vector<uint8_t> shadowBuf(sw * sh, 0);
                            for(int y=0; y<outline.height; y++) {
                                int srcOffset = y * outline.width;
                                int dstOffset = (y+blur)*sw + blur;
                                if(srcOffset < (int)outline.buffer.size() && dstOffset + outline.width <= (int)shadowBuf.size())
                                    memcpy(shadowBuf.data() + dstOffset, outline.buffer.data() + srcOffset, outline.width);
                            }
                            std::vector<uint8_t> blurred = BitmapUtils::ApplyGaussianBlur(shadowBuf, sw, sh, (float)blur);
                            GlyphBitmap shRes; shRes.buffer = blurred; shRes.width = sw; shRes.height = sh;
                            
                            BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                                   drawRefX - blur, drawRefY - blur, shRes, 
                                                   settings.shadowColor, nullptr);
                         }
                     } else {
                         BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                                drawRefX, drawRefY, outline, 
                                                settings.shadowColor, nullptr);
                     }
                }

                auto DrawStroke = [&]() {
                    if (settings.enableStroke && settings.strokeWidth > 0) {
                         bool mask = (settings.strokePosition == 2); // Inside
                         BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                               penX + outline.bearingX, penY - outline.bearingY, outline, 
                                               settings.strokeColor, &settings.strokeGradient, mask);
                    }
                };

                auto DrawBody = [&]() {
                    int bodyX = penX + body.bearingX;
                    int bodyY = penY - body.bearingY;
                    
                    BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                           bodyX, bodyY, body, 
                                           settings.fillColor, &settings.fillGradient);

                    if (settings.enableInnerGlow && settings.innerGlowSize > 0) {
                         BitmapUtils::DrawInnerGlow(targetPage.pixels, targetPage.width, targetPage.height, 
                                                    bodyX, bodyY, body, settings.innerGlowSize, settings.innerGlowChoke, settings.innerGlowColor,
                                                    settings.innerGlowBlendMode);
                    }

                    if (settings.pattern.enabled) {
                        BitmapUtils::ApplyPatternOverlay(targetPage.pixels, targetPage.width, targetPage.height,
                                                         bodyX, bodyY, body,
                                                         settings.pattern);
                    }
                };

                if (settings.strokePosition == 0) {
                    DrawStroke();
                    DrawBody();
                } else {
                    DrawBody();
                    DrawStroke();
                }

                if (settings.enableBevel && settings.bevelDistance > 0) {
                    BitmapUtils::DrawBevel(targetPage.pixels, targetPage.width, targetPage.height,
                                           penX + body.bearingX, penY - body.bearingY,
                                           body,
                                           settings.bevelDistance, (float)settings.bevelAngle, 
                                           settings.bevelSpread, settings.bevelStrength, settings.bevelType,
                                           settings.bevelHighlightColor, settings.bevelShadowColor);
                }
            }
        }));
    }
    for (auto& f : futures) f.wait();
    
    // Check skipped
    if (result.skippedGlyphs > 0) {
        result.hasErrors = true;
        result.errorMessage = "Some glyphs were skipped.";
    }
    
    result.fontName = fontManager.GetFontName();
    return result;
}

// Legacy Preview
AtlasResult TextureGenerator::GenerateTextPreview(FontManager& fontManager, const std::string& text, const AtlasSettings& settings) {
    // Basic single-page implementation for preview
    // We will just assume it fits in one page for preview purposes.
    // If it doesn't, we should probably scale it or error.
    
    // Super Sampling Check
    int factor = settings.superSamplingFactor;
    if (factor <= 0) factor = 1;

    if (factor > 1) {
        // ... (Similar recursion)
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
        result.hasErrors = highRes.hasErrors;
        result.errorMessage = highRes.errorMessage;
        
        result.fontSize = highRes.fontSize / factor;
        result.lineHeight = highRes.lineHeight / factor;
        result.lineHeight = highRes.lineHeight / factor;
        result.base = highRes.base / factor;
        result.fontName = highRes.fontName;

        // Downsample Page 0
        if (!highRes.pages.empty()) {
            AtlasPage page;
            page.id = 0;
            const auto& highPage = highRes.pages[0];
            
            page.width = highPage.width / factor;
            page.height = highPage.height / factor;
            result.atlasWidth = page.width;
            result.atlasHeight = page.height;

            page.pixels.resize(page.width * page.height * 4);
            
            for(int y=0; y<page.height; y++) {
                for(int x=0; x<page.width; x++) {
                    int baseDest = (y*page.width + x)*4;
                    int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                    int totalAlpha = 0;
                    for (int sy = 0; sy < factor; sy++) {
                        for (int sx = 0; sx < factor; sx++) {
                            int srcX = x * factor + sx;
                            int srcY = y * factor + sy;
                            int baseSrc = (srcY * highPage.width + srcX) * 4;
                            uint8_t a = highPage.pixels[baseSrc + 3];
                            sumR += highPage.pixels[baseSrc + 0] * a;
                            sumG += highPage.pixels[baseSrc + 1] * a;
                            sumB += highPage.pixels[baseSrc + 2] * a;
                            totalAlpha += a;
                            sumA += a;
                        }
                    }
                    if (totalAlpha == 0) {
                        page.pixels[baseDest+0]=0; page.pixels[baseDest+1]=0; page.pixels[baseDest+2]=0; page.pixels[baseDest+3]=0;
                    } else {
                        page.pixels[baseDest+0] = (uint8_t)(sumR/totalAlpha);
                        page.pixels[baseDest+1] = (uint8_t)(sumG/totalAlpha);
                        page.pixels[baseDest+2] = (uint8_t)(sumB/totalAlpha);
                        page.pixels[baseDest+3] = (uint8_t)(sumA/(factor*factor));
                    }
                }
            }
            result.pages.push_back(page);
        }
        return result;
    }
    
    AtlasResult result;
    if (!fontManager.IsLoaded()) return result;
    fontManager.SetSize(settings.fontSize);

    struct RenderedChar {
        uint32_t code;
        GlyphBitmap body;
        GlyphBitmap outline; 
        int width, height; 
        int bearingY, bearingX;
        long advance;
        int outlineX, outlineY;
        int bodyX, bodyY;
        int currentPenX;
    };
    std::vector<RenderedChar> chars;
    
    // Bounds Measure
    int minX = 100000, minY = 100000, maxX = -100000, maxY = -100000;
    int currentPenX = 0; 
    
    for (char c : text) {
        RenderedChar rc;
        rc.code = (uint32_t)c;
        
        FT_Int32 flags = FT_LOAD_RENDER;
        if (settings.hintingMode == 0) flags |= (FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT); 
        else if (settings.hintingMode == 1) flags |= FT_LOAD_FORCE_AUTOHINT;
        else flags |= FT_LOAD_TARGET_NORMAL; 
        
        rc.body = fontManager.RenderGlyph(rc.code, flags);
        
        if (settings.enableStroke && settings.strokeWidth > 0) {
            rc.outline = fontManager.RenderGlyphStroke(rc.code, settings.strokeWidth, flags);
            rc.width = rc.outline.width;
            rc.height = rc.outline.height;
            rc.bearingY = rc.outline.bearingY; 
            rc.bearingX = rc.outline.bearingX; 
        } else {
            rc.outline = rc.body;
            rc.width = rc.body.width;
            rc.height = rc.body.height;
            rc.bearingY = rc.body.bearingY; 
            rc.bearingX = rc.body.bearingX;
        }
        rc.advance = rc.body.advance;
        
        int refLeft = currentPenX + rc.bearingX;
        int refTop = -rc.bearingY;
        int refRight = refLeft + rc.width;
        int refBottom = refTop + rc.height;
        int sLeft = refLeft + settings.globalXOffset;
        int sTop = refTop + settings.globalYOffset;
        int sRight = sLeft + rc.width; 
        int sBottom = sTop + rc.height;

        if (settings.enableShadow) {
            int shLeft = sLeft + settings.shadowOffsetX;
            int shTop = sTop + settings.shadowOffsetY;
            int shRight = shLeft + rc.width; 
            int shBottom = shTop + rc.height;
            if (shLeft < minX) minX = shLeft;
            if (shTop < minY) minY = shTop;
            if (shRight > maxX) maxX = shRight;
            if (shBottom > maxY) maxY = shBottom;
        }

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
    
    // Single Page Result
    AtlasPage page;
    page.id = 0;
    page.width = totalWidth;
    page.height = totalHeight;
    result.atlasWidth = totalWidth;
    result.atlasHeight = totalHeight;

    try {
        page.pixels.assign(page.width * page.height * 4, 0);
    } catch (const std::bad_alloc&) {
        result.hasErrors = true;
        result.errorMessage = "Memory allocation failed for preview! Font size too large.";
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
    
    // We already calculated layout, now render.
    // Sequential is fine for preview, or reuse parallel logic. 
    // Reuse parallel logic for consistence/speed.
    
    int pNumGlyphs = (int)chars.size();
    int pNumThreads = std::min((int)std::thread::hardware_concurrency(), (pNumGlyphs / 5) + 1);
    if (pNumThreads < 1) pNumThreads = 1;
    int pChunkSize = (pNumGlyphs + pNumThreads - 1) / pNumThreads;
    std::vector<std::future<void>> pFutures;
    
    // We only have one page. Access it directly.
    unsigned char* rawPixels = page.pixels.data();
    int pw = page.width;
    int ph = page.height;

    for (int t = 0; t < pNumThreads; t++) {
        int startIdx = t * pChunkSize;
        int endIdx = std::min(startIdx + pChunkSize, pNumGlyphs);
        if (startIdx >= endIdx) break;

        pFutures.push_back(std::async(std::launch::async, [&, startIdx, endIdx]() {
            for (int i = startIdx; i < endIdx; i++) {
                const auto& rc = chars[i];
                
                if (settings.enableShadow) {
                    int shX = rc.outlineX + settings.shadowOffsetX;
                    int shY = rc.outlineY + settings.shadowOffsetY;
                    int blurLayers = settings.shadowBlur > 0 ? settings.shadowBlur : 1;
                    for (int blur = 0; blur < blurLayers; blur++) {
                        float blurAlpha = settings.shadowColor[3] / (float)(blurLayers);
                        uint8_t shCol[4] = {settings.shadowColor[0], settings.shadowColor[1], settings.shadowColor[2], (uint8_t)blurAlpha};
                        int off = (blur - blurLayers/2);
                        BitmapUtils::BlitGlyph(page.pixels, pw, ph, shX + off, shY + off, rc.outline, shCol, nullptr);
                    }
                }
                
                auto DrawStroke = [&]() {
                     if (settings.enableStroke && settings.strokeWidth > 0) {
                         BitmapUtils::BlitGlyph(page.pixels, pw, ph, rc.outlineX, rc.outlineY, rc.outline, settings.strokeColor, &settings.strokeGradient);
                     }
                };
                auto DrawBody = [&]() {
                    BitmapUtils::BlitGlyph(page.pixels, pw, ph, rc.bodyX, rc.bodyY, rc.body, settings.fillColor, &settings.fillGradient);
                    if (settings.enableInnerGlow && settings.innerGlowSize > 0) {
                        BitmapUtils::DrawInnerGlow(page.pixels, pw, ph, rc.bodyX, rc.bodyY, rc.body, settings.innerGlowSize, settings.innerGlowChoke, settings.innerGlowColor, settings.innerGlowBlendMode);
                    }
                    if (settings.pattern.enabled) {
                        BitmapUtils::ApplyPatternOverlay(page.pixels, pw, ph, rc.bodyX, rc.bodyY, rc.body, settings.pattern);
                    }
                };

                if (settings.strokePosition == 0) { 
                    DrawStroke();
                    DrawBody();
                } else {
                    DrawBody();
                    DrawStroke();
                     if (settings.strokePosition == 2 && settings.enableStroke && settings.strokeWidth > 0) {
                        BitmapUtils::BlitGlyph(page.pixels, pw, ph, rc.outlineX, rc.outlineY, rc.outline, settings.strokeColor, &settings.strokeGradient, true);
                    }
                }

                if (settings.enableBevel && settings.bevelDistance > 0) {
                    BitmapUtils::DrawBevel(page.pixels, pw, ph,
                                           rc.bodyX, rc.bodyY, rc.body,
                                           settings.bevelDistance, (float)settings.bevelAngle,
                                           settings.bevelSpread, settings.bevelStrength, settings.bevelType,
                                           settings.bevelHighlightColor, settings.bevelShadowColor);
                }
            }
        }));
    }
    for (auto& f : pFutures) f.wait();
    
    result.pages.push_back(page);
    result.fontName = fontManager.GetFontName();
    return result;
}
