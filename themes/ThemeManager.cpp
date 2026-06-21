#include "ThemeManager.h"
#include <imgui.h>
#include <fstream>
#include <json/json.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui::theme {

ThemeManager::ThemeManager() 
    : current_theme_(ThemeType::DARK) {
    init_light_theme();
    init_dark_theme();
    init_high_contrast_theme();
}

bool ThemeManager::initialize() {
    return load_theme_preference();
}

ThemeType ThemeManager::get_current_theme() const {
    return current_theme_;
}

const ThemeColors& ThemeManager::get_current_colors() const {
    switch (current_theme_) {
        case ThemeType::LIGHT:
            return light_colors_;
        case ThemeType::DARK:
            return dark_colors_;
        case ThemeType::HIGH_CONTRAST:
            return high_contrast_colors_;
        default:
            return dark_colors_;
    }
}

void ThemeManager::set_theme(ThemeType theme_type, bool apply_immediately) {
    current_theme_ = theme_type;
    if (apply_immediately) {
        apply_theme_to_imgui();
        save_theme_preference();
    }
}

const ThemeColors& ThemeManager::get_theme_colors(ThemeType theme_type) const {
    switch (theme_type) {
        case ThemeType::LIGHT:
            return light_colors_;
        case ThemeType::DARK:
            return dark_colors_;
        case ThemeType::HIGH_CONTRAST:
            return high_contrast_colors_;
        default:
            return dark_colors_;
    }
}

std::vector<ThemeMetadata> ThemeManager::get_all_themes() const {
    return {
        {ThemeType::LIGHT, "light", "Light Theme", "Bright interface suitable for daytime use"},
        {ThemeType::DARK, "dark", "Dark Theme", "Dark backgrounds to reduce eye strain"},
        {ThemeType::HIGH_CONTRAST, "high_contrast", "High Contrast", "Maximum contrast for accessibility"}
    };
}

void ThemeManager::apply_theme_to_imgui() {
    apply_colors_to_imgui(get_current_colors());
}

bool ThemeManager::save_theme_preference() {
    try {
        auto config_path = get_theme_config_path();
        fs::create_directories(fs::path(config_path).parent_path());

        Json::Value root;
        root["theme"] = static_cast<int>(current_theme_);
        root["version"] = "1.0";

        std::ofstream config_file(config_path);
        if (!config_file.is_open()) {
            return false;
        }

        Json::StreamWriterBuilder writer;
        config_file << Json::writeString(writer, root);
        config_file.close();
        return true;
    } catch (...) {
        return false;
    }
}

bool ThemeManager::load_theme_preference() {
    try {
        auto config_path = get_theme_config_path();
        
        // If config doesn't exist, use default (dark theme)
        if (!fs::exists(config_path)) {
            current_theme_ = ThemeType::DARK;
            apply_theme_to_imgui();
            return true;
        }

        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            return false;
        }

        Json::Value root;
        config_file >> root;
        config_file.close();

        if (root.isMember("theme")) {
            int theme_index = root["theme"].asInt();
            if (theme_index >= 0 && theme_index < static_cast<int>(ThemeType::COUNT)) {
                current_theme_ = static_cast<ThemeType>(theme_index);
                apply_theme_to_imgui();
                return true;
            }
        }

        // Fallback to dark theme
        current_theme_ = ThemeType::DARK;
        apply_theme_to_imgui();
        return true;
    } catch (...) {
        current_theme_ = ThemeType::DARK;
        return false;
    }
}

std::string ThemeManager::get_theme_config_path() {
    // Cross-platform config path
    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            return std::string(appdata) + "\\Amplitron\\theme_config.json";
        }
        return "theme_config.json";
    #else
        const char* home = std::getenv("HOME");
        if (home) {
            return std::string(home) + "/.config/amplitron/theme_config.json";
        }
        return "theme_config.json";
    #endif
}

