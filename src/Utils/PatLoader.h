#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct PatImage {
    std::string name;
    std::string id;
    int width;
    int height;
    int mode; // 1=Grayscale, 3=RGB, 4=CMYK
    std::vector<uint8_t> pixels; // RGBA encoded
    bool isValid;
};

class PatLoader {
public:
    static std::vector<PatImage> Load(const std::string& path);

private:
    static uint32_t ReadUInt32BE(std::ifstream& file);
    static uint16_t ReadUInt16BE(std::ifstream& file);
    static uint8_t ReadUInt8(std::ifstream& file);
    static std::string ReadString(std::ifstream& file); // Reads length + unicode string?
    static void DecodePackBits(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& dst);
};
