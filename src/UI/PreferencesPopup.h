#pragma once

// Forward declaration
namespace Utils {
    // ...
}

namespace UI {

    class PreferencesPopup {
    public:
        static void Show(bool* open, int* ssaaFactor, bool* showFontPreview, bool* showRecentError);
    };

}