// ===== LIGHT THEME =====
void ThemeManager::init_light_theme() {
    light_colors_.primary = ImVec4(0.2f, 0.5f, 0.9f, 1.0f);       // Blue
    light_colors_.primary_light = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
    light_colors_.primary_dark = ImVec4(0.0f, 0.3f, 0.7f, 1.0f);

    light_colors_.secondary = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);     // Orange
    light_colors_.secondary_light = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
    light_colors_.secondary_dark = ImVec4(0.7f, 0.4f, 0.0f, 1.0f);

    light_colors_.bg_main = ImVec4(0.98f, 0.98f, 0.98f, 1.0f);    // Near white
    light_colors_.bg_secondary = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
    light_colors_.bg_tertiary = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
    light_colors_.bg_hover = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);

    light_colors_.text_primary = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);   // Dark text
    light_colors_.text_secondary = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    light_colors_.text_disabled = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    light_colors_.text_inverse = ImVec4(0.98f, 0.98f, 0.98f, 1.0f);

    light_colors_.border = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    light_colors_.border_light = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    light_colors_.border_dark = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

    light_colors_.success = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);        // Green
    light_colors_.warning = ImVec4(1.0f, 0.7f, 0.0f, 1.0f);        // Yellow
    light_colors_.error = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);          // Red
    light_colors_.info = ImVec4(0.2f, 0.6f, 0.9f, 1.0f);           // Blue

    light_colors_.button_bg = ImVec4(0.2f, 0.5f, 0.9f, 1.0f);
    light_colors_.button_hover = ImVec4(0.4f, 0.7f, 1.0f, 1.0f);
    light_colors_.button_active = ImVec4(0.0f, 0.3f, 0.7f, 1.0f);
    light_colors_.button_text = ImVec4(0.98f, 0.98f, 0.98f, 1.0f);

    light_colors_.pedal_enabled = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    light_colors_.pedal_disabled = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    light_colors_.pedal_highlight = ImVec4(0.2f, 0.5f, 0.9f, 1.0f);

    light_colors_.knob_fg = ImVec4(0.2f, 0.5f, 0.9f, 1.0f);
    light_colors_.knob_bg = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    light_colors_.led_on = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    light_colors_.led_off = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
}

// ===== DARK THEME =====
void ThemeManager::init_dark_theme() {
    dark_colors_.primary = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);         // Light blue
    dark_colors_.primary_light = ImVec4(0.5f, 0.85f, 1.0f, 1.0f);
    dark_colors_.primary_dark = ImVec4(0.1f, 0.4f, 0.8f, 1.0f);

    dark_colors_.secondary = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);       // Light orange
    dark_colors_.secondary_light = ImVec4(1.0f, 0.85f, 0.6f, 1.0f);
    dark_colors_.secondary_dark = ImVec4(0.8f, 0.5f, 0.1f, 1.0f);

    dark_colors_.bg_main = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);      // Very dark gray
    dark_colors_.bg_secondary = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
    dark_colors_.bg_tertiary = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    dark_colors_.bg_hover = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

    dark_colors_.text_primary = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);  // Light text
    dark_colors_.text_secondary = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
    dark_colors_.text_disabled = ImVec4(0.35f, 0.35f, 0.35f, 1.0f);
    dark_colors_.text_inverse = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);

    dark_colors_.border = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    dark_colors_.border_light = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    dark_colors_.border_dark = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

    dark_colors_.success = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);          // Bright green
    dark_colors_.warning = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);          // Bright yellow
    dark_colors_.error = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);            // Bright red
    dark_colors_.info = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);             // Bright blue

    dark_colors_.button_bg = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
    dark_colors_.button_hover = ImVec4(0.5f, 0.85f, 1.0f, 1.0f);
    dark_colors_.button_active = ImVec4(0.1f, 0.4f, 0.8f, 1.0f);
    dark_colors_.button_text = ImVec4(0.08f, 0.08f, 0.08f, 1.0f);

    dark_colors_.pedal_enabled = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
    dark_colors_.pedal_disabled = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    dark_colors_.pedal_highlight = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);

    dark_colors_.knob_fg = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
    dark_colors_.knob_bg = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    dark_colors_.led_on = ImVec4(0.3f, 0.9f, 0.3f, 1.0f);
    dark_colors_.led_off = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
}

// ===== HIGH CONTRAST THEME =====
void ThemeManager::init_high_contrast_theme() {
    high_contrast_colors_.primary = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);         // Pure blue
    high_contrast_colors_.primary_light = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
    high_contrast_colors_.primary_dark = ImVec4(0.0f, 0.0f, 0.5f, 1.0f);

    high_contrast_colors_.secondary = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);       // Pure yellow
    high_contrast_colors_.secondary_light = ImVec4(1.0f, 1.0f, 0.5f, 1.0f);
    high_contrast_colors_.secondary_dark = ImVec4(0.5f, 0.5f, 0.0f, 1.0f);

    high_contrast_colors_.bg_main = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);         // Pure black
    high_contrast_colors_.bg_secondary = ImVec4(0.05f, 0.05f, 0.05f, 1.0f);
    high_contrast_colors_.bg_tertiary = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    high_contrast_colors_.bg_hover = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);

    high_contrast_colors_.text_primary = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);    // Pure white
    high_contrast_colors_.text_secondary = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    high_contrast_colors_.text_disabled = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    high_contrast_colors_.text_inverse = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

    high_contrast_colors_.border = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);          // Pure white
    high_contrast_colors_.border_light = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    high_contrast_colors_.border_dark = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

    high_contrast_colors_.success = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);         // Bright lime green
    high_contrast_colors_.warning = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);         // Bright yellow
    high_contrast_colors_.error = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);           // Pure red
    high_contrast_colors_.info = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);            // Cyan

    high_contrast_colors_.button_bg = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
    high_contrast_colors_.button_hover = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
    high_contrast_colors_.button_active = ImVec4(0.0f, 0.0f, 0.5f, 1.0f);
    high_contrast_colors_.button_text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    high_contrast_colors_.pedal_enabled = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    high_contrast_colors_.pedal_disabled = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    high_contrast_colors_.pedal_highlight = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);

    high_contrast_colors_.knob_fg = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
    high_contrast_colors_.knob_bg = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    high_contrast_colors_.led_on = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    high_contrast_colors_.led_off = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
}

