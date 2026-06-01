#include "test_framework.h"
#include "gui/theme/theme.h"
#include <cstring>

using namespace Amplitron;

// ============================================================
// Theme / Design System tests
// ============================================================

TEST(theme_app_name_not_empty) {
    ASSERT_TRUE(std::strlen(Theme::APP_NAME) > 0);
    ASSERT_TRUE(std::strlen(Theme::WINDOW_TITLE) > 0);
}

TEST(theme_app_name_is_amplitron) {
    ASSERT_TRUE(std::strcmp(Theme::APP_NAME, "Amplitron") == 0);
}

TEST(theme_gold_color_valid) {
    ImVec4 gold = Theme::Gold();
    ASSERT_GT(gold.x, 0.5f);   // R > 0.5
    ASSERT_GT(gold.y, 0.3f);   // G > 0.3
    ASSERT_LT(gold.z, 0.5f);   // B < 0.5 (gold is warm)
    ASSERT_NEAR(gold.w, 1.0f, 1e-6f);
}

TEST(theme_text_primary_is_light) {
    ImVec4 tp = Theme::TextPrimary();
    // Primary text should be light (readable on dark bg)
    ASSERT_GT(tp.x, 0.7f);
    ASSERT_GT(tp.y, 0.7f);
    ASSERT_GT(tp.z, 0.7f);
}

TEST(theme_text_dim_is_darker_than_primary) {
    ImVec4 tp = Theme::TextPrimary();
    ImVec4 td = Theme::TextDim();
    float lum_primary = tp.x + tp.y + tp.z;
    float lum_dim = td.x + td.y + td.z;
    ASSERT_GT(lum_primary, lum_dim);
}

TEST(theme_rec_blink_returns_valid_color) {
    ImVec4 c1 = Theme::RecBlink(0.0f);
    ImVec4 c2 = Theme::RecBlink(1.0f);
    // Should always be reddish
    ASSERT_GT(c1.x, 0.8f);
    ASSERT_GT(c2.x, 0.8f);
    // Alpha should be positive
    ASSERT_GT(c1.w, 0.0f);
    ASSERT_GT(c2.w, 0.0f);
}

TEST(theme_layout_constants_positive) {
    ASSERT_GT(Theme::PEDAL_WIDTH, 0.0f);
    ASSERT_GT(Theme::PEDAL_HEIGHT, 0.0f);
    ASSERT_GT(Theme::KNOB_RADIUS, 0.0f);
    ASSERT_GT(Theme::ROUNDING_SM, 0.0f);
    ASSERT_GT(Theme::ROUNDING_MD, 0.0f);
    ASSERT_GT(Theme::ROUNDING_LG, 0.0f);
    ASSERT_GT(Theme::ROUNDING_LG, Theme::ROUNDING_SM);
}

// ============================================================
// Effect color lookup tests (SOLID: Open/Closed)
// ============================================================

TEST(effect_color_lookup_known_effects) {
    const char* known[] = {
        "Distortion", "Overdrive", "Delay", "Reverb",
        "Chorus", "Equalizer", "Noise Gate", "Compressor", "Cabinet"
    };

    for (const char* name : known) {
        const auto* entry = get_effect_color(name);
        ASSERT_TRUE(entry != nullptr);
        ASSERT_TRUE(std::strcmp(entry->name, name) == 0);
        // Colors should have positive RGB
        ASSERT_GT(entry->pedal_color.x + entry->pedal_color.y + entry->pedal_color.z, 0.0f);
        ASSERT_GT(entry->led_color.x + entry->led_color.y + entry->led_color.z, 0.0f);
    }
}

TEST(effect_color_lookup_unknown_returns_fallback) {
    const auto* entry = get_effect_color("NonexistentEffect");
    ASSERT_TRUE(entry != nullptr);
    ASSERT_TRUE(std::strcmp(entry->name, "Default") == 0);
}

TEST(effect_colors_are_distinct) {
    const auto* dist = get_effect_color("Distortion");
    const auto* od = get_effect_color("Overdrive");
    const auto* rv = get_effect_color("Reverb");

    // Pedal colors should be different from each other
    bool dist_od_same = (std::fabs(dist->pedal_color.x - od->pedal_color.x) < 0.01f &&
                         std::fabs(dist->pedal_color.y - od->pedal_color.y) < 0.01f &&
                         std::fabs(dist->pedal_color.z - od->pedal_color.z) < 0.01f);
    ASSERT_FALSE(dist_od_same);

    bool dist_rv_same = (std::fabs(dist->pedal_color.x - rv->pedal_color.x) < 0.01f &&
                         std::fabs(dist->pedal_color.y - rv->pedal_color.y) < 0.01f &&
                         std::fabs(dist->pedal_color.z - rv->pedal_color.z) < 0.01f);
    ASSERT_FALSE(dist_rv_same);
}
