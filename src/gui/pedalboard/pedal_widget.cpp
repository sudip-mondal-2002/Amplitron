#include "gui/pedalboard/pedal_widget.h"

#include <cmath>
#include <cstring>

#include "audio/effects/amp_cab/amp_simulator.h"
#include "audio/effects/utility/tuner.h"
#include "audio/engine/audio_engine.h"
#include "gui/commands/command.h"
#include "gui/commands/command_history.h"
#include "gui/components/footswitch.h"
#include "gui/components/led.h"
#include "gui/components/screen.h"
#include "gui/dialogs/file_dialog.h"
#include "gui/theme/theme.h"
#include "gui/views/gui_midi.h"

namespace Amplitron {

/** @brief Construct PedalWidget and look up color scheme for the effect type.
 */
PedalWidget::PedalWidget(IAudioEngine& engine, std::shared_ptr<Effect> effect, int index)
    : engine_(engine), effect_(std::move(effect)), index_(index) {
    assign_colors();
}

PedalWidget::~PedalWidget() {
    if (analyzer_open_) {
        engine_.unregister_pedal_analyzer(index_);
    }
}

/** @brief Look up pedal_color_ and led_color_ from the theme's effect color
 * table. */
void PedalWidget::assign_colors() {
    const auto* entry = get_effect_color(effect_->name());
    pedal_color_ = entry->pedal_color;
    led_color_ = entry->led_color;
}

/** @brief Render the full pedal widget (body, knobs, switch, LED). @return true
 * if remove requested. */
bool PedalWidget::render(float zoom) {
    bool should_remove = false;

    ImGui::PushID(index_);

    bool is_amp = (std::strcmp(effect_->name(), "Amp Sim") == 0);
    bool is_mb_comp = (std::strcmp(effect_->name(), "MultiBand Compressor") == 0);
    bool enabled = effect_->is_enabled();
    bool is_looper = !is_amp && (std::strcmp(effect_->name(), "Looper") == 0);

    float pedal_width =
        is_mb_comp ? (Theme::PEDAL_WIDTH * 2.2f * zoom) : (Theme::PEDAL_WIDTH * zoom);
    float pedal_height = Theme::PEDAL_HEIGHT * zoom;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Pedal body
    ImVec2 p0 = cursor;
    ImVec2 p1 = ImVec2(cursor.x + pedal_width, cursor.y + pedal_height);

    // Shadow
    dl->AddRectFilled(ImVec2(p0.x + 4 * zoom, p0.y + 4 * zoom),
                      ImVec2(p1.x + 4 * zoom, p1.y + 4 * zoom), Theme::PEDAL_SHADOW,
                      Theme::ROUNDING_MD * zoom);

    if (is_amp) {
        render_amp_cabinet(dl, p0, p1, pedal_width, pedal_height, zoom);
    } else {
        render_standard_pedal(dl, p0, p1, pedal_width, enabled, zoom);
    }

    // Dim the pedal body when bypassed so the inactive state is immediately
    // obvious
    if (!enabled && !is_amp) {
        dl->AddRectFilled(p0, p1, Theme::PEDAL_BYPASS_OVERLAY, Theme::ROUNDING_MD * zoom);
    }

    // --- Spectrum analyzer button ---
    float btn_x = p0.x + pedal_width - 50.0f * zoom;
    float btn_y = is_amp ? (p0.y + 16.0f * zoom) : (p0.y + 12.0f * zoom);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f * zoom);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f * zoom, 2.0f * zoom));

    ImVec4 btn_col =
        analyzer_open_ ? ImVec4(0.16f, 0.66f, 0.4f, 0.6f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));

    ImGui::SetCursorScreenPos(ImVec2(btn_x, btn_y));
    char btn_id[64];
    std::snprintf(btn_id, sizeof(btn_id), "##spec_%d", index_);

    ImGui::SetNextItemAllowOverlap();
    if (ImGui::Button(btn_id, ImVec2(20.0f * zoom, 20.0f * zoom))) {
        if (analyzer_open_) {
            engine_.unregister_pedal_analyzer(index_);
            analyzer_open_ = false;
        } else {
            if (engine_.register_pedal_analyzer(index_)) {
                analyzer_open_ = true;
                analyzer_last_sequence_ = 0;
            }
        }
    }

    // --- Draw custom spectrum analyzer icon (sliders) ---
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float cx = min.x + (max.x - min.x) * 0.5f;
    float cy = min.y + (max.y - min.y) * 0.5f;
    float line_w = 2.0f * zoom;
    float line_h = 12.0f * zoom;
    float bar_w = 6.0f * zoom;
    float bar_h = 2.0f * zoom;
    float spacing = 3.0f * zoom;

    ImU32 icon_col = ImGui::GetColorU32(ImGuiCol_Text);

    // Left slider vertical line
    dl->AddRectFilled(ImVec2(cx - spacing - line_w * 0.5f, cy - line_h * 0.5f),
                      ImVec2(cx - spacing + line_w * 0.5f, cy + line_h * 0.5f), icon_col);
    // Left slider horizontal bar
    dl->AddRectFilled(ImVec2(cx - spacing - bar_w * 0.5f, cy + spacing - bar_h * 0.5f),
                      ImVec2(cx - spacing + bar_w * 0.5f, cy + spacing + bar_h * 0.5f), icon_col);

    // Right slider vertical line
    dl->AddRectFilled(ImVec2(cx + spacing - line_w * 0.5f, cy - line_h * 0.5f),
                      ImVec2(cx + spacing + line_w * 0.5f, cy + line_h * 0.5f), icon_col);
    // Right slider horizontal bar
    dl->AddRectFilled(ImVec2(cx + spacing - bar_w * 0.5f, cy - spacing - bar_h * 0.5f),
                      ImVec2(cx + spacing + bar_w * 0.5f, cy - spacing + bar_h * 0.5f), icon_col);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle pedal spectrum analyzer");
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    // --- Tuner custom display ---
    bool is_tuner = !is_amp && (std::strcmp(effect_->name(), "Tuner") == 0);
    if (is_tuner) {
        ScreenProps props;
        props.type = ScreenType::Tuner;
        props.effect = effect_;
        props.index = index_;
        props.engine = &engine_;
        props.gui_midi = gui_midi_;
        props.on_commit_param_change = [this](int pi, float old_val, float new_val) {
            commit_param_change(pi, old_val, new_val);
        };
        ScreenComponent::render(dl, p0, pedal_width, zoom, props);
    }

    // --- IR Cabinet custom display ---
    bool is_ir_cab = !is_amp && (std::strcmp(effect_->name(), "Cabinet") == 0);
    if (is_ir_cab) {
        ScreenProps props;
        props.type = ScreenType::Cabinet;
        props.effect = effect_;
        props.index = index_;
        props.engine = &engine_;
        props.gui_midi = gui_midi_;
        props.on_commit_param_change = [this](int pi, float old_val, float new_val) {
            commit_param_change(pi, old_val, new_val);
        };
        ScreenComponent::render(dl, p0, pedal_width, zoom, props);
    }

    // --- NAM Loader custom display ---
    bool is_nam_loader = !is_amp && (std::strcmp(effect_->name(), "NAM Loader") == 0);
    if (is_nam_loader) {
        render_nam_loader_display(p0, pedal_width, zoom);
    }

    if (is_looper) {
        ScreenProps props;
        props.type = ScreenType::Looper;
        props.effect = effect_;
        props.index = index_;
        props.engine = &engine_;
        props.gui_midi = gui_midi_;
        props.on_commit_param_change = [this](int pi, float old_val, float new_val) {
            commit_param_change(pi, old_val, new_val);
        };
        ScreenComponent::render(dl, p0, pedal_width, zoom, props);
    } else if (is_mb_comp) {
        ScreenProps props;
        props.type = ScreenType::MultiBandCompressor;
        props.effect = effect_;
        props.index = index_;
        props.engine = &engine_;
        props.gui_midi = gui_midi_;
        props.on_commit_param_change = [this](int pi, float old_val, float new_val) {
            commit_param_change(pi, old_val, new_val);
        };
        ScreenComponent::render(dl, p0, pedal_width, zoom, props);
    } else {
        render_knobs(dl, p0, pedal_width, is_amp, is_tuner, is_ir_cab || is_nam_loader, zoom);
    }

    render_footswitch_and_extras(dl, p0, p1, pedal_width, pedal_height, is_amp, enabled,
                                 should_remove, zoom);

    if (analyzer_open_) {
        render_spectrum_overlay(dl, p0, pedal_width, zoom);
    }

    ImGui::PopID();
    return should_remove;
}