void ThemeManager::apply_colors_to_imgui(const ThemeColors& colors) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors_array = style.Colors;

    // Window Background
    colors_array[ImGuiCol_WindowBg] = colors.bg_secondary;
    colors_array[ImGuiCol_ChildBg] = colors.bg_main;
    colors_array[ImGuiCol_PopupBg] = colors.bg_secondary;

    // Buttons
    colors_array[ImGuiCol_Button] = colors.button_bg;
    colors_array[ImGuiCol_ButtonHovered] = colors.button_hover;
    colors_array[ImGuiCol_ButtonActive] = colors.button_active;

    // Frames (text input, checkboxes, sliders)
    colors_array[ImGuiCol_FrameBg] = colors.bg_tertiary;
    colors_array[ImGuiCol_FrameBgHovered] = colors.bg_hover;
    colors_array[ImGuiCol_FrameBgActive] = colors.primary;

    // Scrollbar
    colors_array[ImGuiCol_ScrollbarBg] = colors.bg_tertiary;
    colors_array[ImGuiCol_ScrollbarGrab] = colors.border;
    colors_array[ImGuiCol_ScrollbarGrabHovered] = colors.border_light;
    colors_array[ImGuiCol_ScrollbarGrabActive] = colors.primary;

    // Check Mark
    colors_array[ImGuiCol_CheckMark] = colors.primary;

    // Slider
    colors_array[ImGuiCol_SliderGrab] = colors.primary;
    colors_array[ImGuiCol_SliderGrabActive] = colors.primary_light;

    // Text
    colors_array[ImGuiCol_Text] = colors.text_primary;
    colors_array[ImGuiCol_TextDisabled] = colors.text_disabled;

    // Header
    colors_array[ImGuiCol_Header] = colors.primary;
    colors_array[ImGuiCol_HeaderHovered] = colors.primary_light;
    colors_array[ImGuiCol_HeaderActive] = colors.primary_dark;

    // Separator
    colors_array[ImGuiCol_Separator] = colors.border;
    colors_array[ImGuiCol_SeparatorHovered] = colors.border_light;
    colors_array[ImGuiCol_SeparatorActive] = colors.primary;

    // Resize Grip
    colors_array[ImGuiCol_ResizeGrip] = colors.border;
    colors_array[ImGuiCol_ResizeGripHovered] = colors.border_light;
    colors_array[ImGuiCol_ResizeGripActive] = colors.primary;

    // Tab
    colors_array[ImGuiCol_Tab] = colors.bg_tertiary;
    colors_array[ImGuiCol_TabHovered] = colors.primary_light;
    colors_array[ImGuiCol_TabActive] = colors.primary;
    colors_array[ImGuiCol_TabUnfocused] = colors.bg_tertiary;
    colors_array[ImGuiCol_TabUnfocusedActive] = colors.bg_secondary;

    // Title Bar
    colors_array[ImGuiCol_TitleBg] = colors.primary_dark;
    colors_array[ImGuiCol_TitleBgActive] = colors.primary;
    colors_array[ImGuiCol_TitleBgCollapsed] = colors.bg_tertiary;

    // Menu Bar
    colors_array[ImGuiCol_MenuBarBg] = colors.bg_secondary;

    // Plot
    colors_array[ImGuiCol_PlotLines] = colors.primary;
    colors_array[ImGuiCol_PlotLinesHovered] = colors.primary_light;
    colors_array[ImGuiCol_PlotHistogram] = colors.success;
    colors_array[ImGuiCol_PlotHistogramHovered] = colors.success;
}

}  // namespace gui::theme