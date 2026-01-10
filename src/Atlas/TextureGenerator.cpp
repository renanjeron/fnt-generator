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

static FT_Stroker_LineJoin GetFTJoinStyle(int style) {
    if (style == 0) return FT_STROKER_LINEJOIN_BEVEL;
    if (style == 1) return FT_STROKER_LINEJOIN_MITER;
    if (style == 2) return FT_STROKER_LINEJOIN_ROUND;
    return FT_STROKER_LINEJOIN_ROUND;
}

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

        // UPDATE: Scale Replaced Glyphs for SSAA
        // Ensure images are rendered at higher resolution
        std::set<uint32_t> processedReplacements;
        for (uint32_t code : charset) {
            if (processedReplacements.count(code)) continue;
            
            auto it = highSettings.replacedGlyphs.find(code);
            if (it != highSettings.replacedGlyphs.end()) {
                ReplacedGlyph& rep = it->second;
                processedReplacements.insert(code);
                
                int naturalW = 0, naturalH = 0;
                // Only load if we need natural size and image path is set
                if ((rep.width <= 0 || rep.height <= 0) && !rep.imagePath.empty()) {
                    std::vector<uint8_t> tempPx;
                    if (BitmapUtils::GetPatternPixels(rep.imagePath, tempPx, naturalW, naturalH)) {
                         // Cache populated
                    }
                }

                if (rep.width > 0) {
                    rep.width *= factor;
                } else if (naturalW > 0) {
                    rep.width = naturalW * factor;
                }
                
                if (rep.height > 0) {
                    rep.height *= factor;
                } else if (naturalH > 0) {
                    rep.height = naturalH * factor;
                }
                
                // CRITICAL: Scale manual adjustments for SSAA
                rep.xOffset *= factor;
                rep.yOffset *= factor;
                if (rep.advance > 0) {
                    rep.advance *= factor;
                }
            }
        }

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

        // Downsample Kernings
        for(const auto& k : highRes.kernings) {
            AtlasResult::KerningPair nk = k;
            nk.amount /= factor; // Scale down the kerning amount
            result.kernings.push_back(nk);
        }
        
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
        
        // Manual adjustments for replaced glyphs (to be applied during export)
        int manualXOffset = 0;
        int manualYOffset = 0;
        int manualAdvance = 0; // 0 = use body.advance
    };
    
    std::vector<RenderedChar> chars;
    size_t limit = 65536; 
    if (charset.size() > limit) {
        result.hasErrors = true;
        result.errorMessage = "Too many characters selected! Limiting to " + std::to_string(limit) + ".";
    }

    for (uint32_t code : charset) {
        if (chars.size() >= limit) break;
        
        // 2a. Replaced Glyphs Check
        if (settings.replacedGlyphs.count(code)) {
            const auto& rep = settings.replacedGlyphs.at(code);
            std::vector<uint8_t> px; int iw, ih;
            // Valid if we can load it
            if (BitmapUtils::GetPatternPixels(rep.imagePath, px, iw, ih)) {
                RenderedChar rc;
                rc.code = code;
                
                // Use defined size or natural size
                int finalW = (rep.width > 0) ? rep.width : iw;
                int finalH = (rep.height > 0) ? rep.height : ih;
                
                // Safety check: limit maximum size to prevent crashes
                const int MAX_GLYPH_SIZE = 2048;
                if (finalW > MAX_GLYPH_SIZE || finalH > MAX_GLYPH_SIZE) {
                    // Scale down proportionally
                    float scale = std::min((float)MAX_GLYPH_SIZE / finalW, (float)MAX_GLYPH_SIZE / finalH);
                    finalW = (int)(finalW * scale);
                    finalH = (int)(finalH * scale);
                }
                
                // For proper alignment, use the same proportion as capital letters
                int capHeight = settings.fontSize;
                float bearingRatio = 1.0f; // Default: bearingY = height (sits on baseline)
                
                try {
                    GlyphBitmap refGlyph = fontManager.RenderGlyph('H', FT_LOAD_RENDER);
                    if (refGlyph.height > 0 && refGlyph.bearingY > 0) {
                        capHeight = refGlyph.bearingY;
                        bearingRatio = (float)refGlyph.bearingY / (float)refGlyph.height;
                    }
                } catch (...) {
                    // Use defaults
                }
                
                // Apply the same proportion to the icon
                int iconBearingY = (int)(finalH * bearingRatio);
                
                rc.body.width = finalW;
                rc.body.height = finalH;
                rc.body.bearingX = 0;
                rc.body.bearingY = iconBearingY; // Proportional to icon height
                rc.body.advance = finalW; 
                
                
                // Store manual adjustments for export
                rc.manualXOffset = rep.xOffset;
                rc.manualYOffset = rep.yOffset;
                rc.manualAdvance = rep.advance;
                
                if (rep.applyEffects) {
                     // Add padding for effects
                     int p = settings.padding; 
        
 
                     rc.outline = rc.body;
                     
                     // NOTE: Manual adjustments (xOffset, yOffset, advance) are NOT applied here
                     // They are stored in rc.manual* fields and applied during export only
                     
                     if (settings.enableStroke && settings.strokeWidth > 0) {
                         int s = (int)ceil(settings.strokeWidth);
                       
                         rc.packingWidth = finalW + s*2 + 2;
                         rc.packingHeight = finalH + s*2 + 2;
                         rc.penOffsetX = s + 1; 
                         rc.penOffsetY = finalH + s + 1; 
                     } else {
                         rc.packingWidth = finalW + 2;
                         rc.packingHeight = finalH + 2;
                         rc.penOffsetX = 1;
                         rc.penOffsetY = finalH + 1;
                     }
                } else {
                    rc.outline = rc.body; 
                    rc.packingWidth = finalW + 2;
                    rc.packingHeight = finalH + 2;
                    rc.penOffsetX = 1;
                    rc.penOffsetY = finalH + 1;
                }
                
                chars.push_back(rc);
                continue; 
            }
        }

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
            if (!fontManager.GetGlyphStrokeBounds(code, effWidth, GetFTJoinStyle(settings.strokeJoinStyle), settings.strokeMiterLimit, rc.outline, flags)) {
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
        if (settings.atlasWidth <= 0) {
        long long totalArea = 0;
        int maxGlyphW = 0;
        int maxGlyphH = 0;
        
        for(auto& c : chars) {
            // Sanity Check for Glyph Dimensions
            if (c.packingWidth > 4096) c.packingWidth = 4096;
            if (c.packingHeight > 4096) c.packingHeight = 4096;

            int pw = c.packingWidth + settings.padding;
            int ph = c.packingHeight + settings.padding;
            totalArea += (long long)pw * ph;
            if (pw > maxGlyphW) maxGlyphW = pw;
            if (ph > maxGlyphH) maxGlyphH = ph;
        }
        
        // Be more aggressive with initial guess (1.05x). 
        // If it doesn't fit, the retry loop will double it anyway.
        long long safeArea = (long long)(totalArea * 1.05); 
        
        int dim = 128;
        // Prevent infinite loop if safeArea is massive by checking dim < 32768
        while((long long)dim * dim < safeArea && dim < 16384) dim *= 2;
        
        // constraint min dim to largest glyph
        while(dim < maxGlyphW && dim < 16384) dim *= 2;
        while(dim < maxGlyphH && dim < 16384) dim *= 2;

        currentW = std::max(128, dim);

        // Soft Limit logic for MultiPage:
        // If we need a massive texture > 8192, and MultiPage is allowed, 
        // cap it at 4096 or 8192 to prefer pages over giant textures.
        // Starling limits are typically 4096 or 16384 depending on profile. 4096 is safest.
        if (settings.allowMultiPage && currentW > 4096) {
             currentW = 4096;
             // Ensure it still fits single glyph
             if (currentW < maxGlyphW) currentW = maxGlyphW;
             if (currentW < maxGlyphH) currentW = maxGlyphH;
        } else if (!settings.allowMultiPage && currentW > 16384) {
             currentW = 16384; // Hard Limit
        }

        if (settings.atlasHeight <= 0) {
            // FORCE SQUARE for maximum compatibility (Starling, PVRTC, etc.)
            currentH = currentW;
        }
    } else if (settings.atlasHeight <= 0) {
        // Width Fixed, Height Auto.
        // Even here, we should probably prefer POT height, but "Square" is impossible if Width is fixed.
        // We will just grow H as needed POT.
        int maxGlyphH = 0;
        for(const auto& c : chars) {
            int ph = c.packingHeight + settings.padding;
            if (ph > maxGlyphH) maxGlyphH = ph;
        }
        currentH = currentW; // Initial guess? No, start small
        while (currentH < maxGlyphH) currentH *= 2;
    }
    
    int maxPageDim = 16384; // Increased to support full CJK on modern GPUs
    if (settings.atlasWidth > 0) maxPageDim = settings.atlasWidth; 

    struct PageState {
        int id;
        int width;
        int height;
        int currentX;
        int currentY;
        int rowHeight;
        int maxUsedY; // To trim height at the end
    };
    
    std::vector<PageState> pageStates;

    bool packingComplete = false;
    
    while (!packingComplete) {
        // Reset state for retry
        pageStates.clear();
        pageStates.push_back({0, currentW, currentH, settings.padding, settings.padding, 0, settings.padding});
        
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
                         // Update MaxUsedY
                         int glyphBottom = rc.atlasY + rc.packingHeight + settings.padding;
                         if (glyphBottom > page.maxUsedY) page.maxUsedY = glyphBottom;

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
                            
                            int glyphBottom = rc.atlasY + rc.packingHeight + settings.padding;
                            if (glyphBottom > page.maxUsedY) page.maxUsedY = glyphBottom;

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
                // If Multi-Page is allowed, we cap growth at 8192 (Soft Limit) to prefer new pages over massive textures.
                int growLimit = (settings.allowMultiPage) ? 8192 : maxPageDim;
                if (growLimit > 16384) growLimit = 16384; // Absolute hard limit

                bool canGrowW = (settings.atlasWidth <= 0 && currentW < growLimit);
                bool canGrowH = (settings.atlasHeight <= 0 && currentH < growLimit);
                
                // Correction: If user wants 16k single page (No MultiPage), growLimit is 16384. 
                // If MultiPage enabled, growLimit is 8192.
                
                if (canGrowW || canGrowH) {
                    // If BOTH are Auto, we must grow BOTH to keep it square.
                    bool bothAuto = (settings.atlasWidth <= 0 && settings.atlasHeight <= 0);

                    if (bothAuto && canGrowW && canGrowH) {
                        currentW *= 2;
                        currentH *= 2;
                    } else {
                        // Keep roughly square logic for mixed cases (one fixed, one auto)
                        // If W <= H, double W.
                        if (currentW <= currentH && canGrowW) currentW *= 2;
                        else if (canGrowH) currentH *= 2;
                        else if (canGrowW) currentW *= 2; // Fallback
                    }
                    
                    allFit = false; 
                    break; // BREAK INNER LOOP to restart packing with new size
                } else {
                    // Cannot grow (Max reached or Fixed Size).
                    // Add new Page? Only if allowed.
                    if (settings.allowMultiPage) {
                        // Create new page
                        int nextId = (int)pageStates.size();
                        // Use current sizes for new pages (assuming uniform page size)
                        pageStates.push_back({nextId, currentW, currentH, settings.padding, settings.padding, 0, settings.padding});
                        
                        // Retry fitting this char in the new page
                        auto& page = pageStates.back();
                        
                         // Check fit logic again on the fresh page
                         if (page.currentX + rc.packingWidth + settings.padding <= page.width && 
                             page.currentY + rc.packingHeight + settings.padding <= page.height) {
                                rc.atlasX = page.currentX;
                                rc.atlasY = page.currentY;
                                rc.pageIndex = page.id;
                                page.currentX += rc.packingWidth + settings.padding;
                                if (rc.packingHeight > page.rowHeight) page.rowHeight = rc.packingHeight;
                                int glyphBottom = rc.atlasY + rc.packingHeight + settings.padding;
                                if (glyphBottom > page.maxUsedY) page.maxUsedY = glyphBottom;
                                fitted = true;
                         } else {
                             rc.skip = true;
                         }
                    } else {
                        rc.skip = true;
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
        
        // TRIM HEIGHT
        
        // For maximum compatibility (Starling, engines, etc.), we maintain the Power-of-Two 
        // square shape calculated during packing and do NOT trim the height.
        ap.height = ps.height;

        try {
            ap.pixels.assign(ap.width * ap.height * 4, 0); // Clear to 0
        } catch (const std::bad_alloc&) {
            result.hasErrors = true;
            result.errorMessage = "Memory allocation failed for Page " + std::to_string(ap.id) + " (" + std::to_string(ap.width) + "x" + std::to_string(ap.height) + ")";
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
    // 5. Populate Result
    result.skippedGlyphs = 0;
    for(const auto& rc: chars) {
        if(rc.skip) result.skippedGlyphs++;
    }

    for (const auto& rc : chars) {
        if (rc.skip) continue;
        GlyphPlacement gp;
        gp.charCode = rc.code;
        gp.x = rc.atlasX;
        gp.y = rc.atlasY;
        gp.width = rc.packingWidth;
        gp.height = rc.packingHeight;
        
        // Use manual advance if specified, otherwise use body.advance
        int baseAdvance = (rc.manualAdvance > 0) ? rc.manualAdvance : rc.body.advance;
        gp.advance = baseAdvance + settings.globalXAdvance;

        // Apply offsets
        if (settings.replacedGlyphs.count(rc.code)) {
            // Replaced glyph: use 1:1 mapping from UI to XML (plus global adjustments)
            gp.xoffset = rc.manualXOffset + settings.globalXOffset;
            gp.yoffset = rc.manualYOffset + settings.globalYOffset;
        } else {
            // Normal font glyph: relative to baseline
            gp.xoffset = -rc.penOffsetX + settings.globalXOffset; 
            gp.yoffset = (fontManager.GetAscender() - rc.penOffsetY) + settings.globalYOffset;
        }
        
        gp.pageIndex = rc.pageIndex;
        result.glyphs.push_back(gp);
    }

    // 5b. Extract Kerning Pairs (N^2 check on included chars)
    // Only if we have more than 1 character AND enabled in settings
    if (settings.enableKerning && chars.size() > 1) {
        if (!fontManager.HasKerning()) {
            // Font doesn't support legacy kerning, skip expensive checks.
            // Using a simpler message to avoid spamming warnings for fonts that just don't have it (like many CJK fonts).
        } else {
             std::cout << "[INFO] Font has legacy 'kern' mechanism available." << std::endl;
        
             // Optimization: Pre-filter candidates.
             // Only check kerning for characters < 0x2E80 (Latin, Greek, Cyrillic, etc.)
             // This reduces N from ~20k (CJK) to < 500, making the N^2 check trivial.
             std::vector<uint32_t> candidates;
             candidates.reserve(chars.size());
             for(const auto& rc : chars) {
                 if (!rc.skip && rc.code < 0x2E80) {
                     candidates.push_back(rc.code);
                 }
             }

             if (!candidates.empty()) {
                 std::cout << "[INFO] Checking kerning for " << candidates.size() << " legacy candidates." << std::endl;
                 int kerningCount = 0;

                 for (size_t i = 0; i < candidates.size(); ++i) {
                     for (size_t j = 0; j < candidates.size(); ++j) {
                         uint32_t first = candidates[i];
                         uint32_t second = candidates[j];
                         
                         int amount = fontManager.GetKerning(first, second);
                         if (amount != 0) {
                             AtlasResult::KerningPair pair;
                             pair.first = first;
                             pair.second = second;
                             pair.amount = amount;
                             result.kernings.push_back(pair);
                             kerningCount++;
                         }
                     }
                 }
             }
        }
    }

    // 6. Parallel Composition (Restored with localized locking)
    int numGlyphs = (int)chars.size();
    int numHardwareThreads = std::thread::hardware_concurrency();
    int numThreads = std::max(1, std::min(numHardwareThreads, (numGlyphs / 10) + 1)); // At least 10 chars per thread

    int chunkSize = (numGlyphs + numThreads - 1) / numThreads;
    std::vector<std::future<void>> futures;

    for (int t = 0; t < numThreads; t++) {
        int startIdx = t * chunkSize;
        int endIdx = std::min(startIdx + chunkSize, numGlyphs);
        if (startIdx >= endIdx) break;

        futures.push_back(std::async(std::launch::async, [&, startIdx, endIdx]() {
            try {
                for (int i = startIdx; i < endIdx; i++) {
                    const auto& rcCoords = chars[i]; // Coords and skip info only
                    if (rcCoords.skip) continue;
                    
                    if (rcCoords.pageIndex < 0 || rcCoords.pageIndex >= (int)result.pages.size()) {
                        // This should never happen if logic is correct, but safety first
                        continue;
                    }

                    AtlasPage& targetPage = result.pages[rcCoords.pageIndex];

                    GlyphBitmap body;
                    GlyphBitmap outline;
                    bool replaced = false;

                    // 0. Replaced Glyph Render
                    if (settings.replacedGlyphs.count(rcCoords.code)) {
                        const auto& rep = settings.replacedGlyphs.at(rcCoords.code);
                        std::vector<uint8_t> px; int iw, ih;
                        if (BitmapUtils::GetPatternPixels(rep.imagePath, px, iw, ih)) {
                            // Resize if needed (Consolidated Logic)
                            int finalW = (rep.width > 0) ? rep.width : iw;
                            int finalH = (rep.height > 0) ? rep.height : ih;
                            
                            if (finalW != iw || finalH != ih) {
                                std::vector<uint8_t> finalPx;
                                BitmapUtils::ResizeImage(px, iw, ih, finalPx, finalW, finalH);
                                px = std::move(finalPx);
                                iw = finalW;
                                ih = finalH;
                            }
                            
                            int penX = rcCoords.atlasX + rcCoords.penOffsetX; 
                            int penY = rcCoords.atlasY + rcCoords.penOffsetY;

                            // If effects are enabled, we do NOT continue; we let the pipeline run.
                            // But we need to prep the 'body' to match the image alpha
                            if (rep.applyEffects) {
                                body.width = iw;
                                body.height = ih;
                                body.bearingX = 0;
                                body.bearingY = ih; // Should match measured
                                body.advance = iw;
                                body.buffer.resize(iw * ih);
                                
                                // Extract Alpha
                                for(int i=0; i<iw*ih; i++) {
                                    body.buffer[i] = px[i*4 + 3];
                                }
                                
                                // Set Outline eq Body OR Dilated Body
                                if (settings.enableStroke && settings.strokeWidth > 0) {
                                     // Dilate Image Alpha
                                     float sw = settings.strokeWidth;
                                     if (settings.strokePosition == 1) sw *= 0.5f; // Center
                                     
                                     int outW, outH;
                                     outline.buffer = BitmapUtils::DilateAlpha(body.buffer, body.width, body.height, sw, outW, outH);
                                     outline.width = outW;
                                     outline.height = outH;
                                     // We dilated by 'strokeWidth' (radius).
                                     // The center should align.
                                     // New bearingX = oldBearingX - radius
                                     // New bearingY = oldBearingY + radius
                                     int r = (int)ceil(sw);
                                     outline.bearingX = body.bearingX - r;
                                     outline.bearingY = body.bearingY + r; 
                                     outline.advance = body.advance; // Doesn't really matter for rendering
                                } else {
                                     outline = body; 
                                }
                                
                                replaced = true;
                            } else {
                                // Direct Image Blit (Legacy/No-Effect Mode)
                                int blitY = penY - ih; 
                                BitmapUtils::BlitImage(targetPage.pixels, targetPage.width, targetPage.height,
                                                     penX, blitY, px, iw, ih);
                                continue;
                            }
                        }
                    }

                    // ----- RENDER (Locked internally by FontManager) -----
                    FT_Int32 flags = FT_LOAD_RENDER;
                    if (settings.hintingMode == 0) flags |= (FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT); 
                    else if (settings.hintingMode == 1) flags |= FT_LOAD_FORCE_AUTOHINT;
                    else flags |= FT_LOAD_TARGET_NORMAL; 

                    if (!replaced) {
                        // Regular Rendering
                        if (!settings.replacedGlyphs.count(rcCoords.code)) {
                             body = fontManager.RenderGlyph(rcCoords.code, flags);
                             if (settings.enableStroke && settings.strokeWidth > 0) {
                                 float effWidth = settings.strokeWidth;
                                 if (settings.strokePosition == 1) effWidth *= 0.5f; 
                                 outline = fontManager.RenderGlyphStroke(rcCoords.code, effWidth, GetFTJoinStyle(settings.strokeJoinStyle), settings.strokeMiterLimit, flags);
                             } else {
                                 outline = body; 
                             }
                        }
                    }
                    // Else: body/outline already set above if applyEffects=true

                    // ----- PROCESSING & COMPOSITION (Unlocked / Parallel) -----
                    int penX = rcCoords.atlasX + rcCoords.penOffsetX; 
                    int penY = rcCoords.atlasY + rcCoords.penOffsetY;
                    
                    const ReplacedGlyph* pRep = nullptr;
                    if (replaced && settings.replacedGlyphs.count(rcCoords.code)) {
                        pRep = &settings.replacedGlyphs.at(rcCoords.code); 
                    }

                    // A. Shadow
                    if (settings.enableShadow && (!pRep || pRep->applyShadow)) {
                        int blur = settings.shadowBlur;
                        int drawRefX = penX + outline.bearingX + settings.shadowOffsetX;
                        int drawRefY = penY - outline.bearingY + settings.shadowOffsetY;

                        if (blur > 0) {
                            int sw = outline.width + blur*2;
                            int sh = outline.height + blur*2;
                            if (sw > 0 && sh > 0 && sw < 4096 && sh < 4096) { // Sanity check on buffer size
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



                    // B. Stroke (Background part)
                    if (settings.enableStroke && settings.strokeWidth > 0 && settings.strokePosition != 2 && (!pRep || pRep->applyStroke)) { 
                        if (settings.strokePosition == 0) { 
                            BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                                penX + outline.bearingX, penY - outline.bearingY, outline, 
                                                settings.strokeColor, &settings.strokeGradient, false);
                        }
                    }
                    
                    // C. Body
                    int bodyX = penX + body.bearingX;
                    int bodyY = penY - body.bearingY;
                    
                    bool specialBodyDraw = false;
                    if (settings.replacedGlyphs.count(rcCoords.code)) {
                        const auto& rep = settings.replacedGlyphs.at(rcCoords.code);
                         if (rep.applyEffects && !rep.applyFill) {
                            // Re-get image for Blitting (Could cache this? but threads...)
                            std::vector<uint8_t> px; int iw, ih;
                            if (BitmapUtils::GetPatternPixels(rep.imagePath, px, iw, ih)) {
                                int finalW = (rep.width > 0) ? rep.width : iw;
                                int finalH = (rep.height > 0) ? rep.height : ih;
                                if (finalW != iw || finalH != ih) {
                                    std::vector<uint8_t> finalPx;
                                    BitmapUtils::ResizeImage(px, iw, ih, finalPx, finalW, finalH);
                                    px = finalPx; iw = finalW; ih = finalH;
                                }
                                BitmapUtils::BlitImage(targetPage.pixels, targetPage.width, targetPage.height,
                                                     bodyX, bodyY, px, iw, ih);
                                specialBodyDraw = true;
                            }
                         }
                    }

                    if (!specialBodyDraw) {
                        BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                            bodyX, bodyY, body, 
                                            settings.fillColor, &settings.fillGradient);
                    }

                    if (settings.enableInnerGlow && settings.innerGlowSize > 0 && (!pRep || pRep->applyInnerGlow)) {
                            BitmapUtils::DrawInnerGlow(targetPage.pixels, targetPage.width, targetPage.height, 
                                                    bodyX, bodyY, body, settings.innerGlowSize, settings.innerGlowChoke, settings.innerGlowColor,
                                                    settings.innerGlowBlendMode);
                    }

                    if (settings.pattern.enabled && (!pRep || pRep->applyPattern)) {
                        BitmapUtils::ApplyPatternOverlay(targetPage.pixels, targetPage.width, targetPage.height,
                                                        bodyX, bodyY, body,
                                                        settings.pattern);
                    }
                    
                    // D. Stroke (Foreground part)
                    if (settings.enableStroke && settings.strokeWidth > 0 && (!pRep || pRep->applyStroke)) {
                        if (settings.strokePosition == 1) { 
                            bool mask = (settings.strokePosition == 2);
                            BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                                penX + outline.bearingX, penY - outline.bearingY, outline, 
                                                settings.strokeColor, &settings.strokeGradient, mask);
                        } else if (settings.strokePosition == 2) { 
                            BitmapUtils::BlitGlyph(targetPage.pixels, targetPage.width, targetPage.height, 
                                                penX + outline.bearingX, penY - outline.bearingY, outline, 
                                                settings.strokeColor, &settings.strokeGradient, true);
                        }
                    }

                    if (settings.enableBevel && settings.bevelDistance > 0 && (!pRep || pRep->applyBevel)) {
                        BitmapUtils::DrawBevel(targetPage.pixels, targetPage.width, targetPage.height,
                                            penX + body.bearingX, penY - body.bearingY,
                                            body,
                                            settings.bevelDistance, (float)settings.bevelAngle, 
                                            settings.bevelSpread, settings.bevelStrength, settings.bevelType,
                                            settings.bevelHighlightColor, settings.bevelShadowColor);
                    }
                }
            } catch (const std::exception& e) {
                // Catch any standard exception to prevent abort()
                std::cerr << "[ERROR] Exception in render thread: " << e.what() << std::endl;
            } catch (...) {
                 std::cerr << "[ERROR] Unknown exception in render thread." << std::endl;
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
        
        // UPDATE: Scale Replaced Glyphs for SSAA (Preview Text)
        std::set<uint32_t> processedReplacements;
        std::vector<uint32_t> uniqueCharset = Utils::DecodeUtf8(text.c_str()); // Decode strictly for looking up reps
        
        for (uint32_t code : uniqueCharset) {
            if (processedReplacements.count(code)) continue;
            
            auto it = highSettings.replacedGlyphs.find(code);
            if (it != highSettings.replacedGlyphs.end()) {
                ReplacedGlyph& rep = it->second;
                processedReplacements.insert(code);
                
                int naturalW = 0, naturalH = 0;
                if ((rep.width <= 0 || rep.height <= 0) && !rep.imagePath.empty()) {
                    std::vector<uint8_t> tempPx;
                    if (BitmapUtils::GetPatternPixels(rep.imagePath, tempPx, naturalW, naturalH)) {
                         // Cache
                    }
                }

                if (rep.width > 0) {
                    rep.width *= factor;
                } else if (naturalW > 0) {
                    rep.width = naturalW * factor;
                }
                
                if (rep.height > 0) {
                    rep.height *= factor;
                } else if (naturalH > 0) {
                    rep.height = naturalH * factor;
                }
                
                rep.xOffset *= factor;
                rep.yOffset *= factor;
                if (rep.advance > 0) rep.advance *= factor;
            }
        }

        AtlasResult highRes = GenerateTextPreview(fontManager, text, highSettings);
        
        if (highRes.hasErrors) return highRes;
        
        AtlasResult result;
        result.hasErrors = highRes.hasErrors;
        result.errorMessage = highRes.errorMessage;
        
        result.fontSize = highRes.fontSize / factor;
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

            try {
                page.pixels.resize(page.width * page.height * 4);
            } catch (const std::length_error& le) {
                 result.hasErrors = true;
                 result.errorMessage = "Memory allocation failed (length_error) during SSAA downsample.";
                 return result;
            } catch (const std::bad_alloc& ba) {
                 result.hasErrors = true;
                 result.errorMessage = "Memory allocation failed (bad_alloc) during SSAA downsample.";
                 return result;
            }

            for(int y=0; y<page.height; y++) {
                for(int x=0; x<page.width; x++) {
                    int baseDest = (y*page.width + x)*4;
                    int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
                    int totalAlpha = 0;
                    // Safe Loop Bounds
                    for (int sy = 0; sy < factor; sy++) {
                        for (int sx = 0; sx < factor; sx++) {
                            int srcX = x * factor + sx;
                            int srcY = y * factor + sy;
                            
                            if (srcX < highPage.width && srcY < highPage.height) {
                                int baseSrc = (srcY * highPage.width + srcX) * 4;
                                uint8_t a = highPage.pixels[baseSrc + 3];
                                sumR += highPage.pixels[baseSrc + 0] * a;
                                sumG += highPage.pixels[baseSrc + 1] * a;
                                sumB += highPage.pixels[baseSrc + 2] * a;
                                totalAlpha += a;
                                sumA += a;
                            }
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
    
    std::vector<uint32_t> charset = Utils::DecodeUtf8(text.c_str());
    for (uint32_t code : charset) {
        RenderedChar rc;
        rc.code = code;
        
        bool replaced = false;
        if (settings.replacedGlyphs.count(code)) {
            const auto& rep = settings.replacedGlyphs.at(code);
            std::vector<uint8_t> px; int iw, ih;
            if (BitmapUtils::GetPatternPixels(rep.imagePath, px, iw, ih)) {
                 int finalW = (rep.width > 0) ? rep.width : iw;
                 int finalH = (rep.height > 0) ? rep.height : ih;
                 
                 // Safety check: limit maximum size to prevent crashes
                 // Increased limit to support SSAA 4x on reasonably large icons (e.g. 1024 -> 4096)
                 const int MAX_GLYPH_SIZE = 8192;
                 if (finalW > MAX_GLYPH_SIZE || finalH > MAX_GLYPH_SIZE) {
                     // Scale down proportionally
                     float scale = std::min((float)MAX_GLYPH_SIZE / finalW, (float)MAX_GLYPH_SIZE / finalH);
                     finalW = (int)(finalW * scale);
                     finalH = (int)(finalH * scale);
                 }
                 
                 // Resize if needed
                 if (finalW != iw || finalH != ih) {
                     std::vector<uint8_t> finalPx;
                     BitmapUtils::ResizeImage(px, iw, ih, finalPx, finalW, finalH);
                     px = std::move(finalPx);
                     iw = finalW;
                     ih = finalH;
                 }
                 
                 // For proper alignment, calculate bearingY as a proportion of the icon height
                 // The proportion should match typical capital letters (bearingY ≈ height)
                 // This ensures consistent alignment regardless of SSAA factor
                 
                 // Default: icon sits on baseline (bearingY = height)
                 float bearingRatio = 1.0f;
                 
                 // Try to get actual ratio from font, but use original fontSize context
                 try {
                     // Render at base size to get true proportion
                     GlyphBitmap refGlyph = fontManager.RenderGlyph('H', FT_LOAD_RENDER);
                     if (refGlyph.height > 0 && refGlyph.bearingY > 0) {
                         bearingRatio = (float)refGlyph.bearingY / (float)refGlyph.height;
                     }
                 } catch (...) {
                     // Use default
                 }
                 
                 // Apply proportion to icon height
                 // Since user specifies absolute pixel size, we use that directly
                 int iconBearingY = (int)(finalH * bearingRatio);
                 
                 rc.body.width = finalW;
                 rc.body.height = finalH;
                 rc.body.bearingX = 0;
                 rc.body.bearingY = iconBearingY;
                 rc.body.advance = finalW;
                 
                 // Extract alpha channel to body.buffer for effects rendering
                 if (rep.applyEffects) {
                     rc.body.buffer.resize(finalW * finalH);
                     for (int i = 0; i < finalW * finalH; i++) {
                         rc.body.buffer[i] = px[i * 4 + 3]; // Extract alpha
                     }
                     
                     // Fix: Generate Outline if Stroke is enabled
                     if (settings.enableStroke && settings.strokeWidth > 0 && rep.applyStroke) {
                        // We need to dilate the alpha to create the stroke outline
                        // Using same logic as GenerateAtlas
                        float sw = settings.strokeWidth;
                        if (settings.strokePosition == 1) sw *= 0.5f; // Center

                        int outW, outH;
                        rc.outline.buffer = BitmapUtils::DilateAlpha(rc.body.buffer, finalW, finalH, sw, outW, outH);
                        rc.outline.width = outW;
                        rc.outline.height = outH;
                        
                        // Adjust bearing for stroke expansion
                        int r = (int)ceil(sw);
                        rc.outline.bearingX = rc.body.bearingX - r;
                        rc.outline.bearingY = rc.body.bearingY + r; 
                     } else {
                        rc.outline = rc.body;
                     }
                 } else {
                    rc.outline = rc.body;
                 }
                 
                 rc.width = finalW;
                 rc.height = finalH;
                 
                 rc.bearingX = 0 + rep.xOffset;
                 rc.bearingY = iconBearingY - rep.yOffset;
                 
                 rc.body.bearingX = rc.bearingX;
                 rc.body.bearingY = rc.bearingY;
                 
                 if (rep.advance > 0) rc.advance = rep.advance;
                 else rc.advance = finalW;
                 
                 replaced = true;
            }
        }

        if (!replaced) {
            FT_Int32 flags = FT_LOAD_RENDER;
            if (settings.hintingMode == 0) flags |= (FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT); 
            else if (settings.hintingMode == 1) flags |= FT_LOAD_FORCE_AUTOHINT;
            else flags |= FT_LOAD_TARGET_NORMAL; 
            
            rc.body = fontManager.RenderGlyph(rc.code, flags);
            
            if (settings.enableStroke && settings.strokeWidth > 0) {
                rc.outline = fontManager.RenderGlyphStroke(rc.code, settings.strokeWidth, GetFTJoinStyle(settings.strokeJoinStyle), settings.strokeMiterLimit, flags);
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
        }
        
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
    
    // Validate Dimensions
    if (totalWidth <= 0 || totalHeight <= 0) {
        // Fallback to avoid invalid allocation
        totalWidth = 64; 
        totalHeight = 64; 
    }
    // Hard Limit for Preview (e.g. 32k) to prevent ridiculous allocations
    if (totalWidth > 32768) totalWidth = 32768;
    if (totalHeight > 32768) totalHeight = 32768;

    // Single Page Result
    AtlasPage page;
    page.id = 0;
    page.width = totalWidth;
    page.height = totalHeight;
    result.atlasWidth = totalWidth;
    result.atlasHeight = totalHeight;

    try {
        page.pixels.assign(page.width * page.height * 4, 0);
    } catch (const std::length_error& e) {
        result.hasErrors = true;
        result.errorMessage = "Preview too large (length_error): " + std::to_string(totalWidth) + "x" + std::to_string(totalHeight);
        std::cerr << "[ERROR] " << result.errorMessage << std::endl;
        return result;
    } catch (const std::bad_alloc& e) {
        result.hasErrors = true;
        result.errorMessage = "Memory allocation failed for preview: " + std::to_string(totalWidth) + "x" + std::to_string(totalHeight);
        std::cerr << "[ERROR] " << result.errorMessage << std::endl;
        return result;
    }

    int baselineInTex = -minY + settings.padding;
    int startPenXInTex = -minX + settings.padding;
    currentPenX = startPenXInTex;

    for (auto& rc : chars) {
        int drawX = currentPenX + rc.bearingX + settings.globalXOffset;
        int drawY = baselineInTex - rc.bearingY + settings.globalYOffset;

        rc.bodyX = currentPenX + rc.body.bearingX + settings.globalXOffset;
        rc.bodyY = baselineInTex - rc.body.bearingY + settings.globalYOffset;

        // Force Outline to follow Body with correct radius offset
        // This fixes misalignment for replaced glyphs where offsets are manually applied
        if (settings.enableStroke && settings.strokeWidth > 0) {
             float sw = settings.strokeWidth;
             if (settings.strokePosition == 1) sw *= 0.5f; // Center
             int r = (int)ceil(sw);
             rc.outlineX = rc.bodyX - r;
             rc.outlineY = rc.bodyY - r;
        } else {
             rc.outlineX = rc.bodyX;
             rc.outlineY = rc.bodyY;
        }
        rc.currentPenX = currentPenX;

        currentPenX += rc.advance + settings.globalXAdvance;
    }
    
    // We already calculated layout, now render.
    // Sequential is fine for preview, or reuse parallel logic. 
    // Reuse parallel logic for consistence/speed.
    
    // We only have one page. Access it directly.
    unsigned char* rawPixels = page.pixels.data();
    int pw = page.width;
    int ph = page.height;
    
    int pNumGlyphs = (int)chars.size();

    // Sequential Rendering to avoid Race Conditions (Glyphs overlap)
    for (int i = 0; i < pNumGlyphs; i++) {
        const auto& rc = chars[i];
        
        if (settings.replacedGlyphs.count(rc.code)) {
             // Replaced glyphs are handled in DrawBody and Shadow/Stroke blocks below.
             // We no longer render them early here to allow effects to work correctly.
        }
        
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
            bool drawn = false;
                // Check replaced
                if (settings.replacedGlyphs.count(rc.code)) {
                    const auto& rep = settings.replacedGlyphs.at(rc.code);
                    if (!rep.applyEffects || !rep.applyFill) {
                            std::vector<uint8_t> px; int iw, ih;
                            if (BitmapUtils::GetPatternPixels(rep.imagePath, px, iw, ih)) {
                            int finalW = (rep.width > 0) ? rep.width : iw;
                            int finalH = (rep.height > 0) ? rep.height : ih;
                            if (finalW != iw || finalH != ih) {
                                std::vector<uint8_t> finalPx;
                                BitmapUtils::ResizeImage(px, iw, ih, finalPx, finalW, finalH);
                                px = std::move(finalPx); iw = finalW; ih = finalH;
                            }
                            BitmapUtils::BlitImage(page.pixels, pw, ph, rc.bodyX, rc.bodyY, px, iw, ih);
                            drawn = true;
                            }
                    }
                }
            
            if (!drawn) {
                BitmapUtils::BlitGlyph(page.pixels, pw, ph, rc.bodyX, rc.bodyY, rc.body, settings.fillColor, &settings.fillGradient);
                if (settings.enableInnerGlow && settings.innerGlowSize > 0) {
                    BitmapUtils::DrawInnerGlow(page.pixels, pw, ph, rc.bodyX, rc.bodyY, rc.body, settings.innerGlowSize, settings.innerGlowChoke, settings.innerGlowColor, settings.innerGlowBlendMode);
                }
                if (settings.pattern.enabled) {
                    BitmapUtils::ApplyPatternOverlay(page.pixels, pw, ph, rc.bodyX, rc.bodyY, rc.body, settings.pattern);
                }
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
    
    result.pages.push_back(page);
    result.fontName = fontManager.GetFontName();
    return result;
}
