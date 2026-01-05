#include "FontPreviewUtils.h"
#include <map>
#include <vector>
#include <algorithm>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <GLFW/glfw3.h> // Ensure OpenGL functions are available

namespace Utils {

    static std::map<std::string, GLuint> g_FontPreviewCache;
    static FT_Library g_PreviewLibrary = nullptr;

    void CleanupFontPreviews() {
        for (auto& pair : g_FontPreviewCache) {
            if (pair.second != 0) glDeleteTextures(1, &pair.second);
        }
        g_FontPreviewCache.clear();
        
        if (g_PreviewLibrary) {
            FT_Done_FreeType(g_PreviewLibrary);
            g_PreviewLibrary = nullptr;
        }
    }

    unsigned int GenerateFontPreview(const std::string& fontPath) {
        if (g_FontPreviewCache.find(fontPath) != g_FontPreviewCache.end()) {
            return g_FontPreviewCache[fontPath];
        }

        if (!g_PreviewLibrary) {
            if (FT_Init_FreeType(&g_PreviewLibrary)) return 0;
        }
        
        FT_Face face;
        if (FT_New_Face(g_PreviewLibrary, fontPath.c_str(), 0, &face)) {
            return 0;
        }
        
        FT_Set_Pixel_Sizes(face, 0, 24); // Small preview size
        
        // Render "Abc"
        std::string text = "Abc";
        int totalW = 0;
        int maxH = 0;
        int baseline = 0;
        
        // 1. Calculate dimensions
        for (char c : text) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;
            totalW += (face->glyph->advance.x >> 6);
            maxH = std::max(maxH, (int)face->glyph->bitmap.rows);
            baseline = std::max(baseline, (int)face->glyph->bitmap_top);
        }
        
        if (totalW == 0 || maxH == 0) {
            FT_Done_Face(face);
            return 0;
        }
        
        int height = (int)(maxH * 1.5f); // Some padding
        std::vector<unsigned char> canvas(totalW * height * 4, 0); // RGBA
        
        int currentX = 0;
        int fixedBase = maxH; // Simple baseline assumption
        
        for (char c : text) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;
            
            FT_Bitmap& bmp = face->glyph->bitmap;
            int top = fixedBase - face->glyph->bitmap_top;
            
            for (int y = 0; y < bmp.rows; y++) {
                for (int x = 0; x < bmp.width; x++) {
                    int outX = currentX + x + face->glyph->bitmap_left;
                    int outY = top + y;
                    
                    if (outX >= 0 && outX < totalW && outY >= 0 && outY < height) {
                         unsigned char alpha = bmp.buffer[y * bmp.pitch + x];
                         int idx = (outY * totalW + outX) * 4;
                         // White text
                         canvas[idx + 0] = 255;
                         canvas[idx + 1] = 255;
                         canvas[idx + 2] = 255;
                         canvas[idx + 3] = alpha;
                    }
                }
            }
            currentX += (face->glyph->advance.x >> 6);
        }
        
        FT_Done_Face(face);
        
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, totalW, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, canvas.data());
        
        g_FontPreviewCache[fontPath] = tex;
        return tex;
    }
}
