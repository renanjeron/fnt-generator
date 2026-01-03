#include "Exporter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

bool Exporter::ExportAtlasToDisk(const AtlasResult& atlas, const std::string& destinationFolder, const std::string& fileNameBase) {
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

    // 2. Save Data (XML .fnt)
    std::string fntFilename = fileNameBase + ".fnt";
    std::filesystem::path fntPath = folder / fntFilename;

    std::ofstream o(fntPath);
    if (!o.is_open()) return false;

    // Header matches the reference
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>" << std::endl;
    o << "<!--Created using Font Exporter Native-->" << std::endl;
    o << "<font>" << std::endl;
    
    // Info tag
    o << "    <info face=\"" << fileNameBase << "\" size=\"" << atlas.fontSize << "\" bold=\"0\" italic=\"0\" charset=\"\" unicode=\"1\" stretchH=\"100\" smooth=\"1\" aa=\"1\" padding=\"0,0,0,0\" spacing=\"0,0\"/>" << std::endl;
    
    // Common tag
    // lineHeight: derived from font metrics. base: ascender.
    o << "    <common lineHeight=\"" << atlas.lineHeight << "\" base=\"" << atlas.base << "\" scaleW=\"" << atlas.width << "\" scaleH=\"" << atlas.height << "\" pages=\"1\" packed=\"0\"/>" << std::endl;
    
    // Pages
    o << "    <pages>" << std::endl;
    o << "        <page id=\"0\" file=\"" << pngFilename << "\"/>" << std::endl;
    o << "    </pages>" << std::endl;

    // Chars
    o << "    <chars count=\"" << atlas.glyphs.size() << "\">" << std::endl;

    for (const auto& g : atlas.glyphs) {
        // <char id="48" x="666" y="2" width="54" height="57" xoffset="-8" yoffset="30" xadvance="39" page="0" chnl="0" letter="0"/>
        o << "        <char id=\"" << g.charCode << "\" "
          << "x=\"" << g.x << "\" "
          << "y=\"" << g.y << "\" "
          << "width=\"" << g.width << "\" "
          << "height=\"" << g.height << "\" "
          << "xoffset=\"" << g.xoffset << "\" "
          << "yoffset=\"" << g.yoffset << "\" " // IMPORTANT: Y-offset often needs flipping/adjustment depending on coord system. 
                                    // In FreeType, Y is up. In Texture/Screen, Y is down.
                                    // Ref file has positive yoffset ~30 for size 50. 
                                    // Our g.yoffset is bearingY (upwards). 
                                    // We might need to invert or adjust: offset = ascent - bearingY? 
                                    // For now using bearingY directly but it might need negation/adjustment.
          << "xadvance=\"" << g.advance << "\" "
          << "page=\"0\" chnl=\"0\" letter=\"";
          
          // Safety for XML chars
          if (g.charCode == '"') o << "&quot;";
          else if (g.charCode == '&') o << "&amp;";
          else if (g.charCode == '<') o << "&lt;";
          else if (g.charCode == '>') o << "&gt;";
          else if (g.charCode >= 32 && g.charCode <= 126) o << (char)g.charCode;
          // else: leave empty or hex? Reference puts explicit char if printable.
        
        o << "\"/>" << std::endl;
    }
    
    o << "    </chars>" << std::endl;
    o << "    <kernings count=\"0\">" << std::endl;
    o << "    </kernings>" << std::endl;
    o << "</font>" << std::endl;

    return true;
}

