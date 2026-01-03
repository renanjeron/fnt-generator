#pragma once
#include <string>
#include "../Atlas/TextureGenerator.h"

class Exporter {
public:
    static bool ExportAtlasToDisk(const AtlasResult& atlas, const std::string& destinationFolder, const std::string& fileNameBase);
};
