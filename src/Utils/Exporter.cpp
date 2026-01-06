#include "Exporter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// --- Helper Functions for Exporters ---

static void WriteXML(const AtlasResult& atlas, const std::string& folder, const std::string& filename, const std::vector<std::string>& pageFilenames) {
    std::filesystem::path path = std::filesystem::path(folder) / filename;
    std::ofstream o(path);
    if (!o.is_open()) return;



    o << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" << std::endl;
    o << "<!--Created using Fnt Generator-->" << std::endl;
    o << "<font>" << std::endl;
    std::string faceName = atlas.fontName.empty() ? "FntGenerator" : atlas.fontName;
    o << "    <info face=\"" << faceName << "\" size=\"" << atlas.fontSize << "\" bold=\"0\" italic=\"0\" charset=\"\" unicode=\"1\" stretchH=\"100\" smooth=\"1\" aa=\"1\" padding=\"0,0,0,0\" spacing=\"0,0\"/>" << std::endl;
    o << "    <common lineHeight=\"" << atlas.lineHeight << "\" base=\"" << atlas.base << "\" scaleW=\"" << atlas.atlasWidth << "\" scaleH=\"" << atlas.atlasHeight << "\" pages=\"" << std::max(1, (int)atlas.pages.size()) << "\" packed=\"0\"/>" << std::endl;
    o << "    <pages>" << std::endl;
    
    for (size_t i = 0; i < pageFilenames.size(); ++i) {
        o << "        <page id=\"" << i << "\" file=\"" << pageFilenames[i] << "\"/>" << std::endl;
    }
    
    o << "    </pages>" << std::endl;
    o << "    <chars count=\"" << atlas.glyphs.size() << "\">" << std::endl;

    for (const auto& g : atlas.glyphs) {
        o << "        <char id=\"" << g.charCode << "\" "
          << "x=\"" << g.x << "\" "
          << "y=\"" << g.y << "\" "
          << "width=\"" << g.width << "\" "
          << "height=\"" << g.height << "\" "
          << "xoffset=\"" << g.xoffset << "\" "
          << "yoffset=\"" << g.yoffset << "\" "
          << "xadvance=\"" << g.advance << "\" "
          << "page=\"" << g.pageIndex << "\" chnl=\"15\" letter=\"";

        if (g.charCode == '"') o << "&quot;";
        else if (g.charCode == '&') o << "&amp;";
        else if (g.charCode == '<') o << "&lt;";
        else if (g.charCode == '>') o << "&gt;";
        else {
             if (g.charCode >= 32 && g.charCode <= 126) o << (char)g.charCode;
        }
        o << "\"/>" << std::endl;
    }
    
    o << "    </chars>" << std::endl;
    o << "    <kernings count=\"" << atlas.kernings.size() << "\">" << std::endl;
    for (const auto& k : atlas.kernings) {
        o << "        <kerning first=\"" << k.first << "\" second=\"" << k.second << "\" amount=\"" << k.amount << "\"/>" << std::endl;
    }
    o << "    </kernings>" << std::endl;
    o << "</font>" << std::endl;
}

static void WriteText(const AtlasResult& atlas, const std::string& folder, const std::string& filename, const std::vector<std::string>& pageFilenames) {
    std::filesystem::path path = std::filesystem::path(folder) / filename;
    std::ofstream o(path);
    if (!o.is_open()) return;

    // Info
    std::string faceName = atlas.fontName.empty() ? "FntGenerator" : atlas.fontName;
    o << "info face=\"" << faceName << "\" size=" << atlas.fontSize << " bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=0,0" << std::endl;
    // Common
    o << "common lineHeight=" << atlas.lineHeight << " base=" << atlas.base << " scaleW=" << atlas.atlasWidth << " scaleH=" << atlas.atlasHeight << " pages=" << std::max(1, (int)atlas.pages.size()) << " packed=0 alphaChnl=1 redChnl=0 greenChnl=0 blueChnl=0" << std::endl;
    // Pages
    for (size_t i = 0; i < pageFilenames.size(); ++i) {
        o << "page id=" << i << " file=\"" << pageFilenames[i] << "\"" << std::endl;
    }
    // Chars
    o << "chars count=" << atlas.glyphs.size() << std::endl;

    for (const auto& g : atlas.glyphs) {
        o << "char id=" << g.charCode
          << " x=" << g.x
          << " y=" << g.y
          << " width=" << g.width
          << " height=" << g.height
          << " xoffset=" << g.xoffset
          << " yoffset=" << g.yoffset
          << " xadvance=" << g.advance
          << " page=" << g.pageIndex << " chnl=15" << std::endl;
    }
    // Kernings
    // Kernings
    o << "kernings count=" << atlas.kernings.size() << std::endl;
    for (const auto& k : atlas.kernings) {
        o << "kerning first=" << k.first << " second=" << k.second << " amount=" << k.amount << std::endl;
    }
}

