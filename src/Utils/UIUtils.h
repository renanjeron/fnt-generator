#pragma once
#include "imgui.h"
#include <string>

struct GLFWwindow;

namespace Utils {
    bool KnobAngle(const char* label, float* p_value, float min_v, float max_v);
    int MyResizeCallback(ImGuiInputTextCallbackData* data);
    unsigned int CreateCheckerTexture();
    void SetWindowIcon(GLFWwindow* window);
}
