#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace Utils {
    std::vector<uint32_t> DecodeUtf8(const char* p);
    std::string EncodeUtf8(const std::vector<uint32_t>& charset);
    bool StringContains(const std::string& str, const std::string& sub);
}
