#pragma once

#include <imgui.h>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <string>

namespace Amplitron {

// ============================================================
// AMPLITRON Design System
// Color palette extracted from icon.svg
// Warm vintage amp aesthetic: dark browns, golds, muted metals
// ============================================================

namespace Theme {

// --- Brand ---
constexpr const char* APP_NAME     = "Amplitron";
constexpr const char* WINDOW_TITLE = "Amplitron - Guitar Amp Simulator";

// --- Core Palette (from icon.svg) ---
// Cabinet / background tones
constexpr ImU32 BG_DARKEST     = IM_COL32(20, 18, 16, 255);    // #141210  app bg
constexpr ImU32 BG_DARK        = IM_COL32(28, 26, 24, 255);    // #1c1a18  cabinet body
constexpr ImU32 BG_PANEL       = IM_COL32(42, 38, 32, 255);    // #2a2620  top panel
constexpr ImU32 BG_SURFACE     = IM_COL32(50, 46, 38, 255);    // #322e26  elevated surfaces
constexpr ImU32 BG_GRILLE      = IM_COL32(26, 18, 16, 255);    // #1a1210  speaker grille

// Metal / borders
constexpr ImU32 BORDER_DARK    = IM_COL32(58, 54, 48, 255);    // #3a3630  subtle border
constexpr ImU32 BORDER_MID     = IM_COL32(68, 68, 68, 255);    // #444444  medium border
constexpr ImU32 BORDER_LIGHT   = IM_COL32(85, 85, 85, 255);    // #555555  bright border
constexpr ImU32 METAL_STRIP    = IM_COL32(61, 56, 48, 255);    // #3d3830  brushed metal

// Gold accent (brand color — knob pointers, highlights, brand text)
constexpr ImU32 ACCENT_GOLD    = IM_COL32(200, 168, 74, 255);  // #c8a84a
constexpr ImU32 ACCENT_GOLD_DIM= IM_COL32(160, 130, 55, 255);  // dimmed gold
constexpr ImU32 ACCENT_GOLD_HOT= IM_COL32(230, 200, 100, 255); // bright gold

// Power LED green
constexpr ImU32 LED_GREEN      = IM_COL32(45, 110, 45, 255);   // #2d6e2d
constexpr ImU32 LED_GREEN_GLOW = IM_COL32(127, 255, 127, 200); // #7fff7f

// Text
constexpr ImU32 TEXT_PRIMARY   = IM_COL32(220, 215, 205, 255);  // warm white
constexpr ImU32 TEXT_SECONDARY = IM_COL32(136, 136, 136, 255);  // #888
constexpr ImU32 TEXT_DIM       = IM_COL32(102, 102, 102, 255);  // #666
constexpr ImU32 TEXT_MUTED     = IM_COL32(80, 76, 68, 255);     // very dim

// Knobs (from icon: dark circles with gold pointers)
constexpr ImU32 KNOB_BG        = IM_COL32(26, 26, 26, 255);    // #1a1a1a
constexpr ImU32 KNOB_RING      = IM_COL32(34, 34, 34, 255);    // #222222
constexpr ImU32 KNOB_FACE      = IM_COL32(42, 42, 42, 255);    // #2a2a2a
constexpr ImU32 KNOB_HOVER     = IM_COL32(55, 52, 46, 255);    // warm hover
constexpr ImU32 KNOB_ACTIVE    = IM_COL32(68, 64, 56, 255);    // warm active
constexpr ImU32 KNOB_TRACK_OFF = IM_COL32(36, 34, 30, 255);    // unfilled arc

// Status
constexpr ImU32 STATUS_LIVE    = IM_COL32(50, 200, 80, 255);
constexpr ImU32 STATUS_STOPPED = IM_COL32(200, 50, 50, 255);
constexpr ImU32 STATUS_REC     = IM_COL32(255, 40, 40, 255);

// Meters
constexpr ImU32 METER_BG       = IM_COL32(26, 22, 18, 255);
constexpr ImU32 METER_GREEN    = IM_COL32(50, 180, 70, 255);
constexpr ImU32 METER_YELLOW   = IM_COL32(210, 170, 50, 255);
constexpr ImU32 METER_RED      = IM_COL32(220, 50, 40, 255);

// Signal chain
constexpr ImU32 CHAIN_LINE     = IM_COL32(58, 54, 48, 150);    // matches border
constexpr ImU32 CHAIN_JACK     = IM_COL32(160, 150, 130, 255); // warm metal
constexpr ImU32 CHAIN_DOT      = IM_COL32(120, 110, 95, 200);

// Pedal chrome
constexpr ImU32 PEDAL_SHADOW   = IM_COL32(0, 0, 0, 120);
constexpr ImU32 PEDAL_BORDER   = IM_COL32(70, 66, 58, 255);    // warm grey
constexpr ImU32 PEDAL_PLATE    = IM_COL32(46, 42, 36, 200);    // top plate
constexpr ImU32 SWITCH_BODY    = IM_COL32(38, 36, 32, 255);
constexpr ImU32 SWITCH_RING    = IM_COL32(68, 64, 56, 255);
constexpr ImU32 SWITCH_ACTIVE  = IM_COL32(72, 65, 42, 255);    // gold-tinted = enabled
constexpr ImU32 SWITCH_IDLE    = IM_COL32(28, 26, 22, 255);    // dark = bypassed
constexpr ImU32 LED_OFF        = IM_COL32(36, 34, 30, 255);
constexpr ImU32 PEDAL_BYPASS_OVERLAY = IM_COL32(0, 0, 0, 90);  // dim overlay when bypassed

// --- ImVec4 helpers (for ImGui style and TextColored) ---
inline ImVec4 Gold()          { return ImVec4(0.78f, 0.66f, 0.29f, 1.0f); }
inline ImVec4 GoldDim()       { return ImVec4(0.63f, 0.51f, 0.22f, 1.0f); }
inline ImVec4 GoldHot()       { return ImVec4(0.90f, 0.78f, 0.39f, 1.0f); }
inline ImVec4 TextPrimary()   { return ImVec4(0.86f, 0.84f, 0.80f, 1.0f); }
inline ImVec4 TextSecondary() { return ImVec4(0.53f, 0.53f, 0.53f, 1.0f); }
inline ImVec4 TextDim()       { return ImVec4(0.40f, 0.40f, 0.40f, 1.0f); }
inline ImVec4 Live()          { return ImVec4(0.20f, 0.78f, 0.31f, 1.0f); }
inline ImVec4 Stopped()       { return ImVec4(0.78f, 0.20f, 0.20f, 1.0f); }
inline ImVec4 RecBlink(float t) {
    float a = (std::sin(t * 4.0f) > 0.0f) ? 1.0f : 0.3f;
    return ImVec4(1.0f, 0.1f, 0.1f, a);
}

// --- Parameter value formatter ---
// Produces human-readable display strings with unit-aware formatting:
//   Hz  -> "440 Hz" or "1.5 kHz" (>=1000)
//   dB  -> "-6.0 dB"
//   %   -> "75%"
//   ms  -> "250 ms"
//   s   -> "1.20 s"
//   other / none -> "1.5" or "1.5 unit"
inline std::string formatParameterValue(float value, const std::string& unit) {
    char buf[64];
    if (unit == "Hz") {
        if (value >= 1000.0f)
            snprintf(buf, sizeof(buf), "%.1f kHz", value / 1000.0f);
        else
            snprintf(buf, sizeof(buf), "%.0f Hz", value);
    } else if (unit == "dB") {
        snprintf(buf, sizeof(buf), "%.1f dB", value);
    } else if (unit == "%" || unit == "pct") {
        snprintf(buf, sizeof(buf), "%.0f%%", value);
    } else if (unit == "ms") {
        snprintf(buf, sizeof(buf), "%.1f ms", value);
    } else if (unit == "s") {
        snprintf(buf, sizeof(buf), "%.2f s", value);
    } else if (!unit.empty()) {
        snprintf(buf, sizeof(buf), "%.1f %s", value, unit.c_str());
    } else {
        snprintf(buf, sizeof(buf), "%.1f", value);
    }
    return std::string(buf);
}

// --- Spacing / Layout ---
constexpr float PEDAL_WIDTH         = 190.0f;
constexpr float PEDAL_HEIGHT        = 360.0f;
constexpr float KNOB_RADIUS         = 20.0f;
constexpr float KNOB_HIT_MULT       = 2.2f;
constexpr float KNOB_SPACING_X      = 85.0f;   // horizontal distance between knob columns
constexpr float KNOB_SPACING_Y      = 95.0f;   // vertical distance between knob rows
constexpr float KNOB_Y_START        = 70.0f;   // knob area top offset from pedal top
constexpr float SWITCH_BOTTOM_OFFSET = 55.0f;  // footswitch distance from pedal bottom
constexpr float ROUNDING_SM   = 4.0f;
constexpr float ROUNDING_MD   = 8.0f;
constexpr float ROUNDING_LG   = 12.0f;

// --- Apply the full ImGui style ---
inline void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding
    style.WindowRounding    = ROUNDING_MD;
    style.ChildRounding     = ROUNDING_SM;
    style.FrameRounding     = ROUNDING_SM;
    style.PopupRounding     = ROUNDING_SM;
    style.ScrollbarRounding = ROUNDING_SM;
    style.GrabRounding      = ROUNDING_SM;
    style.TabRounding       = ROUNDING_SM;

