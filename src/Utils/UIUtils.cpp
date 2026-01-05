#include "UIUtils.h"
#include "PlatformUtils.h"
#include "JsonUtils.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Utils {

bool KnobAngle(const char* label, float* p_value, float min_v, float max_v) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    
    float radius_outer = 12.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(pos.x + radius_outer, pos.y + radius_outer);
    
    ImGui::PushID(label);
    ImGui::InvisibleButton("##knob", ImVec2(radius_outer*2, radius_outer*2));
    
    bool value_changed = false;
    bool is_active = ImGui::IsItemActive();
    
    if (is_active) {
        ImVec2 d = ImVec2(io.MousePos.x - center.x, io.MousePos.y - center.y);
        if (d.x*d.x + d.y*d.y > 0) {
            float angle = std::atan2(d.y, d.x) * 180.0f / 3.14159265f;
            *p_value = angle;
            value_changed = true;
        }
    }
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircleFilled(center, radius_outer, ImGui::GetColorU32(ImGuiCol_FrameBg), 16);
    draw_list->AddCircle(center, radius_outer, ImGui::GetColorU32(ImGuiCol_Border), 16);
    
    float angle_rad = *p_value * 3.14159265f / 180.0f;
    draw_list->AddLine(center, ImVec2(center.x + std::cos(angle_rad) * radius_outer, center.y + std::sin(angle_rad) * radius_outer), ImGui::GetColorU32(ImGuiCol_SliderGrabActive), 2.0f);
    
    ImGui::PopID();
    return value_changed;
}

int MyResizeCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = (std::string*)data->UserData;
        str->resize(data->BufSize);
        data->Buf = (char*)str->c_str();
    }
    return 0;
}

unsigned int CreateCheckerTexture() {
    int w = 32, h = 32;
    std::vector<unsigned char> pixels(w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            bool dark = ((x / 8) + (y / 8)) % 2 == 1;
            uint8_t c = dark ? 204 : 255;
            int idx = (y * w + x) * 4;
            pixels[idx] = c;
            pixels[idx+1] = c;
            pixels[idx+2] = c;
            pixels[idx+3] = 255;
        }
    }

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return tex;
}

void SetWindowIcon(GLFWwindow* window) {
    if (!window) return;
    
    // Attempt to load from several locations
    std::vector<std::string> paths = {
        "res/icon.png",                                      // CWD
        GetExecutablePath() + "/res/icon.png",               // Exe Dir
        GetExecutablePath() + "/../res/icon.png",            // Exe Parent (for build/Release structure)
        "../res/icon.png"                                    // Relative to CWD parent
    };

    GLFWimage image;
    int channels;
    unsigned char* pixels = nullptr;

    for (const auto& path : paths) {
        pixels = stbi_load(path.c_str(), &image.width, &image.height, &channels, 4);
        if (pixels) break;
    }
    
    if (pixels) {
        image.pixels = pixels;
        glfwSetWindowIcon(window, 1, &image);
        stbi_image_free(pixels);
    }
}

void ParseGradientStops(const std::string& content, const std::string& key, ImGG::GradientWidget& widget) {
    size_t pos = content.find("\"" + key + "\"");
    if (pos == std::string::npos) return;
    size_t start = content.find("[", pos);
    if (start == std::string::npos) return;

    int brackets = 0;
    size_t aEnd = start;
    for(size_t i=start; i<content.length(); i++) {
        if(content[i] == '[') brackets++;
        else if(content[i] == ']') {
            brackets--;
            if(brackets == 0) { aEnd = i; break; }
        }
    }
    
    widget.gradient().clear();
    std::string block = content.substr(start, aEnd - start + 1);
    
    size_t bCur = 0;
    while(true) {
        size_t bObj = block.find("{", bCur);
        if(bObj == std::string::npos) break;
        size_t bObjClose = block.find("}", bObj);
        if(bObjClose == std::string::npos) break;
        
        std::string item = block.substr(bObj, bObjClose - bObj + 1);
        float p = Utils::ParseFloatValue(item, "p", 0.0f);
        float c[4] = {1,1,1,1};
        Utils::ParseColor4(item, "c", c);
        
        widget.gradient().add_mark(ImGG::Mark(ImGG::RelativePosition(p), ImVec4(c[0], c[1], c[2], c[3])));
        
        bCur = bObjClose + 1;
    }
    
    if(widget.gradient().get_marks().empty()) {
        widget.gradient().add_mark(ImGG::Mark(ImGG::RelativePosition(0.0f), ImVec4(0,0,0,1)));
        widget.gradient().add_mark(ImGG::Mark(ImGG::RelativePosition(1.0f), ImVec4(1,1,1,1)));
    }
}

} // namespace Utils
