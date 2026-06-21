#pragma once

#include "ThemeManager.h"

namespace gui::theme {

/**
 * @class ThemeSelectorUI
 * @brief ImGui theme selector component for settings/navigation
 */
class ThemeSelectorUI {
public:
    explicit ThemeSelectorUI(ThemeManager& theme_manager);

    /**
     * @brief Render theme selector UI
     * @return true if theme was changed
     */
    bool render();

    /**
     * @brief Render compact theme selector (for toolbar)
     */
    void render_compact();

private:
    ThemeManager& theme_manager_;
};

}  // namespace gui::theme