    // Spacing
    style.WindowPadding  = ImVec2(8, 8);
    style.FramePadding   = ImVec2(6, 4);
    style.ItemSpacing    = ImVec2(8, 5);
    style.ScrollbarSize  = 12;
    style.GrabMinSize    = 10;

    // Borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize  = 1.0f;
    style.FrameBorderSize  = 0.0f;

    // Colors
    ImVec4* c = style.Colors;

    // Window
    c[ImGuiCol_WindowBg]       = ImVec4(0.11f, 0.10f, 0.09f, 1.00f);  // #1c1a18
    c[ImGuiCol_ChildBg]        = ImVec4(0.08f, 0.07f, 0.06f, 0.40f);
    c[ImGuiCol_PopupBg]        = ImVec4(0.14f, 0.13f, 0.11f, 0.95f);

    // Borders
    c[ImGuiCol_Border]         = ImVec4(0.23f, 0.21f, 0.19f, 0.60f);
    c[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Title / Menu
    c[ImGuiCol_TitleBg]        = ImVec4(0.12f, 0.11f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]  = ImVec4(0.16f, 0.15f, 0.13f, 1.00f);
    c[ImGuiCol_MenuBarBg]      = ImVec4(0.14f, 0.13f, 0.11f, 1.00f);

    // Text
    c[ImGuiCol_Text]           = TextPrimary();
    c[ImGuiCol_TextDisabled]   = TextDim();

    // Frame (input fields, sliders)
    c[ImGuiCol_FrameBg]        = ImVec4(0.16f, 0.15f, 0.13f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.20f, 0.17f, 1.00f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.27f, 0.25f, 0.22f, 1.00f);

    // Buttons — gold tinted
    c[ImGuiCol_Button]         = ImVec4(0.22f, 0.20f, 0.16f, 1.00f);
    c[ImGuiCol_ButtonHovered]  = ImVec4(0.35f, 0.30f, 0.18f, 1.00f);
    c[ImGuiCol_ButtonActive]   = ImVec4(0.50f, 0.42f, 0.20f, 1.00f);

    // Header / selectable
    c[ImGuiCol_Header]         = ImVec4(0.22f, 0.20f, 0.16f, 1.00f);
    c[ImGuiCol_HeaderHovered]  = ImVec4(0.35f, 0.30f, 0.18f, 1.00f);
    c[ImGuiCol_HeaderActive]   = ImVec4(0.50f, 0.42f, 0.20f, 1.00f);

    // Slider grab — gold
    c[ImGuiCol_SliderGrab]     = Gold();
    c[ImGuiCol_SliderGrabActive]= GoldHot();

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.08f, 0.07f, 0.06f, 0.50f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.25f, 0.23f, 0.20f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.30f, 0.25f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45f, 0.38f, 0.28f, 1.00f);

