#pragma once

#include "ThemeTypes.h"
#include <memory>
#include <string>
#include <vector>

namespace gui::theme {

/**
 * @class ThemeManager
 * @brief Manages theme switching, persistence, and application
 * 
 * Responsibilities:
 * - Load/save user theme preference to local storage
 * - Apply themes globally to ImGui style
 * - Provide theme switching functionality
 * - Auto-load saved theme on startup
 */
class ThemeManager {
public:
    ThemeManager();
    ~ThemeManager() = default;

    /**
     * @brief Initialize theme manager and load saved preference
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Get the current active theme
     */
    ThemeType get_current_theme() const;

    /**
     * @brief Get current theme colors
     */
    const ThemeColors& get_current_colors() const;

    /**
     * @brief Switch to a specific theme
     * @param theme_type Theme to switch to
     * @param apply_immediately Apply theme to ImGui immediately
     */
    void set_theme(ThemeType theme_type, bool apply_immediately = true);

    /**
     * @brief Get theme colors for a specific theme type
     */
    const ThemeColors& get_theme_colors(ThemeType theme_type) const;

    /**
     * @brief Get metadata for all available themes
     */
    std::vector<ThemeMetadata> get_all_themes() const;

    /**
     * @brief Apply current theme to ImGui style
     */
    void apply_theme_to_imgui();

    /**
     * @brief Save current theme preference to storage
     * @return true if save successful
     */
    bool save_theme_preference();

    /**
     * @brief Load theme preference from storage
     * @return true if load successful
     */
    bool load_theme_preference();

    /**
     * @brief Get storage path for theme preferences
     */
    static std::string get_theme_config_path();

private:
    ThemeType current_theme_;
    ThemeColors light_colors_;
    ThemeColors dark_colors_;
    ThemeColors high_contrast_colors_;

    /**
     * @brief Initialize light theme colors
     */
    void init_light_theme();

    /**
     * @brief Initialize dark theme colors
     */
    void init_dark_theme();

    /**
     * @brief Initialize high contrast theme colors
     */
    void init_high_contrast_theme();

    /**
     * @brief Apply ImGui style for specified colors
     */
    void apply_colors_to_imgui(const ThemeColors& colors);
};

}  // namespace gui::theme