static void WriteByte(std::ofstream& o, uint8_t v) { o.write((const char*)&v, 1); }
static void WriteShort(std::ofstream& o, int16_t v) { o.write((const char*)&v, 2); } // Assumes LE
static void WriteUShort(std::ofstream& o, uint16_t v) { o.write((const char*)&v, 2); }
static void WriteInt(std::ofstream& o, int32_t v) { o.write((const char*)&v, 4); }
static void WriteUInt(std::ofstream& o, uint32_t v) { o.write((const char*)&v, 4); }

static void WriteBinary(const AtlasResult& atlas, const std::string& folder, const std::string& filename, const std::vector<std::string>& pageFilenames) {
    std::filesystem::path path = std::filesystem::path(folder) / filename;
    std::ofstream o(path, std::ios::binary);
    if (!o.is_open()) return;

    // Header BMF3
    WriteByte(o, 66); // B
    WriteByte(o, 77); // M
    WriteByte(o, 70); // F
    WriteByte(o, 3);  // Version 3

    // Block 1: Info
    std::string fontName = atlas.fontName.empty() ? "FntGenerator" : atlas.fontName;
    int32_t blockSize1 = 14 + (int32_t)fontName.length() + 1;
    
    WriteByte(o, 1); // Block Type 1
    WriteInt(o, blockSize1);
    
    WriteShort(o, (int16_t)atlas.fontSize);
    WriteByte(o, 0); // bitField (0 for no bold/italic etc)
    WriteByte(o, 0); // charSet
    WriteUShort(o, 100); // stretchH
    WriteByte(o, 1); // aa
    WriteByte(o, 0); WriteByte(o, 0); WriteByte(o, 0); WriteByte(o, 0); // Padding Up, Right, Down, Left
    WriteByte(o, 0); WriteByte(o, 0); // Spacing H, V
    WriteByte(o, 0); // outline
    o.write(fontName.c_str(), fontName.length() + 1);

    // Block 2: Common
    // 15 bytes total
    int32_t blockSize2 = 15;
    WriteByte(o, 2); // Block Type 2
    WriteInt(o, blockSize2);
    
    WriteUShort(o, (uint16_t)atlas.lineHeight);
    WriteUShort(o, (uint16_t)atlas.base);
    WriteUShort(o, (uint16_t)atlas.atlasWidth);
    WriteUShort(o, (uint16_t)atlas.atlasHeight);
    WriteUShort(o, (uint16_t)std::max(1, (int)atlas.pages.size())); // pages
    WriteByte(o, 0); // bitField (packed=0)
    WriteByte(o, 0); // alpha
    WriteByte(o, 0); // red
    WriteByte(o, 0); // green
    WriteByte(o, 0); // blue

    // Block 3: Pages
    // p strings
    // Calculate total size
    int32_t totalStringLen = 0;
    for(const auto& s : pageFilenames) totalStringLen += (int32_t)s.length() + 1;

    int32_t blockSize3 = totalStringLen;
    WriteByte(o, 3); // Block Type 3
    WriteInt(o, blockSize3);
    for(const auto& s : pageFilenames) {
        o.write(s.c_str(), s.length() + 1); // null term included
    }

    // Block 4: Chars
    // 20 bytes per char
    int32_t blockSize4 = (int32_t)atlas.glyphs.size() * 20;
    WriteByte(o, 4); // Block Type 4
    WriteInt(o, blockSize4);
    
    for(const auto& g : atlas.glyphs) {
        WriteUInt(o, g.charCode);
        WriteUShort(o, (uint16_t)g.x);
        WriteUShort(o, (uint16_t)g.y);
        WriteUShort(o, (uint16_t)g.width);
        WriteUShort(o, (uint16_t)g.height);
        WriteShort(o, (int16_t)g.xoffset);
        WriteShort(o, (int16_t)g.yoffset);
        WriteShort(o, (int16_t)g.advance);
        WriteByte(o, (uint8_t)g.pageIndex); // page
        WriteByte(o, 15); // chnl (all)
    }

    // Block 5: Kerning
    // 10 bytes per pair (Type 5)
    // Structure: 4 bytes first, 4 bytes second, 2 bytes amount
    int32_t numKernings = (int32_t)atlas.kernings.size();
    if(numKernings > 0) {
        int32_t blockSize5 = numKernings * 10;
        WriteByte(o, 5); // Block Type 5
        WriteInt(o, blockSize5);
        
        for(const auto& k : atlas.kernings) {
            WriteUInt(o, k.first);
            WriteUInt(o, k.second);
            WriteShort(o, (int16_t)k.amount);
        }
    }
}

