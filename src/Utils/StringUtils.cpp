#include "StringUtils.h"
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace Utils {

std::vector<uint32_t> DecodeUtf8(const char* p) {
    std::vector<uint32_t> res;
    if (!p) return res;
    while (*p) {
        uint32_t c = 0;
        if ((*p & 0x80) == 0) { c = *p++; }
        else if ((*p & 0xE0) == 0xC0) { c = (*p++ & 0x1F) << 6; c |= (*p++ & 0x3F); }
        else if ((*p & 0xF0) == 0xE0) { c = (*p++ & 0x0F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
        else if ((*p & 0xF8) == 0xF0) { c = (*p++ & 0x07) << 18; c |= (*p++ & 0x3F) << 12; c |= (*p++ & 0x3F) << 6; c |= (*p++ & 0x3F); }
        else { p++; continue; }
        // Filter out non-printable glyphs but keep newlines/tabs if needed
        if (c >= 32 || c == '\n' || c == '\r' || c == '\t') res.push_back(c);
    }
    return res;
}

std::string EncodeUtf8(const std::vector<uint32_t>& charset) {
    std::string out;
    for (uint32_t cp : charset) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
            out += (char)((cp >> 6) | 0xC0);
            out += (char)((cp & 0x3F) | 0x80);
        } else if (cp < 0x10000) {
            out += (char)((cp >> 12) | 0xE0);
            out += (char)(((cp >> 6) & 0x3F) | 0x80);
            out += (char)((cp & 0x3F) | 0x80);
        } else if (cp < 0x110000) {
            out += (char)((cp >> 18) | 0xF0);
            out += (char)(((cp >> 12) & 0x3F) | 0x80);
            out += (char)(((cp >> 6) & 0x3F) | 0x80);
            out += (char)((cp & 0x3F) | 0x80);
        }
    }
    return out;
}

bool StringContains(const std::string& str, const std::string& sub) {
    if (sub.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
    );
    return (it != str.end());
}

} // namespace Utils
