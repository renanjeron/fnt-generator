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
    // Actually, ps-pat-load just reads len*2.
    // PHP writes ucs2 string then 00 00.
    // Let's peek.
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
    std::ofstream log("pat_debug_log.txt");
    log << "[PatLoader] Start Load: " << path << std::endl;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        log << "[PatLoader] Failed to open file." << std::endl;
        return images;
    }

    // 1. Global Header
    uint32_t sig = ReadUInt32BE(file); 
    if (sig != 0x38425054) { // "8BPT"
        log << "[PatLoader] Invalid Signature" << std::endl;
        return images;
    }

    uint16_t version = ReadUInt16BE(file); // 2 bytes
    if (version != 1) {
        log << "[PatLoader] Unsupported Version: " << version << std::endl;
        return images;
    }

    uint32_t count = ReadUInt32BE(file); // 4 bytes
    log << "[PatLoader] Pattern Count: " << count << std::endl;

    for (uint32_t i = 0; i < count; i++) {
        log << "[PatLoader] Loading Pattern " << i << std::endl;
        
        uint32_t patVer = ReadUInt32BE(file);
        uint32_t mode = ReadUInt32BE(file); // 1=Gray, 2=Indexed, 3=RGB
        uint16_t h = ReadUInt16BE(file);
        uint16_t w = ReadUInt16BE(file);
        
        log << "[PatLoader] PatVer: " << patVer << " Mode: " << mode << " Size: " << w << "x" << h << std::endl;

        std::string name = ReadString(file);
        log << "[PatLoader] Name: " << name << std::endl;

        // ID / UUID (37 bytes usually - length byte + 36 chars? or just fixed?)
        // ps-pat-load.c skips 37 bytes.
        // PHP writes 0x24 (1 byte) + 36 byte UUID strings.
        // Let's read 1 byte len.
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

        uint32_t colorModel = ReadUInt32BE(file); // Should be 3 for RGB, 1 for Gray. Matches Mode?
        // Note: Reference reads this.
        
        uint32_t patternSize = ReadUInt32BE(file); // Size of remaining data
        log << "[PatLoader] Pattern Data Size: " << patternSize << std::endl;
        
        std::streampos patDataStart = file.tellg();
        std::streampos nextPatPos = patDataStart + (std::streampos)patternSize;

        // Header Rect
        uint32_t top = ReadUInt32BE(file);
        uint32_t left = ReadUInt32BE(file);
        uint32_t bottom = ReadUInt32BE(file);
        uint32_t right = ReadUInt32BE(file);
        uint32_t depth = ReadUInt32BE(file); // 24?

        log << "[PatLoader] Rect: " << left << "," << top << " " << " Depth: " << depth << std::endl;

        // Channels
        // Derived from Mode
        int numChannels = 3;
        if (mode == 1 || mode == 2) numChannels = 1;

        std::vector<std::vector<uint8_t>> channels;
        
        for (int c = 0; c < numChannels; c++) {
            // Read Channel
            uint32_t chVer = ReadUInt32BE(file);
            uint32_t chSize = ReadUInt32BE(file); // Length of data + header fields?
            // ps-pat-load.c:
            // if (!pspat_read_ulong (f, &sample_size)) ...
            // And then later reads data of size: width * height?
            // NO. The data is compressed.
            // chSize includes data + some header bytes?
            
            // Ref: channel['size'] = 23 + data_size.
            // Header is 23 bytes (after size?).
            // Let's parse header.
            
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
            
            log << "[PatLoader] Chan " << c << " DataLen: " << dataLen << " Comp: " << (int)compression << std::endl;

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
        // ps-pat-load checks: if (ftell < next_pattern - (88 + 31)) ?
        // 88 is some padding? 31 is alpha header?
        
        if (cur < nextPatPos) {
             // Maybe padding?
             // PHP: writes padding.
             // If we have substantial data left, try reading alpha.
             // Alpha channel has same header (23 bytes) + Ver(4) + Size(4) = 31 bytes minimum + data.
             if ((int64_t)(nextPatPos - cur) > 31) {
                 log << "[PatLoader] Found extra data, attempting Alpha." << std::endl;
                 
                 // Maybe padding first?
                 // PHP: writes padding.
                 // We can search for header?
                 // Simple heuristic: Try to read channel header.
                 // First 4 bytes = Version (1).
                 // We can scan for 00 00 00 01?
                 
                 // Let's just try reading as channel.
                 // Need to skip padding?
                 // ps-pat-load: fseek(f, 88, SEEK_CUR); ???
                 // It skips 88 bytes padding for RGB?
                 // Let's peek.
                 
                 // Better: align to end, but if we want alpha, we need it.
                 // Just support RGB for now to fix the bug.
                 // The user wants patterns, usually opaque.
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