void PedalWidget::render_standard_pedal(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width,
                                        bool enabled, float zoom) {
    // ========== STANDARD PEDAL VISUAL ==========
    ImU32 body_color = ImGui::ColorConvertFloat4ToU32(pedal_color_);
    dl->AddRectFilled(p0, p1, body_color, Theme::ROUNDING_MD * zoom);
    dl->AddRect(p0, p1, Theme::PEDAL_BORDER, Theme::ROUNDING_MD * zoom, 0, 2.0f * zoom);

    // Metallic top plate
    ImVec2 plate_p0 = ImVec2(p0.x + 8 * zoom, p0.y + 8 * zoom);
    ImVec2 plate_p1 = ImVec2(p1.x - 8 * zoom, p0.y + 45 * zoom);
    dl->AddRectFilled(plate_p0, plate_p1, Theme::PEDAL_PLATE, Theme::ROUNDING_SM * zoom);

    // Effect name
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 12 * zoom, p0.y + 14 * zoom));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextPrimary());
    ImGui::Text("%s", effect_->name());
    ImGui::PopStyleColor();

    // Reusable status LED indicator
    float led_x = p0.x + pedal_width - 25 * zoom;
    float led_y = p0.y + 20 * zoom;

    LedProps led_props;
    led_props.enabled = enabled;
    led_props.led_color = led_color_;
    led_props.tooltip = enabled ? "Effect active" : "Effect bypassed";

    char led_id[64];
    std::snprintf(led_id, sizeof(led_id), "##led_%d", index_);
    LedComponent::render(led_id, led_props, zoom, ImVec2(led_x, led_y));
}

