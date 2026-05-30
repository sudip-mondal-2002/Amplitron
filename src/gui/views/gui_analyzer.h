#pragma once

#include "audio/dsp/spectrum_analyzer.h"
#include "audio/dsp/level_analyzer.h"
#include "gui/ui_component.h"
#include <imgui.h>
#include <functional>

namespace Amplitron {

enum class SpectrumDisplayMode {
    Input   = 0,
    Output  = 1,
    Overlay = 2,
};

struct SpectrumSnapshot {
    std::array<float, SpectrumAnalyzer::DISPLAY_BARS> smoothed_input_db{};
    std::array<float, SpectrumAnalyzer::DISPLAY_BARS> smoothed_output_db{};
    std::array<float, SpectrumAnalyzer::DISPLAY_BARS> input_peak_db{};
    std::array<float, SpectrumAnalyzer::DISPLAY_BARS> output_peak_db{};
};

struct AnalyzerProps {
    // VU levels (pre-calculated by LevelAnalyzer in the audio engine)
    float smoothed_input_rms  = 0.0f;
    float smoothed_output_rms = 0.0f;
    float input_peak_hold     = 0.0f;
    float output_peak_hold    = 0.0f;
    bool  input_clip_active   = false;
    bool  output_clip_active  = false;
    float input_clip_flash    = 0.0f;
    float output_clip_flash   = 0.0f;

    // Spectrum analyzer (pre-calculated by SpectrumAnalyzer in the audio engine)
    SpectrumSnapshot spectrum;

    std::function<void(bool)>                on_expanded_changed;
    std::function<void(SpectrumDisplayMode)> on_mode_changed;
    std::function<void(bool)>                on_set_analyzer_enabled;
};

/**
 * @brief Reactive real-time analyzer panel component.
 *
 * Receives all pre-computed level and spectrum data via AnalyzerProps.
 * Zero math or signal processing occurs here — only ImGui drawing calls.
 */
class GuiAnalyzer : public UIComponent<AnalyzerProps> {
public:
    GuiAnalyzer() = default;

    /** @brief Render the collapsible analyzer panel. */
    void render() override;

    /** @brief Height to reserve for this panel in the parent layout. */
    float analyzer_reserved_height() const { return expanded_ ? 245.0f : 38.0f; }

    SpectrumDisplayMode current_mode() const { return mode_; }
    bool is_expanded() const { return expanded_; }

private:
    void render_vu_bar(const char* id,
                       const char* label,
                       float rms_level,
                       float peak_hold,
                       bool  clip_active,
                       float clip_flash,
                       ImU32 base_color,
                       ImU32 peak_color);

    void draw_spectrum(ImDrawList* dl,
                       const ImVec2& pos,
                       const ImVec2& size) const;

    bool expanded_ = true;
    SpectrumDisplayMode mode_ = SpectrumDisplayMode::Output;
};

} // namespace Amplitron
