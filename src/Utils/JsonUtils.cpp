#include "JsonUtils.h"
#include <sstream>

namespace Utils {

std::string ParseStringValue(const std::string& content, const std::string& key) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    size_t start = content.find("\"", pos + key.length() + 2); // after "key":
    if (start == std::string::npos) return "";
    start++; // Skip opening quote
    
    std::string result;
    bool escape = false;
    for (size_t i = start; i < content.length(); ++i) {
        char c = content[i];
        if (escape) {
            switch (c) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: 
                    // Pass through unknown escapes (or handle \u if needed later)
                    // For now, reconstruct the escape sequence if not handled, 
                    // but standard JSON only has specific escapes. 
                    // To be safe against \" breaking things, we primarily acted on ".
                    result += '\\';
                    result += c; 
                    break;
            }
            escape = false;
        } else {
            if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                return result;
            } else {
                result += c;
            }
        }
    }
    return result;
}

float ParseFloatValue(const std::string& content, const std::string& key, float def) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    size_t start = content.find(":", pos);
    if (start == std::string::npos) return def;
    
    size_t end = content.find_first_of(",}\n", start);
    if (end == std::string::npos) end = content.length();
    
    try {
        return std::stof(content.substr(start + 1, end - (start + 1)));
    } catch (...) {
        return def;
    }
}

int ParseIntValue(const std::string& content, const std::string& key, int def) {
    return (int)ParseFloatValue(content, key, (float)def);
}

bool ParseBoolValue(const std::string& content, const std::string& key, bool def) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return def;
    size_t start = content.find(":", pos);
    if (start == std::string::npos) return def;
    
    size_t end = content.find_first_of(",}\n", start);
    if (end == std::string::npos) end = content.length();
    
    std::string val = content.substr(start + 1, end - (start + 1));
    return val.find("true") != std::string::npos;
}

void ParseColor3(const std::string& content, const std::string& key, float* col) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    size_t end = content.find("]", start);
    if(start == std::string::npos || end == std::string::npos) return;
    std::string arr = content.substr(start+1, end-start-1);
    std::stringstream ss(arr);
    std::string seg;
    int idx=0;
    while(std::getline(ss, seg, ',') && idx < 3) {
        try { col[idx++] = std::stof(seg); } catch(...) {}
    }
}

void ParseColor4(const std::string& content, const std::string& key, float* col) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    size_t end = content.find("]", start);
    if(start == std::string::npos || end == std::string::npos) return;
    std::string arr = content.substr(start+1, end-start-1);
    std::stringstream ss(arr);
    std::string seg;
    int idx=0;
    while(std::getline(ss, seg, ',') && idx < 4) {
        try { col[idx++] = std::stof(seg); } catch(...) {}
    }
}

void ParseIntArray(const std::string& content, const std::string& key, std::set<uint32_t>& outSet) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    size_t end = content.find("]", start);
    if(start == std::string::npos || end == std::string::npos) return;
    
    std::string arr = content.substr(start+1, end-start-1);
    std::stringstream ss(arr);
    std::string seg;
    while(std::getline(ss, seg, ',')) {
        try { outSet.insert((uint32_t)std::stoul(seg)); } catch(...) {}
    }
}

} // namespace Utils
