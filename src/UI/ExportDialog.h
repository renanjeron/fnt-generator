#pragma once
#include <string>
#include <vector>

namespace ExportDialog {
    // Shows the export button. If clicked, opens the modal.
    // Returns true if the "Export" button in the dialog was clicked.
    bool RenderButton();

    // Renders the modal dialog if it is open.
    // Handles all inputs and the final export trigger.
    void RenderDialog();

    // Renders a success confirmation overlay in the center of the screen
    void RenderSuccessNotification();

    // Trigger open from external code
    void Open();
}
