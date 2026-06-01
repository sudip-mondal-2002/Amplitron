#include "gui/views/gui_analyzer.h"
#include "gui/theme/theme.h"
#include "common.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace Amplitron {

// Display axis constants (for drawing only — DSP uses its own internal values)
namespace {
constexpr float kDisplayMinHz =  20.0f;
constexpr float kDisplayMaxHz = 20000.0f;
constexpr float kDisplayMinDb = -90.0f;
constexpr float kDisplayMaxDb =   0.0f;

inline float hz_to_log_norm(float hz) {
    const float lo = std::log10(kDisplayMinHz);
    const float hi = std::log10(kDisplayMaxHz);
    return clamp((std::log10(hz) - lo) / (hi - lo), 0.0f, 1.0f);
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// VU bar — pure drawing, receives pre-computed scalars
// ─────────────────────────────────────────────────────────────────────────────
void GuiAnalyzer::render_vu_bar(const char* id,
                                const char* label,
                                float rms_level,
                                float peak_hold,
                                bool  clip_active,
                                float clip_flash,
                                ImU32 base_color,
                                ImU32 peak_color) {
    ImGui::PushID(id);
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    float db_value = (rms_level > 0.0001f) ? (20.0f * std::log10(rms_level)) : -96.0f;
    ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.80f, 1.0f), "%.1f dB", db_value);

    ImVec2 pos    = ImGui::GetCursorScreenPos();
    float  width  = ImGui::GetContentRegionAvail().x;
    float  height = 18.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg_col = Theme::METER_BG;
    if (clip_active || clip_flash > 0.01f) {
        const float flash = clamp(clip_flash, 0.0f, 1.0f);
        int alpha = static_cast<int>(90.0f + flash * 130.0f);
        bg_col = IM_COL32(180, 30, 30, alpha);
    }

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bg_col, Theme::ROUNDING_SM);

    float rms_fill = clamp(rms_level, 0.0f, 1.0f) * width;
    dl->AddRectFilled(pos, ImVec2(pos.x + rms_fill, pos.y + height), base_color, Theme::ROUNDING_SM);

    float peak_x = pos.x + clamp(peak_hold, 0.0f, 1.0f) * width;
    dl->AddLine(ImVec2(peak_x, pos.y - 1.0f), ImVec2(peak_x, pos.y + height + 1.0f), peak_color, 2.0f);

    if (clip_active || clip_flash > 0.01f)
        dl->AddText(ImVec2(pos.x + width - 32.0f, pos.y - 1.0f), IM_COL32(255, 90, 90, 255), "CLIP");

    ImGui::Dummy(ImVec2(width, height + 6.0f));
    ImGui::PopID();
}

