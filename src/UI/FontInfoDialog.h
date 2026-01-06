#pragma once
#include "../Font/FontManager.h"

namespace FontInfoDialog {
    // Renders the [i] button. If clicked, opens the modal and fetches metadata from the provided manager.
    void RenderButton(const FontManager& fontManager);

    // Renders the modal dialog if it is open.
    void RenderDialog(const FontManager& fontManager);
}