void PedalWidget::render_footswitch_and_extras(ImDrawList* dl, ImVec2 p0, ImVec2 p1,
                                               float pedal_width, float pedal_height, bool is_amp,
                                               bool enabled, bool& should_remove, float zoom) {
    (void)p1;
    (void)should_remove;
    (void)dl;
    bool is_looper = !is_amp && (std::strcmp(effect_->name(), "Looper") == 0);

    // Footswitch (toggle on/off) — amps are always on, no footswitch
    if (!is_amp && !is_looper) {
        float switch_y = p0.y + pedal_height - Theme::SWITCH_BOTTOM_OFFSET * zoom;
        float switch_x = p0.x + (pedal_width - 50 * zoom) / 2;
        ImVec2 sw_center = ImVec2(switch_x + 25 * zoom, switch_y + 15 * zoom);

        FootswitchProps fs_props;
        fs_props.enabled = enabled;
        fs_props.tooltip_prefix = "";
        fs_props.on_clicked = [this, enabled]() {
            bool new_enabled = !enabled;
            effect_->set_enabled(new_enabled);
            engine_.push_effect_enabled(index_, new_enabled ? 1.0f : 0.0f);
        };

        char sw_id[64];
        std::snprintf(sw_id, sizeof(sw_id), "##switch_%d", index_);
        FootswitchComponent::render(sw_id, fs_props, zoom, sw_center);
    }
}

void PedalWidget::commit_param_change(int param_index, float old_val, float new_val) {
    if (!history_) return;
    auto cmd =
        std::make_unique<ParameterChangeCommand>(engine_, effect_, param_index, old_val, new_val);
    history_->push_executed(std::move(cmd));
}

