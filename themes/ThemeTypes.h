#pragma once

#include <string>
#include <map>
#include <imgui.h>

namespace gui::theme {

/**
 * @enum ThemeType
 * @brief Enumeration of available theme modes
 */
enum class ThemeType {
    LIGHT,           ///< Light theme - bright interface for daytime use
    DARK,            ///< Dark theme - dark backgrounds for low-light environments
    HIGH_CONTRAST,   ///< High contrast theme - maximum contrast for accessibility
    COUNT            ///< Total number of themes
};

/**
 * @struct ThemeColors
 * @brief Centralized theme color palette and tokens
 */
struct ThemeColors {
    // Primary Colors
    ImVec4 primary;
    ImVec4 primary_light;
    ImVec4 primary_dark;

    // Secondary Colors
    ImVec4 secondary;
    ImVec4 secondary_light;
    ImVec4 secondary_dark;

    // Background Colors
    ImVec4 bg_main;          ///< Main background
    ImVec4 bg_secondary;     ///< Secondary background (panels, windows)
    ImVec4 bg_tertiary;      ///< Tertiary background (nested elements)
    ImVec4 bg_hover;         ///< Hover state background

    // Text Colors
    ImVec4 text_primary;     ///< Primary text color
    ImVec4 text_secondary;   ///< Secondary/muted text color
    ImVec4 text_disabled;    ///< Disabled text color
    ImVec4 text_inverse;     ///< Inverse text (on colored backgrounds)

    // Border & Edge Colors
    ImVec4 border;           ///< Border color
    ImVec4 border_light;     ///< Light border
    ImVec4 border_dark;      ///< Dark border

    // Status Colors
    ImVec4 success;          ///< Success state (green)
    ImVec4 warning;          ///< Warning state (yellow/orange)
    ImVec4 error;            ///< Error state (red)
    ImVec4 info;             ///< Info state (blue)

    // Interactive Elements
    ImVec4 button_bg;        ///< Button background
    ImVec4 button_hover;     ///< Button hover state
    ImVec4 button_active;    ///< Button active/pressed state
    ImVec4 button_text;      ///< Button text color

    // Pedal Board Specific
    ImVec4 pedal_enabled;    ///< Enabled pedal color
    ImVec4 pedal_disabled;   ///< Bypassed/disabled pedal color
    ImVec4 pedal_highlight;  ///< Highlighted/selected pedal

    // Special Effects
    ImVec4 knob_fg;          ///< Knob foreground
    ImVec4 knob_bg;          ///< Knob background
    ImVec4 led_on;           ///< LED indicator (on)
    ImVec4 led_off;          ///< LED indicator (off)
};

/**
 * @struct ThemeMetadata
 * @brief Metadata and display information for themes
 */
struct ThemeMetadata {
    ThemeType type;
    std::string name;
    std::string display_name;
    std::string description;
};

}  // namespace gui::theme