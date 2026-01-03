#pragma once

#include <imgui.h> // Include ImVec4

namespace ImGG {

/// sRGB, Straight Alpha
using ColorRGBA = ImVec4;

} // namespace ImGG

#ifndef IMVEC4_OPERATORS_DEFINED
#define IMVEC4_OPERATORS_DEFINED
inline bool operator==(const ImVec4& a, const ImVec4& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
inline bool operator!=(const ImVec4& a, const ImVec4& b) {
    return !(a == b);
}
#endif