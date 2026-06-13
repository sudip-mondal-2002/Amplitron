#pragma once

#include "gui/ui_component.h"

namespace Amplitron {

struct KeyboardShortcutsProps {};

/**
 * @brief Modal dialog presenting the list of application keyboard shortcuts.
 */
class GuiKeyboardShortcuts : public UIComponent<KeyboardShortcutsProps> {
   public:
    GuiKeyboardShortcuts() = default;

    /**
     * @brief Render the shortcuts help modal.
     * Uses the internal show_ flag — call render(show) for external visibility control.
     */
    void render() override { render(show_); }

    /** @brief Render with an external show flag (GuiManager holds authoritative state). */
    void render(bool& show);

   private:
    bool show_ = false;
};

}  // namespace Amplitron