void PedalWidget::render_spectrum_overlay(ImDrawList* dl, ImVec2 pedal_pos, float pedal_width,
                                          float zoom) {
    // 1. Update/poll spectrum data
    uint64_t seq = engine_.get_pedal_analyzer_sequence(index_);
    float dt = std::max(ImGui::GetIO().DeltaTime, 1.0f / 240.0f);
    if (seq != analyzer_last_sequence_) {
        if (engine_.copy_pedal_analyzer_snapshot(index_, analyzer_input_buf_.data(),
                                                 analyzer_output_buf_.data(),
                                                 SpectrumAnalyzer::FFT_SIZE)) {
            spectrum_analyzer_.update(analyzer_input_buf_.data(), analyzer_output_buf_.data(),
                                      engine_.get_sample_rate(), dt);
            analyzer_last_sequence_ = seq;
        }
    } else {
        spectrum_analyzer_.update(analyzer_input_buf_.data(), analyzer_output_buf_.data(),
                                  engine_.get_sample_rate(), dt);
    }

    // 2. Draw overlay floating right above the pedal header!
    ImVec2 overlay_size(pedal_width, 100.0f * zoom);
    ImVec2 overlay_pos(pedal_pos.x, pedal_pos.y - overlay_size.y - 8.0f * zoom);

    // Draw background
    dl->AddRectFilled(overlay_pos,
                      ImVec2(overlay_pos.x + overlay_size.x, overlay_pos.y + overlay_size.y),
                      IM_COL32(15, 16, 20, 240), Theme::ROUNDING_SM * zoom);
    dl->AddRect(overlay_pos, ImVec2(overlay_pos.x + overlay_size.x, overlay_pos.y + overlay_size.y),
                IM_COL32(72, 78, 92, 220), Theme::ROUNDING_SM * zoom, 0, 1.5f * zoom);

    // Reference dB lines
    const float ref_lines[] = {-60.0f, -40.0f, -20.0f};
    for (float db : ref_lines) {
        float t = (db - (-80.0f)) / 80.0f;
        float y = overlay_pos.y + overlay_size.y * (1.0f - t);
        dl->AddLine(ImVec2(overlay_pos.x, y), ImVec2(overlay_pos.x + overlay_size.x, y),
                    IM_COL32(58, 64, 76, 100), 1.0f * zoom);
    }

    // Frequency tick lines
    const float ticks[] = {100.0f, 1000.0f, 10000.0f};
    for (float hz : ticks) {
        const float lo = std::log10(20.0f);
        const float hi = std::log10(20000.0f);
        float norm = std::clamp((std::log10(hz) - lo) / (hi - lo), 0.0f, 1.0f);
        float x = overlay_pos.x + norm * overlay_size.x;
        dl->AddLine(ImVec2(x, overlay_pos.y), ImVec2(x, overlay_pos.y + overlay_size.y),
                    IM_COL32(52, 58, 72, 100), 1.0f * zoom);
    }

    // Render the curves
    const auto& smoothed_in = spectrum_analyzer_.smoothed_input_db();
    const auto& smoothed_out = spectrum_analyzer_.smoothed_output_db();

    // Helper lambda to draw curve
    auto draw_curve = [&](const std::array<float, SpectrumAnalyzer::DISPLAY_BARS>& bars,
                          ImU32 color) {
        constexpr int BARS = SpectrumAnalyzer::DISPLAY_BARS;
        ImVec2 prev_pt;
        for (int i = 0; i < BARS; ++i) {
            float x = overlay_pos.x + (static_cast<float>(i) / (BARS - 1)) * overlay_size.x;
            float db = std::clamp(bars[i], -80.0f, 0.0f);
            float t = (db - (-80.0f)) / 80.0f;
            float y = overlay_pos.y + overlay_size.y * (1.0f - t);
            ImVec2 pt(x, y);
            if (i > 0) {
                dl->AddLine(prev_pt, pt, color, 1.8f * zoom);
            }
            prev_pt = pt;
        }
    };

    // Blue curve = signal entering pedal (pre-processing)
    draw_curve(smoothed_in, IM_COL32(92, 170, 255, 230));
    // Green curve = signal leaving pedal (post-processing)
    draw_curve(smoothed_out, IM_COL32(82, 220, 135, 230));

    // Legend/Label
    ImGui::SetCursorScreenPos(ImVec2(overlay_pos.x + 8.0f * zoom, overlay_pos.y + 6.0f * zoom));
    ImGui::SetWindowFontScale(zoom * 0.7f);
    ImGui::TextColored(Theme::TextSecondary(), "Pre ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.36f, 0.67f, 1.0f, 1.0f), "[In]");
    ImGui::SameLine();
    ImGui::TextColored(Theme::TextSecondary(), " | Post ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.32f, 0.86f, 0.53f, 1.0f), "[Out]");
    ImGui::SetWindowFontScale(1.0f);
}

}  // namespace Amplitron
