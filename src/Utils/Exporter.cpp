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

static void WriteXML(const AtlasResult& atlas, const std::string& folder, const std::string& filename, const std::string& pngFilename) {
    std::filesystem::path path = std::filesystem::path(folder) / filename;
    std::ofstream o(path);
    if (!o.is_open()) return;

    o << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" << std::endl;
    o << "<!--Created using Fnt Generator-->" << std::endl;
    o << "<font>" << std::endl;
    o << "    <info face=\"FntGenerator\" size=\"" << atlas.fontSize << "\" bold=\"0\" italic=\"0\" charset=\"\" unicode=\"1\" stretchH=\"100\" smooth=\"1\" aa=\"1\" padding=\"0,0,0,0\" spacing=\"0,0\"/>" << std::endl;
    o << "    <common lineHeight=\"" << atlas.lineHeight << "\" base=\"" << atlas.base << "\" scaleW=\"" << atlas.width << "\" scaleH=\"" << atlas.height << "\" pages=\"1\" packed=\"0\"/>" << std::endl;
    o << "    <pages>" << std::endl;
    o << "        <page id=\"0\" file=\"" << pngFilename << "\"/>" << std::endl;
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
          << "page=\"0\" chnl=\"15\" letter=\"";

        if (g.charCode == '"') o << "&quot;";
        else if (g.charCode == '&') o << "&amp;";
        else if (g.charCode == '<') o << "&lt;";
        else if (g.charCode == '>') o << "&gt;";
        else {
             // Basic printable ASCII check, otherwise empty or hex if actually needed (usually empty in standard bmfont XML)
             if (g.charCode >= 32 && g.charCode <= 126) o << (char)g.charCode;
        }
        o << "\"/>" << std::endl;
    }
    
    o << "    </chars>" << std::endl;
    o << "    <kernings count=\"0\">" << std::endl;
    o << "    </kernings>" << std::endl;
    o << "</font>" << std::endl;
}

static void WriteText(const AtlasResult& atlas, const std::string& folder, const std::string& filename, const std::string& pngFilename) {
    std::filesystem::path path = std::filesystem::path(folder) / filename;
    std::ofstream o(path);
    if (!o.is_open()) return;

    // Info
    o << "info face=\"FntGenerator\" size=" << atlas.fontSize << " bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=0,0" << std::endl;
    // Common
    o << "common lineHeight=" << atlas.lineHeight << " base=" << atlas.base << " scaleW=" << atlas.width << " scaleH=" << atlas.height << " pages=1 packed=0 alphaChnl=1 redChnl=0 greenChnl=0 blueChnl=0" << std::endl;
    // Pages
    o << "page id=0 file=\"" << pngFilename << "\"" << std::endl;
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
          << " page=0 chnl=15" << std::endl;
    }
    // Kernings
    o << "kernings count=0" << std::endl;
}

static void WriteByte(std::ofstream& o, uint8_t v) { o.write((const char*)&v, 1); }
static void WriteShort(std::ofstream& o, int16_t v) { o.write((const char*)&v, 2); } // Assumes LE
static void WriteUShort(std::ofstream& o, uint16_t v) { o.write((const char*)&v, 2); }
static void WriteInt(std::ofstream& o, int32_t v) { o.write((const char*)&v, 4); }
static void WriteUInt(std::ofstream& o, uint32_t v) { o.write((const char*)&v, 4); }

static void WriteBinary(const AtlasResult& atlas, const std::string& folder, const std::string& filename, const std::string& pngFilename) {
    std::filesystem::path path = std::filesystem::path(folder) / filename;
    std::ofstream o(path, std::ios::binary);
    if (!o.is_open()) return;

    // Header BMF3
    WriteByte(o, 66); // B
    WriteByte(o, 77); // M
    WriteByte(o, 70); // F
    WriteByte(o, 3);  // Version 3

    // Block 1: Info
    // Size calculation:
    // fontSize(2) + bitField(1) + charSet(1) + stretchH(2) + aa(1) + padding(4) + spacing(2) + outline(1) + string(n+1)
    // 14 bytes + string
    std::string fontName = "FntGenerator";
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
    WriteUShort(o, (uint16_t)atlas.width);
    WriteUShort(o, (uint16_t)atlas.height);
    WriteUShort(o, 1); // pages
    WriteByte(o, 0); // bitField (packed=0)
    WriteByte(o, 0); // alpha
    WriteByte(o, 0); // red
    WriteByte(o, 0); // green
    WriteByte(o, 0); // blue

    // Block 3: Pages
    // p strings (1 page)
    int32_t blockSize3 = (int32_t)pngFilename.length() + 1;
    WriteByte(o, 3); // Block Type 3
    WriteInt(o, blockSize3);
    o.write(pngFilename.c_str(), pngFilename.length() + 1);

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
        WriteByte(o, 0); // page
        WriteByte(o, 15); // chnl (all)
    }

    // Block 5: Kerning (Empty) (To-do?)
}

bool Exporter::ExportAtlasToDisk(const AtlasResult& atlas, const std::string& destinationFolder, const std::string& fileNameBase, int format, const std::string& extension) {
    std::filesystem::path folder(destinationFolder);
    if (!std::filesystem::exists(folder)) {
        return false;
    }

    // 1. Save Image (PNG)
    std::string pngFilename = fileNameBase + ".png";
    std::filesystem::path pngPath = folder / pngFilename;

    if (!stbi_write_png(pngPath.string().c_str(), atlas.width, atlas.height, 4, atlas.pixels.data(), atlas.width * 4)) {
        std::cerr << "Failed to write PNG to " << pngPath << std::endl;
        return false;
    }

    // 2. Save Data
    std::string finalExt = extension.empty() ? ".fnt" : extension;
    if(finalExt[0] != '.') finalExt = "." + finalExt;
    std::string fntFilename = fileNameBase + finalExt;

    // Dispatch
    // format: 0=XML, 1=Text, 2=Binary
    if (format == 2) {
        WriteBinary(atlas, destinationFolder, fntFilename, pngFilename);
    } else if (format == 1) {
        WriteText(atlas, destinationFolder, fntFilename, pngFilename);
    } else {
        WriteXML(atlas, destinationFolder, fntFilename, pngFilename);
    }

    return true;
}