    // Checkbox / radio
    c[ImGuiCol_CheckMark]      = Gold();

    // Separator
    c[ImGuiCol_Separator]      = ImVec4(0.23f, 0.21f, 0.19f, 0.50f);
    c[ImGuiCol_SeparatorHovered]= ImVec4(0.50f, 0.42f, 0.20f, 0.70f);
    c[ImGuiCol_SeparatorActive]= ImVec4(0.78f, 0.66f, 0.29f, 1.00f);

    // Tab
    c[ImGuiCol_Tab]            = ImVec4(0.16f, 0.15f, 0.13f, 1.00f);
    c[ImGuiCol_TabHovered]     = ImVec4(0.35f, 0.30f, 0.18f, 1.00f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]         = ImVec4(0.50f, 0.42f, 0.20f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]  = ImVec4(0.50f, 0.42f, 0.20f, 0.60f);
    c[ImGuiCol_ResizeGripActive]   = ImVec4(0.78f, 0.66f, 0.29f, 0.90f);
}

} // namespace Theme

// --- Effect Color Scheme (SOLID: Open/Closed — extend by adding entries, not modifying) ---
struct EffectColorEntry {
    const char* name;
    ImVec4 pedal_color;
    ImVec4 led_color;
};

inline const EffectColorEntry* get_effect_color(const char* effect_name) {
    // Warm-toned pedal colors that complement the Amplitron palette
    static const EffectColorEntry table[] = {
        {"Distortion", ImVec4(0.45f, 0.14f, 0.10f, 1.0f), ImVec4(1.0f, 0.30f, 0.20f, 1.0f)},
        {"Overdrive",  ImVec4(0.45f, 0.34f, 0.12f, 1.0f), ImVec4(0.95f, 0.75f, 0.20f, 1.0f)},
        {"Delay",      ImVec4(0.12f, 0.22f, 0.38f, 1.0f), ImVec4(0.35f, 0.60f, 0.95f, 1.0f)},
        {"Reverb",     ImVec4(0.14f, 0.28f, 0.36f, 1.0f), ImVec4(0.25f, 0.72f, 0.85f, 1.0f)},
        {"Looper",     ImVec4(0.16f, 0.16f, 0.22f, 1.0f), ImVec4(0.78f, 0.66f, 0.29f, 1.0f)},
        {"Chorus",     ImVec4(0.26f, 0.14f, 0.38f, 1.0f), ImVec4(0.65f, 0.35f, 0.95f, 1.0f)},
        {"Phaser",     ImVec4(0.22f, 0.10f, 0.34f, 1.0f), ImVec4(0.80f, 0.25f, 0.90f, 1.0f)},
        {"Flanger",    ImVec4(0.14f, 0.20f, 0.36f, 1.0f), ImVec4(0.30f, 0.70f, 1.00f, 1.0f)},
        {"Equalizer",  ImVec4(0.14f, 0.32f, 0.18f, 1.0f), ImVec4(0.25f, 0.90f, 0.40f, 1.0f)},
        {"Noise Gate", ImVec4(0.20f, 0.19f, 0.22f, 1.0f), ImVec4(0.70f, 0.70f, 0.80f, 1.0f)},
        {"Compressor", ImVec4(0.34f, 0.26f, 0.14f, 1.0f), ImVec4(0.95f, 0.65f, 0.25f, 1.0f)},
        {"Cabinet",    ImVec4(0.26f, 0.18f, 0.10f, 1.0f), ImVec4(0.85f, 0.55f, 0.30f, 1.0f)},
        {"IR Cabinet", ImVec4(0.20f, 0.14f, 0.08f, 1.0f), ImVec4(0.80f, 0.50f, 0.25f, 1.0f)},
        {"Octaver",       ImVec4(0.36f, 0.14f, 0.28f, 1.0f), ImVec4(0.90f, 0.35f, 0.60f, 1.0f)},
        {"Pitch Shifter", ImVec4(0.32f, 0.12f, 0.32f, 1.0f), ImVec4(0.85f, 0.40f, 0.85f, 1.0f)},
        {"Tuner",      ImVec4(0.12f, 0.24f, 0.30f, 1.0f), ImVec4(0.40f, 0.85f, 0.95f, 1.0f)},
    };
    for (const auto& entry : table) {
        if (std::strcmp(effect_name, entry.name) == 0) return &entry;
    }
    // Default fallback
    static const EffectColorEntry fallback =
        {"Default", ImVec4(0.20f, 0.19f, 0.17f, 1.0f), ImVec4(0.78f, 0.66f, 0.29f, 1.0f)};
    return &fallback;
}

} // namespace Amplitron
