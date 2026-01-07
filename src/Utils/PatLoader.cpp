#include "PatLoader.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

// --- Helpers ---

uint32_t PatLoader::ReadUInt32BE(std::ifstream& file) {
    uint8_t bytes[4];
    file.read((char*)bytes, 4);
    return (uint32_t)((bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3]);
}

uint16_t PatLoader::ReadUInt16BE(std::ifstream& file) {
    uint8_t bytes[2];
    file.read((char*)bytes, 2);
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

uint8_t PatLoader::ReadUInt8(std::ifstream& file) {
    char b;
    file.read(&b, 1);
    return (uint8_t)b;
}

std::string PatLoader::ReadString(std::ifstream& file) {
    uint32_t len = ReadUInt32BE(file); 
    if (len == 0) return "";
    
    if (len > 1000) return "Invalid Name"; 

    std::vector<char> buffer(len * 2);
    file.read(buffer.data(), len * 2);

    std::string s;
    for (size_t i = 0; i < len; i++) {
        char c = buffer[i * 2 + 1]; 
        if (c) s += c;
    }
    
    // Check/Skip null terminator if present (2 bytes 00 00)
    int p1 = file.peek();
    if (p1 == 0) {
        std::streampos cur = file.tellg();
        char t[2];
        file.read(t, 2);
        if (t[0] == 0 && t[1] == 0) {
            // Consumed
        } else {
            file.seekg(cur);
        }
    }

    return s;
}

// --- Decompression ---

void PatLoader::DecodePackBits(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& dst) {
    size_t i = 0;
    while (i < srcLen) {
        int8_t n = (int8_t)src[i++];
        if (n == -128) {
            // No-op
        } else if (n >= 0) {
            // Literal copy of n+1 bytes
            int count = n + 1;
            for (int k = 0; k < count && i < srcLen; k++) {
                dst.push_back(src[i++]);
            }
        } else {
            // Repeat next byte -n+1 times
            int count = -n + 1;
            if (i < srcLen) {
                uint8_t val = src[i++];
                for (int k = 0; k < count; k++) dst.push_back(val);
            }
        }
    }
}

// --- Main Loader ---

std::vector<PatImage> PatLoader::Load(const std::string& path) {
    std::vector<PatImage> images;
#ifdef _DEBUG
    std::ofstream log("pat_debug_log.txt");
    log << "[PatLoader] Start Load: " << path << std::endl;
#endif

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
#ifdef _DEBUG
        log << "[PatLoader] Failed to open file." << std::endl;
#endif
        return images;
    }

    // 1. Global Header
    uint32_t sig = ReadUInt32BE(file); 
    if (sig != 0x38425054) { // "8BPT"
#ifdef _DEBUG
        log << "[PatLoader] Invalid Signature" << std::endl;
#endif
        return images;
    }

    uint16_t version = ReadUInt16BE(file); // 2 bytes
    if (version != 1) {
#ifdef _DEBUG
        log << "[PatLoader] Unsupported Version: " << version << std::endl;
#endif
        return images;
    }

    uint32_t count = ReadUInt32BE(file); // 4 bytes
#ifdef _DEBUG
    log << "[PatLoader] Pattern Count: " << count << std::endl;
#endif

    for (uint32_t i = 0; i < count; i++) {
#ifdef _DEBUG
        log << "[PatLoader] Loading Pattern " << i << std::endl;
#endif
        
        uint32_t patVer = ReadUInt32BE(file);
        uint32_t mode = ReadUInt32BE(file); // 1=Gray, 2=Indexed, 3=RGB
        uint16_t h = ReadUInt16BE(file);
        uint16_t w = ReadUInt16BE(file);
        
#ifdef _DEBUG
        log << "[PatLoader] PatVer: " << patVer << " Mode: " << mode << " Size: " << w << "x" << h << std::endl;
#endif

        std::string name = ReadString(file);
#ifdef _DEBUG
        log << "[PatLoader] Name: " << name << std::endl;
#endif

        uint8_t idLen = ReadUInt8(file);
        // Skip idLen bytes
        file.seekg(idLen, std::ios::cur);

        // Palette if Indexed (Mode 2)
        if (mode == 2) {
            // Ref: read 768 bytes cmap.
            // Ref: then read pal_size (2), then skip 2.
            file.seekg(768, std::ios::cur);
            file.seekg(4, std::ios::cur);
        }

        uint32_t colorModel = ReadUInt32BE(file); 
        
        uint32_t patternSize = ReadUInt32BE(file); // Size of remaining data
#ifdef _DEBUG
        log << "[PatLoader] Pattern Data Size: " << patternSize << std::endl;
#endif
        
        std::streampos patDataStart = file.tellg();
        std::streampos nextPatPos = patDataStart + (std::streampos)patternSize;

        // Header Rect
        uint32_t top = ReadUInt32BE(file);
        uint32_t left = ReadUInt32BE(file);
        uint32_t bottom = ReadUInt32BE(file);
        uint32_t right = ReadUInt32BE(file);
        uint32_t depth = ReadUInt32BE(file); // 24?

#ifdef _DEBUG
        log << "[PatLoader] Rect: " << left << "," << top << " " << " Depth: " << depth << std::endl;
#endif

        // Channels
        // Derived from Mode
        int numChannels = 3;
        if (mode == 1 || mode == 2) numChannels = 1;

        std::vector<std::vector<uint8_t>> channels;
        
        for (int c = 0; c < numChannels; c++) {
            // Read Channel
            uint32_t chVer = ReadUInt32BE(file);
            uint32_t chSize = ReadUInt32BE(file); // Length of data + header fields?
            
            uint32_t depthUnused = ReadUInt32BE(file);
            uint32_t cTop = ReadUInt32BE(file);
            uint32_t cLeft = ReadUInt32BE(file);
            uint32_t cBottom = ReadUInt32BE(file);
            uint32_t cRight = ReadUInt32BE(file);
            uint16_t cDepth = ReadUInt16BE(file);
            uint8_t compression = ReadUInt8(file);

            // Channel Header fields size = 4+4+4+4+4+2+1 = 23 bytes.
            // So data length = chSize - 23.
            
            uint32_t dataLen = (chSize >= 23) ? (chSize - 23) : 0;
            
#ifdef _DEBUG
            log << "[PatLoader] Chan " << c << " DataLen: " << dataLen << " Comp: " << (int)compression << std::endl;
#endif

            std::vector<uint8_t> chData;
            
            if (compression == 1) {
                // RLE
                std::vector<uint8_t> raw(dataLen);
                file.read((char*)raw.data(), dataLen);
                
                // Try to detect PSD scanline table
                bool hasTable = false;
                uint32_t sum = 0;
                if (dataLen > (uint32_t)h * 2) {
                    for(int y=0; y<h; y++) {
                        uint16_t rLen = (raw[y*2] << 8) | raw[y*2+1];
                        sum += rLen;
                    }
                    if (sum + h*2 == dataLen) hasTable = true;
                }
                
                if (hasTable) {
                    int offset = h*2;
                    for(int y=0; y<h; y++) {
                         uint16_t rLen = (raw[y*2] << 8) | raw[y*2+1];
                         DecodePackBits(raw.data() + offset, rLen, chData);
                         offset += rLen;
                    }
                } else {
                    DecodePackBits(raw.data(), dataLen, chData);
                }
            } else {
                 // Raw
                 chData.resize(dataLen);
                 file.read((char*)chData.data(), dataLen);
            }
            
            channels.push_back(chData);
        }

        // Check for Alpha (if room left)
        bool hasAlpha = false;
        std::streampos cur = file.tellg();
        
        if (cur < nextPatPos) {
             // If we have substantial data left, try reading alpha.
             // Alpha channel has same header (23 bytes) + Ver(4) + Size(4) = 31 bytes minimum + data.
             if ((int64_t)(nextPatPos - cur) > 31) {
#ifdef _DEBUG
                 log << "[PatLoader] Found extra data, attempting Alpha." << std::endl;
#endif
             }
        }

        // Create Image
        if ((mode == 3 && channels.size() >= 3) || (mode == 1 && channels.size() >= 1)) {
             PatImage img;
             img.name = name;
             img.width = w;
             img.height = h;
             img.mode = mode;
             img.pixels.resize(w * h * 4);
             
             for(int i=0; i<w*h; i++) {
                 uint8_t r=0, g=0, b=0, a=255;
                 if (mode == 3) {
                     if (i < channels[0].size()) r = channels[0][i];
                     if (i < channels[1].size()) g = channels[1][i];
                     if (i < channels[2].size()) b = channels[2][i];
                 } else {
                     if (i < channels[0].size()) r = g = b = channels[0][i];
                 }
                 img.pixels[i*4+0] = r;
                 img.pixels[i*4+1] = g;
                 img.pixels[i*4+2] = b;
                 img.pixels[i*4+3] = a;
             }
             img.isValid = true;
             images.push_back(img);
        }

        // Jump to next
        file.seekg(nextPatPos);
    }

    return images;
}
