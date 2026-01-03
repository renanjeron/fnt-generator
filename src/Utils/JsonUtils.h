#pragma once
#include <string>
#include <vector>
#include <set>
#include <cstdint>

namespace Utils {
    std::string ParseStringValue(const std::string& content, const std::string& key);
    float ParseFloatValue(const std::string& content, const std::string& key, float def);
    int ParseIntValue(const std::string& content, const std::string& key, int def);
    bool ParseBoolValue(const std::string& content, const std::string& key, bool def);
    void ParseColor3(const std::string& content, const std::string& key, float* col);
    void ParseColor4(const std::string& content, const std::string& key, float* col);
    void ParseIntArray(const std::string& content, const std::string& key, std::set<uint32_t>& outSet);
}