bool Exporter::ExportAtlasToDisk(const AtlasResult& atlas, const std::string& destinationFolder, const std::string& fileNameBase, int format, const std::string& extension, const uint8_t* backgroundColor) {
    std::filesystem::path folder(destinationFolder);
    if (!std::filesystem::exists(folder)) {
        return false;
    }

    std::vector<std::string> pageFilenames;
    int pageCount = std::max(1, (int)atlas.pages.size());

    // 1. Save Images (PNG)
    for (int i = 0; i < pageCount; ++i) {
        std::string pngName;
        if (pageCount == 1) {
             pngName = fileNameBase + ".png"; // Legacy/Simple
        } else {
             pngName = fileNameBase + "_" + std::to_string(i) + ".png";
        }
        pageFilenames.push_back(pngName);
        
        std::filesystem::path pngPath = folder / pngName;
        
        if (i < (int)atlas.pages.size()) {
            const auto& p = atlas.pages[i];
            
            // Determine export dimensions (crop overflow if needed)
            int exportW = p.width;
            int exportH = p.height;
            
            if (i == 0 && atlas.atlasWidth > 0 && atlas.atlasWidth < p.width) {
                exportW = atlas.atlasWidth;
            }
            if (i == 0 && atlas.atlasHeight > 0 && atlas.atlasHeight < p.height) {
                exportH = atlas.atlasHeight;
            }

            // Compositing logic
            const uint8_t* finalPixels = p.pixels.data();
            std::vector<uint8_t> compositeBuffer;

            if (backgroundColor && backgroundColor[3] > 0) {
                compositeBuffer.resize(p.pixels.size());
                float bgR = backgroundColor[0] / 255.0f;
                float bgG = backgroundColor[1] / 255.0f;
                float bgB = backgroundColor[2] / 255.0f;
                float bgA = backgroundColor[3] / 255.0f;

                for (size_t j = 0; j < p.pixels.size(); j += 4) {
                    float srcR = p.pixels[j + 0] / 255.0f;
                    float srcG = p.pixels[j + 1] / 255.0f;
                    float srcB = p.pixels[j + 2] / 255.0f;
                    float srcA = p.pixels[j + 3] / 255.0f;

                    // Standard alpha blending (Source Over Destination)
                    float outA = srcA + bgA * (1.0f - srcA);
                    if (outA > 1e-6f) {
                        compositeBuffer[j + 0] = (uint8_t)(((srcR * srcA) + (bgR * bgA * (1.0f - srcA))) / outA * 255.0f);
                        compositeBuffer[j + 1] = (uint8_t)(((srcG * srcA) + (bgG * bgA * (1.0f - srcA))) / outA * 255.0f);
                        compositeBuffer[j + 2] = (uint8_t)(((srcB * srcA) + (bgB * bgA * (1.0f - srcA))) / outA * 255.0f);
                        compositeBuffer[j + 3] = (uint8_t)(outA * 255.0f);
                    } else {
                        compositeBuffer[j + 0] = 0;
                        compositeBuffer[j + 1] = 0;
                        compositeBuffer[j + 2] = 0;
                        compositeBuffer[j + 3] = 0;
                    }
                }
                finalPixels = compositeBuffer.data();
            }

            if (!stbi_write_png(pngPath.string().c_str(), exportW, exportH, 4, finalPixels, p.width * 4)) {
                std::cerr << "Failed to write PNG to " << pngPath << std::endl;
                return false;
            }
        }
    }

    // 2. Save Data
    std::string finalExt = extension.empty() ? ".fnt" : extension;
    if(finalExt[0] != '.') finalExt = "." + finalExt;
    std::string fntFilename = fileNameBase + finalExt;

    // Dispatch
    if (format == 2) {
        WriteBinary(atlas, destinationFolder, fntFilename, pageFilenames);
    } else if (format == 1) {
        WriteText(atlas, destinationFolder, fntFilename, pageFilenames);
    } else {
        WriteXML(atlas, destinationFolder, fntFilename, pageFilenames);
    }

    return true;
}


