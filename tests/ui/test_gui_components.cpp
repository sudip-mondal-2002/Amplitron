/**
 * @file test_gui_components.cpp
 * @brief Headless-safe tests for UI components (Theme, EffectColor, GuiSettings).
 *
 * Tests unit-aware parameter formatting, effect color lookups, style applications, and basic
 * settings class rendering using a software ImGui context.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include <string>
#include "gui/theme/theme.h"
#include "gui/dialogs/file_dialog.h"

#include "audio/engine/audio_engine.h"
#include "gui/views/gui_settings.h"
#include "gui/views/gui_analyzer.h"

using namespace Amplitron;

// ============================================================
// Theme Parameter Value Formatting
// ============================================================

TEST(theme_format_parameter_value_hz_low) {
    std::string formatted = Theme::formatParameterValue(440.0f, "Hz");
    ASSERT_EQ(formatted, "440 Hz");
}

TEST(theme_format_parameter_value_hz_high) {
    std::string formatted = Theme::formatParameterValue(1500.0f, "Hz");
    ASSERT_EQ(formatted, "1.5 kHz");
}

// ============================================================
// Theme Parameter Value Formatting (Remainder)
// ============================================================

TEST(theme_format_parameter_value_db) {
    std::string formatted = Theme::formatParameterValue(-6.0f, "dB");
    ASSERT_EQ(formatted, "-6.0 dB");
}

TEST(theme_format_parameter_value_percent) {
    std::string formatted = Theme::formatParameterValue(75.2f, "%");
    ASSERT_EQ(formatted, "75%");
}

TEST(theme_format_parameter_value_pct) {
    std::string formatted = Theme::formatParameterValue(50.8f, "pct");
    ASSERT_EQ(formatted, "51%");
}

TEST(theme_format_parameter_value_ms) {
    std::string formatted = Theme::formatParameterValue(250.0f, "ms");
    ASSERT_EQ(formatted, "250.0 ms");
}

TEST(theme_format_parameter_value_seconds) {
    std::string formatted = Theme::formatParameterValue(1.234f, "s");
    ASSERT_EQ(formatted, "1.23 s");
}

TEST(theme_format_parameter_value_custom_unit) {
    std::string formatted = Theme::formatParameterValue(3.5f, "oct");
    ASSERT_EQ(formatted, "3.5 oct");
}

TEST(theme_format_parameter_value_empty_unit) {
    std::string formatted = Theme::formatParameterValue(5.0f, "");
    ASSERT_EQ(formatted, "5.0");
}

// ============================================================
// Theme RecBlink Oscillation
// ============================================================

TEST(theme_rec_blink_values) {
    ImVec4 col_on = Theme::RecBlink(0.2f);
    ASSERT_EQ(col_on.w, 1.0f);

    ImVec4 col_off = Theme::RecBlink(1.0f);
    ASSERT_EQ(col_off.w, 0.3f);
}

// ============================================================
// get_effect_color Table Lookups
// ============================================================

TEST(get_effect_color_known_effects) {
    const char* effects[] = {
        "Distortion", "Overdrive", "Delay", "Reverb", "Looper",
        "Chorus", "Phaser", "Flanger", "Equalizer", "Noise Gate",
        "Compressor", "MultiBand Compressor", "Cabinet", "Octaver",
        "Pitch Shifter", "Tuner"
    };

    for (const char* name : effects) {
        const EffectColorEntry* entry = get_effect_color(name);
        ASSERT_EQ(std::string(entry->name), std::string(name));
    }
}

// ============================================================
// Theme Style Application
// ============================================================

TEST_F(PresetTest, theme_apply_style) {
    ScopedImGuiContext imgui;
    Theme::ApplyStyle();
}

TEST(get_effect_color_fallback) {
    const EffectColorEntry* entry = get_effect_color("NonExistentFX");
    ASSERT_EQ(std::string(entry->name), std::string("Default"));
}

// ============================================================
// GuiSettings Construction and Rendering
// ============================================================

TEST_F(PresetTest, gui_settings_construction_no_crash) {
    GuiSettings settings;
    (void)settings;
}

TEST_F(PresetTest, gui_settings_render) {
    ScopedImGuiContext imgui;
    GuiSettings settings;

    SettingsProps props;
    props.device_error = "Simulated device error";
    props.cpu_load = 0.95f;
    props.buffer_size = 128;
    props.suggested_buf = 256;
    props.auto_buf = false;

    settings.set_props(props);

    bool show = true;
    settings.render(show);

    // Test with auto-buffer enabled
    props.auto_buf = true;
    settings.set_props(props);
    settings.render(show);
}

// ============================================================
// GuiAnalyzer Construction and Rendering
// ============================================================

TEST_F(PresetTest, gui_analyzer_render) {
    ScopedImGuiContext imgui;
    GuiAnalyzer analyzer;

    ASSERT_TRUE(analyzer.is_expanded());
    ASSERT_NEAR(analyzer.analyzer_reserved_height(), 245.0f, 0.01f);

    SpectrumAnalyzer sa;
    AnalyzerProps props;
    props.smoothed_input_rms = 0.1f;
    props.smoothed_output_rms = 0.2f;
    props.spectrum.smoothed_input_db  = sa.smoothed_input_db();
    props.spectrum.smoothed_output_db = sa.smoothed_output_db();
    props.spectrum.input_peak_db      = sa.input_peak_db();
    props.spectrum.output_peak_db     = sa.output_peak_db();

    analyzer.set_props(props);

    // Renders input VU, output VU, and spectrum plotter
    analyzer.render();
}

// ============================================================
// File Dialog Native Headless Safe Paths
// ============================================================

TEST(file_dialog_native_open_and_folder_headless) {
    std::string opened = show_open_dialog("Select IR File", "", "wav");
    ASSERT_EQ(opened, "");

    std::string folder = show_folder_dialog("Select Directory");
    ASSERT_EQ(folder, "");

    std::string saved = show_save_dialog("MyPreset", "Preset Files", "json");
    ASSERT_EQ(saved, "");
}
