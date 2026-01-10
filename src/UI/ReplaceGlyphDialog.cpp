#include "ReplaceGlyphDialog.h"
#include "imgui.h"
#include "../Utils/PlatformUtils.h"
#include "../Utils/BitmapUtils.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace UI {

    static ReplacedGlyph s_TempGlyph;
    static uint32_t s_CharCode = 0;
    static GLuint s_PreviewTex = 0;
    static int s_OriginalW = 0;
    static int s_OriginalH = 0;
    static float s_AspectRatio = 1.0f;
    static std::string s_PreviewError;
    static bool s_LockRatio = true;
    static bool s_IsNew = false;

    void ReplaceGlyphDialog::Open(uint32_t charCode, const ReplacedGlyph& existing, bool isNew) {
        s_CharCode = charCode;
        s_TempGlyph = existing;
        s_IsNew = isNew;
        s_LockRatio = true;
        s_AspectRatio = 1.0f; // Default
        
        UpdatePreview();
    }

    void ReplaceGlyphDialog::UpdatePreview() {
        if (s_PreviewTex) {
            glDeleteTextures(1, &s_PreviewTex);
            s_PreviewTex = 0;
        }
        s_PreviewError = "";

        if (!s_TempGlyph.imagePath.empty()) {
             std::vector<uint8_t> px; int w, h;
             if (BitmapUtils::GetPatternPixels(s_TempGlyph.imagePath, px, w, h)) {
                  glGenTextures(1, &s_PreviewTex);
                  glBindTexture(GL_TEXTURE_2D, s_PreviewTex);
                  
                  // Store originals
                  s_OriginalW = w;
                  s_OriginalH = h;
                  if (w > 0 && h > 0) s_AspectRatio = (float)h / (float)w;
                  
                  if (s_TempGlyph.width == 0) s_TempGlyph.width = w;
                  if (s_TempGlyph.height == 0) s_TempGlyph.height = h;
                  
                  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
             } else {
                  s_PreviewError = "Failed to load image.";
             }
        }
    }

    void ReplaceGlyphDialog::Show(bool* p_open, const AtlasSettings& globalSettings, ReplacedGlyph& outReplacement, uint32_t& outCharCode, bool& outSaved, bool& outRemoved, std::function<void(uint32_t, const ReplacedGlyph&)> onLiveUpdate) {
        outSaved = false;
        outRemoved = false;
        outCharCode = s_CharCode;
        
        if (!ImGui::BeginPopupModal("Replace Glyph", p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            return;
        }
        
        ImGui::Text("Replace U+%04X with Image", s_CharCode);
        ImGui::Separator();
        
        ImGui::Text("Image Path:");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", s_TempGlyph.imagePath.empty() ? "(None)" : s_TempGlyph.imagePath.c_str());
        
        if (ImGui::Button("Select Image...##Replace")) {
             std::string path = Utils::PickFileDialog("Images (*.png, *.jpg)\0*.png;*.jpg;*.jpeg\0All Files\0*.*\0");
             if (!path.empty()) {
                 s_TempGlyph.imagePath = path;
                 // Reset dims on new image? Or keep?
                 // Usually reset logic is inside UpdatePreview if they are 0, but here strictly they might be set.
                 // Let's reset them to 0 to trigger auto-size in UpdatePreview, effectively resetting to natural size.
                 s_TempGlyph.width = 0; 
                 s_TempGlyph.height = 0;
                 UpdatePreview();
                 
                 // Trigger live update
                 if (onLiveUpdate) {
                     onLiveUpdate(s_CharCode, s_TempGlyph);
                 }
             }
        }
        
        if (!s_TempGlyph.imagePath.empty()) {
            ImGui::Separator();
            ImGui::Separator();
            ImGui::Text("Dimensions:");
            
            ImGui::PushItemWidth(80);
            
            // Width
            if (s_LockRatio) {
                 if (ImGui::Button("R##DimLocked")) {
                    s_TempGlyph.width = s_OriginalW;
                    s_TempGlyph.height = s_OriginalH;
                    if (onLiveUpdate) onLiveUpdate(s_CharCode, s_TempGlyph);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Width and Height to Original Size");
            } else {
                 if (ImGui::Button("R##W")) {
                    s_TempGlyph.width = s_OriginalW;
                    if (onLiveUpdate) onLiveUpdate(s_CharCode, s_TempGlyph);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Width to Original Size");
            }
            ImGui::SameLine();
            
            int oldW = s_TempGlyph.width;
            if (ImGui::DragInt("Width", &s_TempGlyph.width, 1.0f, 0, 4096)) {
                 if (s_LockRatio && oldW > 0 && s_TempGlyph.height > 0) {
                    float ratio = (float)s_TempGlyph.height / (float)oldW;
                    s_TempGlyph.height = (int)(s_TempGlyph.width * ratio);
                }
                if (onLiveUpdate) onLiveUpdate(s_CharCode, s_TempGlyph);
            }
           
            ImGui::SameLine();
            
            // Height
            if (!s_LockRatio) {
                 if (ImGui::Button("R##H")) {
                    s_TempGlyph.height = s_OriginalH;
                    if (onLiveUpdate) onLiveUpdate(s_CharCode, s_TempGlyph);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Height to Original Size");
            } else {
                 // Dummy to align
                 ImGui::Dummy(ImVec2(ImGui::CalcTextSize("R").x + ImGui::GetStyle().FramePadding.x * 2, 0));
            }
            ImGui::SameLine();
            
            int oldH = s_TempGlyph.height;
            if (ImGui::DragInt("Height", &s_TempGlyph.height, 1.0f, 0, 4096)) {
                  if (s_LockRatio) {
                    s_TempGlyph.width = (int)(s_TempGlyph.height / s_AspectRatio);
                }
                if (onLiveUpdate) onLiveUpdate(s_CharCode, s_TempGlyph);
            }
            
            ImGui::SameLine();
            
            // Logic for Lock Ratio:
            // We need to capture aspect ratio when Lock is enabled, or when image changes.
            // But 's_LockRatio' is static.
            // Let's ensure we have a valid ratio if we are locked.
            // s_AspectRatio should be updated when Image loads.
            // If user toggles Lock ON, update s_AspectRatio from current valid dims.
            
            bool lockClicked = ImGui::Checkbox("Lock Ratio", &s_LockRatio);
            if (lockClicked && s_LockRatio) {
                 if (s_TempGlyph.width > 0 && s_TempGlyph.height > 0) {
                     s_AspectRatio = (float)s_TempGlyph.height / (float)s_TempGlyph.width;
                 } else if (s_OriginalW > 0 && s_OriginalH > 0) {
                     s_AspectRatio = (float)s_OriginalH / (float)s_OriginalW;
                 } else {
                     s_AspectRatio = 1.0f;
                 }
            }
            
            ImGui::PopItemWidth();
            
            ImGui::Separator();
            ImGui::Text("Manual Adjustments:");
            bool manualChanged = false;
            
            ImGui::PushItemWidth(60); 
            // X Offset
            if (ImGui::Button("R##XOff")) { s_TempGlyph.xOffset = 0; manualChanged = true; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset X Offset to 0");
            ImGui::SameLine();
            if (ImGui::DragInt("X Off", &s_TempGlyph.xOffset, 1.0f, -1000, 1000)) manualChanged = true;
            ImGui::SameLine();
            
            // Y Offset
            if (ImGui::Button("R##YOff")) { s_TempGlyph.yOffset = 0; manualChanged = true; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Y Offset to 0");
            ImGui::SameLine();
            if (ImGui::DragInt("Y Off", &s_TempGlyph.yOffset, 1.0f, -1000, 1000)) manualChanged = true;
            ImGui::SameLine();
            
            // Advance
             if (ImGui::Button("R##Adv")) { s_TempGlyph.advance = 0; manualChanged = true; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Advance to Auto (0)");
            ImGui::SameLine();
            if (ImGui::DragInt("Adv", &s_TempGlyph.advance, 1.0f, 0, 4096)) manualChanged = true;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set to 0 to auto-match width.");

            ImGui::PopItemWidth();
            
            if (manualChanged && onLiveUpdate) {
                onLiveUpdate(s_CharCode, s_TempGlyph);
            }

            ImGui::Separator();
            bool effectsToggleChanged = false;
            if (ImGui::Checkbox("Apply Effects", &s_TempGlyph.applyEffects)) {
                effectsToggleChanged = true;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("If enabled, applies active global effects to the image.");
            
            if (s_TempGlyph.applyEffects) {
                ImGui::Indent();
                ImGui::TextDisabled("Active Effects:");
                
                bool effectChanged = false;
                bool isFirstEffect = true;

                auto SmartCheckbox = [&](const char* label, bool* v, const char* tooltip = nullptr) {
                    if (!isFirstEffect) {
                        ImGuiStyle& style = ImGui::GetStyle();
                        float windowRight = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                        float textW = ImGui::CalcTextSize(label).x;
                        float itemW = textW + ImGui::GetFrameHeight() + style.ItemInnerSpacing.x;
                        float nextX = ImGui::GetItemRectMax().x + style.ItemSpacing.x + itemW;
                        if (nextX < windowRight) ImGui::SameLine();
                    }
                    bool c = ImGui::Checkbox(label, v);
                    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
                    isFirstEffect = false;
                    return c;
                };

                if (SmartCheckbox("Apply Fill", &s_TempGlyph.applyFill, "Use global Fill Color/Gradient instead of original image colors.")) effectChanged = true;

                if (globalSettings.enableShadow) {
                    if (SmartCheckbox("Shadow", &s_TempGlyph.applyShadow)) effectChanged = true;
                }
                if (globalSettings.enableStroke) {
                    if (SmartCheckbox("Stroke", &s_TempGlyph.applyStroke)) effectChanged = true;
                }
                if (globalSettings.enableInnerGlow) {
                    if (SmartCheckbox("Inner Glow", &s_TempGlyph.applyInnerGlow)) effectChanged = true;
                }
                if (globalSettings.enableBevel) {
                    if (SmartCheckbox("Bevel", &s_TempGlyph.applyBevel)) effectChanged = true;
                }
                if (globalSettings.pattern.enabled) {
                    if (SmartCheckbox("Pattern", &s_TempGlyph.applyPattern)) effectChanged = true;
                }
                if (globalSettings.fillGradient.enabled || globalSettings.strokeGradient.enabled) {
                     if (SmartCheckbox("Gradient", &s_TempGlyph.applyGradient)) effectChanged = true;
                }

                ImGui::Unindent();
                
                if (effectChanged && onLiveUpdate) {
                    onLiveUpdate(s_CharCode, s_TempGlyph);
                }
            }
            
            ImGui::Separator();
            ImGui::Text("Preview (Original Image):");
            if (s_PreviewTex) {
                // Fixed Size Box Logic
                float boxSize = 250.0f;
                // Use Original Dimensions for aspect ratio of the Preview
                float w = (float)s_OriginalW;
                float h = (float)s_OriginalH;
                if (w <= 0) w = 1; if (h <= 0) h = 1;
                
                float scaleX = boxSize / w;
                float scaleY = boxSize / h;
                float scale = std::min(scaleX, scaleY);
                
                ImGui::Image((void*)(intptr_t)s_PreviewTex, ImVec2(w * scale, h * scale), ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), ImVec4(1,1,1,0.5f));
            } else if (!s_PreviewError.empty()) {
                ImGui::TextColored(ImVec4(1,0,0,1), "%s", s_PreviewError.c_str());
            } else {
                 ImGui::Dummy(ImVec2(64, 64));
                 ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(100, 100, 100, 255));
            }
        }
        
        ImGui::Separator();
        if (ImGui::Button("Save##Replace", ImVec2(120, 0))) {
            if (!s_TempGlyph.imagePath.empty()) {
                outReplacement = s_TempGlyph;
                outSaved = true;
            } else {
                 outRemoved = true; // Emptied -> Remove
            }
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Replacement", ImVec2(160, 0))) {
            outRemoved = true;
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }
        
        // Close button?
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

}
