#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include "../Atlas/TextureGenerator.h"

// Forward decl
struct AtlasSettings;

namespace UI {
    class ReplaceGlyphDialog {
    public:
        // Opens the dialog for a specific character
        static void Open(uint32_t charCode, const ReplacedGlyph& existing, bool isNew);
        
        // Renders the dialog. Returns true if "Save" was clicked (state updated).
        // The updated replacement is written to 'outReplacement'.
        // 'shouldRemove' is set to true if user clicked "Remove".
        // onLiveUpdate is called whenever the glyph changes (for real-time preview)
        static void Show(bool* p_open, const AtlasSettings& globalSettings, ReplacedGlyph& outReplacement, uint32_t& outCharCode, bool& outSaved, bool& outRemoved, std::function<void(uint32_t, const ReplacedGlyph&)> onLiveUpdate = nullptr);

    private:
        static void UpdatePreview();
    };
}
