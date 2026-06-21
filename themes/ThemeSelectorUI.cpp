#include "ThemeSelectorUI.h"
#include <imgui.h>

namespace gui::theme {

ThemeSelectorUI::ThemeSelectorUI(ThemeManager& theme_manager)
    : theme_manager_(theme_manager) {}

bool ThemeSelectorUI::render() {
    bool changed = false;
    auto themes = theme_manager_.get_all_themes();
    auto current_theme = theme_manager_.get_current_theme();

    ImGui::Text("Theme:");
    ImGui::SameLine();

    for (const auto& theme_meta : themes) {
        bool is_selected = (current_theme == theme_meta.type);

        if (ImGui::RadioButton(theme_meta.display_name.c_str(), is_selected)) {
            theme_manager_.set_theme(theme_meta.type);
            changed = true;
        }

        // Tooltip with description
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", theme_meta.description.c_str());
        }

        ImGui::SameLine();
    }

    ImGui::NewLine();
    return changed;
}

void ThemeSelectorUI::render_compact() {
    auto themes = theme_manager_.get_all_themes();
    auto current_theme = theme_manager_.get_current_theme();

    if (ImGui::BeginCombo("##ThemeSelector", "Theme")) {
        for (const auto& theme_meta : themes) {
            bool is_selected = (current_theme == theme_meta.type);

            if (ImGui::Selectable(theme_meta.display_name.c_str(), is_selected)) {
                theme_manager_.set_theme(theme_meta.type);
            }

            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

}  // namespace gui::theme