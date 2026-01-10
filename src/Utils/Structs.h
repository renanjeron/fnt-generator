#pragma once
#include <string>

struct ReplacedGlyph {
    std::string imagePath;
    int width = 0;
    int height = 0;
    
    // Manual Adjustment
    int xOffset = 0;
    int yOffset = 0;
    int advance = 0; // 0 = Auto (Use width)

    bool applyEffects = false; // "Master" switch (legacy/top-level)
    
    // Granular flags (Default true, but only effective if global effect is on)
    bool applyShadow = true;
    bool applyStroke = true;
    bool applyBevel = true;
    bool applyInnerGlow = true;
    bool applyGradient = true;
    bool applyPattern = true;
    
    // New: Allow using global Fill (Color/Gradient) instead of Image Colors
    bool applyFill = false; 
};
