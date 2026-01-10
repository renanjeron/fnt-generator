#pragma once

#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstdint>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <GLFW/glfw3.h>
#include <imgui_gradient/imgui_gradient.hpp>
#include "Utils/UnicodeBlocks.h"
#include "Utils/Structs.h" // Ensure this exists or find where structs are defined
#include "Font/FontManager.h" // For FontManager
#include "Utils/PlatformUtils.h" // For Utils::FontInfo
#include "Utils/PatLoader.h" // For PatImage

// State Variables
extern std::vector<Utils::FontInfo> g_SystemFonts;
extern FontManager g_FontManager;
extern int g_SelectedFontIndex;
extern char g_InputText[1024];
extern int g_FontSize;
extern int g_Padding;

// Atlas Size
extern int g_AtlasWidth;
extern int g_AtlasHeight;
extern bool g_AllowMultiPage;
extern float g_AtlasCheckerOpacity; // Opacity of the checkerboard background 0.0 - 1.0

// Fill
extern float g_FillColor[4];
extern bool g_EnableGradient;
extern ImGG::GradientWidget g_FillGradientWidget;
extern int g_FillGradientType;
extern float g_FillGradientAngle;

// Stroke
extern bool g_EnableStroke;
extern bool g_EnableStrokeGradient;
extern float g_StrokeWidth;
extern float g_StrokeColor[4];
extern ImGG::GradientWidget g_StrokeGradientWidget;
extern int g_StrokeGradientType;
extern float g_StrokeGradientAngle;
extern int g_StrokePosition;
extern int g_StrokeJoinStyle;
extern float g_StrokeMiterLimit;

// Shadow
extern bool g_EnableShadow;
extern int g_ShadowOffsetX;
extern int g_ShadowOffsetY;
extern int g_ShadowBlur;
extern float g_ShadowColor[4];

// Bevel
extern bool g_EnableBevel;
extern int g_BevelAngle;
extern float g_BevelDistance;
extern float g_BevelSpread;
extern float g_BevelStrength;
extern int g_BevelType;
extern float g_BevelHighlightColor[4];
extern float g_BevelShadowColor[4];

// Inner Glow
extern bool g_EnableInnerGlow;
extern float g_InnerGlowSize;
extern float g_InnerGlowChoke;
extern float g_InnerGlowColor[4];
extern int g_InnerGlowBlendMode;

// Pattern
extern bool g_EnablePattern;

// Font Preview (Globals relevant to style loading)
extern std::string g_PatternPath;
extern std::vector<PatImage> g_LoadedPatterns;
extern std::vector<GLuint> g_PatternThumbnails; // If GLuint is used, need GL headers
extern float g_PatternOpacity;
extern float g_PatternAngle;
extern float g_PatternScale;
extern int g_PatternBlendMode;
extern int g_PatternMappingMode;
extern std::string g_OriginalPatternPath;
extern int g_SelectedPatternIndex;
extern bool g_ShowMissingPatternDialog;
extern std::string g_MissingPatternPath;
extern GLuint g_PatternPreviewTexture;

extern int g_SelectedFallbackFontIndex;

// File Export
extern char g_ExportFilename[128];
extern std::string g_ExportPath;
extern int g_ExportFormat;

// Charset
extern bool g_UseCustomGlyphs;
extern std::string g_CustomGlyphsText;
extern std::map<uint32_t, ReplacedGlyph> g_ReplacedGlyphs;
extern std::vector<UnicodeBlock> g_UnicodeBlocks;

// Atlas Interaction
extern std::set<uint32_t> g_ExcludedGlyphs; // If needed
extern int g_SSAAFactor;
extern int g_HintingMode;

// Char Adjustments
extern int g_GlobalXAdvance;
extern int g_GlobalXOffset;
extern int g_GlobalYOffset;

// Globals for fingerprinting
struct GradientFingerprint {
    struct Mark {
        float pos;
        float r, g, b, a;
    };
    std::vector<Mark> marks;
    bool operator!=(const GradientFingerprint& o) const {
        if (marks.size() != o.marks.size()) return true;
        for (size_t i = 0; i < marks.size(); i++) {
            if (marks[i].pos != o.marks[i].pos || marks[i].r != o.marks[i].r || 
                marks[i].g != o.marks[i].g || marks[i].b != o.marks[i].b || marks[i].a != o.marks[i].a) return true;
        }
        return false;
    }
};

extern GradientFingerprint g_LastFillFp;
extern GradientFingerprint g_LastStrokeFp;

GradientFingerprint GetFingerprint(const ImGG::GradientWidget& w);

// Functions needed by StyleUtils
void UpdatePreview(const char* text, bool fullAtlas = true);