// ─────────────────────────────────────────────────────────────────────────────
// Spectrum — pure drawing from pre-computed SpectrumAnalyzer arrays
// ─────────────────────────────────────────────────────────────────────────────
void GuiAnalyzer::draw_spectrum(ImDrawList* dl,
                                const ImVec2& pos,
                                const ImVec2& size) const {
    if (size.x <= 2.0f || size.y <= 2.0f) return;

    const auto& sa = props_.spectrum;
    const ImVec2 pmax(pos.x + size.x, pos.y + size.y);

    dl->AddRect(pos, pmax, IM_COL32(72, 78, 92, 220), Theme::ROUNDING_SM);

    // Reference dB lines
    const float ref_lines[] = {-60.0f, -48.0f, -36.0f, -24.0f, -12.0f};
    for (float db : ref_lines) {
        float t = (db - kDisplayMinDb) / (kDisplayMaxDb - kDisplayMinDb);
        float y = pmax.y - t * size.y;
        dl->AddLine(ImVec2(pos.x, y), ImVec2(pmax.x, y), IM_COL32(58, 64, 76, 180), 1.0f);
    }

    constexpr int   BARS       = SpectrumAnalyzer::DISPLAY_BARS;
    const ImU32 input_col  = IM_COL32(82, 220, 135, 220);
    const ImU32 output_col = IM_COL32(92, 170, 255, 220);
    const ImU32 peak_col   = IM_COL32(255, 240, 165, 255);

    const auto draw_set = [&](const std::array<float, BARS>& bars,
                               const std::array<float, BARS>& peaks,
                               ImU32 bar_col,
                               float width_scale) {
        for (int i = 0; i < BARS; ++i) {
            const float x0 = pos.x + (static_cast<float>(i)     / BARS) * size.x;
            const float x1 = pos.x + (static_cast<float>(i + 1) / BARS) * size.x;
            const float db = clamp(bars[i], kDisplayMinDb, kDisplayMaxDb);
            const float t  = (db - kDisplayMinDb) / (kDisplayMaxDb - kDisplayMinDb);
            const float y  = pmax.y - t * size.y;

            const float center = (x0 + x1) * 0.5f;
            const float half   = (x1 - x0) * 0.5f * width_scale;
            dl->AddRectFilled(ImVec2(center - half, y), ImVec2(center + half, pmax.y), bar_col, 1.5f);

            const float peak_t = (clamp(peaks[i], kDisplayMinDb, kDisplayMaxDb) - kDisplayMinDb) / (kDisplayMaxDb - kDisplayMinDb);
            const float py     = pmax.y - peak_t * size.y;
            dl->AddLine(ImVec2(center - half, py), ImVec2(center + half, py), peak_col, 1.0f);
        }
    };

    switch (mode_) {
        case SpectrumDisplayMode::Input:
            draw_set(sa.smoothed_input_db, sa.input_peak_db, input_col, 0.82f);
            break;
        case SpectrumDisplayMode::Output:
            draw_set(sa.smoothed_output_db, sa.output_peak_db, output_col, 0.82f);
            break;
        case SpectrumDisplayMode::Overlay:
            draw_set(sa.smoothed_input_db,  sa.input_peak_db,  input_col,  0.42f);
            draw_set(sa.smoothed_output_db, sa.output_peak_db, output_col, 0.42f);
            break;
    }

    // Frequency tick lines
    const float ticks[] = {20.0f, 100.0f, 1000.0f, 5000.0f, 10000.0f, 20000.0f};
    for (float hz : ticks) {
        float x = pos.x + hz_to_log_norm(hz) * size.x;
        dl->AddLine(ImVec2(x, pos.y), ImVec2(x, pmax.y), IM_COL32(52, 58, 72, 180), 1.0f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main panel render
// ─────────────────────────────────────────────────────────────────────────────
void GuiAnalyzer::render() {
    const AnalyzerProps& p = props_;

    float panel_h = expanded_ ? 230.0f : 34.0f;
    ImGui::BeginChild("AnalyzerPanel", ImVec2(0, panel_h), true, ImGuiWindowFlags_NoScrollbar);

    const bool expanded = ImGui::CollapsingHeader("Real-Time Analyzer", ImGuiTreeNodeFlags_DefaultOpen);
    if (expanded != expanded_) {
        expanded_ = expanded;
        if (p.on_expanded_changed) p.on_expanded_changed(expanded_);
        if (p.on_set_analyzer_enabled) p.on_set_analyzer_enabled(expanded_);
    }

    if (!expanded_) {
        ImGui::EndChild();
        return;
    }

    // ── Mode selector ──
    int mode_index = static_cast<int>(mode_);
    ImGui::TextUnformatted("Spectrum:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo("##AnalyzerMode", &mode_index, "Input\0Output\0Overlay\0")) {
        mode_ = static_cast<SpectrumDisplayMode>(mode_index);
        if (p.on_mode_changed) p.on_mode_changed(mode_);
    }

    // ── VU bars (pre-calculated values from props) ──
    ImGui::Columns(2, "analyzer_vu_cols", false);
    render_vu_bar("input_vu",  "INPUT RMS",
                  p.smoothed_input_rms,  p.input_peak_hold,
                  p.input_clip_active,   p.input_clip_flash,
                  IM_COL32(60, 200, 110, 230), IM_COL32(255, 230, 120, 255));
    ImGui::NextColumn();
    render_vu_bar("output_vu", "OUTPUT RMS",
                  p.smoothed_output_rms, p.output_peak_hold,
                  p.output_clip_active,  p.output_clip_flash,
                  IM_COL32(80, 170, 245, 230), IM_COL32(255, 230, 120, 255));
    ImGui::Columns(1);

    // ── Spectrum plot ──
    ImVec2 plot_pos(ImGui::GetCursorScreenPos());
    ImVec2 plot_size(ImGui::GetContentRegionAvail().x, 112.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(plot_pos,
                      ImVec2(plot_pos.x + plot_size.x, plot_pos.y + plot_size.y),
                      IM_COL32(20, 22, 28, 255), Theme::ROUNDING_SM);

    draw_spectrum(dl, plot_pos, plot_size);
    ImGui::Dummy(plot_size);

    // ── Frequency axis labels ──
    const float axis_left = ImGui::GetCursorPosX();
    const float axis_w    = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(Theme::TextSecondary(), "20 Hz");
    ImGui::SameLine(axis_left + axis_w * 0.48f);
    ImGui::TextColored(Theme::TextSecondary(), "1 kHz");
    ImGui::SameLine(axis_left + axis_w - 52.0f);
    ImGui::TextColored(Theme::TextSecondary(), "20 kHz");

    ImGui::EndChild();
}

} // namespace Amplitron
