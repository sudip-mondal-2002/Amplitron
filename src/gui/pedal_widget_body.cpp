#include "gui/pedal_widget.h"
#include "audio/audio_engine.h"
#include "audio/effects/tuner.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/looper.h"
#include "audio/effects/multiband_compressor.h"
#include "gui/file_dialog.h"
#include "gui/theme.h"
#include "gui/gui_midi.h"
#include "midi/midi_manager.h"

#include <cstdio>
#include <cmath>

namespace Amplitron {

void PedalWidget::render_amp_cabinet(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, float pedal_height, float zoom) {
    ImU32 cab_body = IM_COL32(30, 22, 16, 255);
    ImU32 cab_border = IM_COL32(90, 70, 40, 255);
    ImU32 cab_grille = IM_COL32(18, 14, 10, 255);
    ImU32 cab_grille_line = IM_COL32(38, 30, 22, 180);

    dl->AddRectFilled(p0, p1, cab_body, Theme::ROUNDING_MD * zoom);
    dl->AddRect(p0, p1, cab_border, Theme::ROUNDING_MD * zoom, 0, 2.5f * zoom);

    dl->AddRectFilled(
        ImVec2(p0.x + 6 * zoom, p0.y + 6 * zoom),
        ImVec2(p1.x - 6 * zoom, p0.y + 10 * zoom),
        Theme::ACCENT_GOLD_DIM, 2.0f * zoom);

    ImVec2 plate_p0 = ImVec2(p0.x + 8 * zoom, p0.y + 14 * zoom);
    ImVec2 plate_p1 = ImVec2(p1.x - 8 * zoom, p0.y + 50 * zoom);
    dl->AddRectFilled(plate_p0, plate_p1,
        IM_COL32(46, 38, 28, 220), Theme::ROUNDING_SM * zoom);
    dl->AddRect(plate_p0, plate_p1,
        IM_COL32(70, 58, 38, 180), Theme::ROUNDING_SM * zoom, 0, 1.0f * zoom);

    ImGui::SetCursorScreenPos(ImVec2(p0.x + 12 * zoom, p0.y + 18 * zoom));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Gold());
    ImGui::Text("AMP");
    ImGui::PopStyleColor();

    int model_idx = static_cast<int>(effect_->params()[0].value);
    const auto& models = get_amp_models();
    const char* model_name = "Unknown";
    if (model_idx >= 0 && model_idx < static_cast<int>(models.size())) {
        model_name = models[model_idx].name;
    }
    ImVec2 mn_size = ImGui::CalcTextSize(model_name);
    float mn_x = p0.x + (pedal_width - mn_size.x) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(mn_x, p0.y + 33 * zoom));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextPrimary());
    ImGui::Text("%s", model_name);
    ImGui::PopStyleColor();

    float led_x = p1.x - 22 * zoom;
    float led_y = p0.y + 26 * zoom;
    dl->AddCircleFilled(ImVec2(led_x, led_y), 5 * zoom, Theme::LED_GREEN);
    dl->AddCircleFilled(ImVec2(led_x, led_y), 8 * zoom, Theme::LED_GREEN_GLOW & 0x30FFFFFF);

    float grille_top = p1.y - 100 * zoom;
    float grille_bottom = p1.y - 12 * zoom;
    float grille_left = p0.x + 12 * zoom;
    float grille_right = p1.x - 12 * zoom;

    dl->AddRectFilled(
        ImVec2(grille_left, grille_top),
        ImVec2(grille_right, grille_bottom),
        cab_grille, Theme::ROUNDING_SM * zoom);
    dl->AddRect(
        ImVec2(grille_left, grille_top),
        ImVec2(grille_right, grille_bottom),
        IM_COL32(50, 40, 28, 180), Theme::ROUNDING_SM * zoom, 0, 1.0f * zoom);

    for (float gy = grille_top + 6 * zoom; gy < grille_bottom - 4 * zoom; gy += 5.0f * zoom) {
        dl->AddLine(
            ImVec2(grille_left + 4 * zoom, gy),
            ImVec2(grille_right - 4 * zoom, gy),
            cab_grille_line, 1.0f * zoom);
    }

    dl->AddRectFilled(
        ImVec2(p0.x + 6 * zoom, p1.y - 10 * zoom),
        ImVec2(p1.x - 6 * zoom, p1.y - 6 * zoom),
        Theme::ACCENT_GOLD_DIM, 2.0f * zoom);
}

void PedalWidget::render_tuner_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom) {
    auto* tuner = dynamic_cast<TunerPedal*>(effect_.get());
    if (tuner) {
        float cx = p0.x + pedal_width * 0.5f;

        bool has_signal = tuner->signal_detected.load(std::memory_order_relaxed);
        int note_idx = tuner->detected_note.load(std::memory_order_relaxed);
        int octave = tuner->detected_octave.load(std::memory_order_relaxed);
        float cents = tuner->detected_cents.load(std::memory_order_relaxed);
        float freq = tuner->detected_freq.load(std::memory_order_relaxed);

        float display_y = p0.y + 55 * zoom;

        if (has_signal && note_idx >= 0) {
            char note_buf[16];
            snprintf(note_buf, sizeof(note_buf), "%s%d",
                     TunerPedal::note_name(note_idx), octave);
            ImVec2 note_size = ImGui::CalcTextSize(note_buf);
            float note_x = cx - note_size.x * 1.5f;
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 2.0f,
                ImVec2(note_x, display_y),
                Theme::TEXT_PRIMARY, note_buf);

            display_y += 45 * zoom;

            char cents_buf[32];
            snprintf(cents_buf, sizeof(cents_buf), "%+.1f cents", cents);
            ImVec2 cents_text_size = ImGui::CalcTextSize(cents_buf);
            ImGui::SetCursorScreenPos(ImVec2(cx - cents_text_size.x * 0.5f, display_y));
            float abs_cents = std::fabs(cents);
            ImVec4 cents_col = (abs_cents < 2.0f)
                ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                : (abs_cents < 15.0f)
                    ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                    : ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, cents_col);
            ImGui::TextUnformatted(cents_buf);
            ImGui::PopStyleColor();

            display_y += 22 * zoom;

            float bar_w = pedal_width - 30 * zoom;
            float bar_h = 10 * zoom;
            float bar_x = p0.x + 15 * zoom;
            float bar_y = display_y;
            dl->AddRectFilled(
                ImVec2(bar_x, bar_y),
                ImVec2(bar_x + bar_w, bar_y + bar_h),
                Theme::KNOB_BG, 3.0f * zoom);
            float center_x = bar_x + bar_w * 0.5f;
            dl->AddLine(
                ImVec2(center_x, bar_y - 1 * zoom),
                ImVec2(center_x, bar_y + bar_h + 1 * zoom),
                Theme::TEXT_DIM, 1.5f * zoom);
            float needle_norm = clamp(cents / 50.0f, -1.0f, 1.0f);
            float needle_x = center_x + needle_norm * (bar_w * 0.5f);
            ImU32 needle_col = ImGui::ColorConvertFloat4ToU32(cents_col);
            dl->AddRectFilled(
                ImVec2(needle_x - 3 * zoom, bar_y - 2 * zoom),
                ImVec2(needle_x + 3 * zoom, bar_y + bar_h + 2 * zoom),
                needle_col, 2.0f * zoom);

            display_y += bar_h + 14 * zoom;

            char freq_buf[32];
            snprintf(freq_buf, sizeof(freq_buf), "%.1f Hz", freq);
            ImVec2 freq_size = ImGui::CalcTextSize(freq_buf);
            ImGui::SetCursorScreenPos(ImVec2(cx - freq_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
            ImGui::TextUnformatted(freq_buf);
            ImGui::PopStyleColor();

            display_y += 22 * zoom;
        } else {
            const char* no_sig = "---";
            ImVec2 ns_size = ImGui::CalcTextSize(no_sig);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 2.0f,
                ImVec2(cx - ns_size.x * 1.5f, display_y),
                Theme::TEXT_DIM, no_sig);
            display_y += 45 * zoom;

            const char* waiting = "Play a note...";
            ImVec2 wt_size = ImGui::CalcTextSize(waiting);
            ImGui::SetCursorScreenPos(ImVec2(cx - wt_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim());
            ImGui::TextUnformatted(waiting);
            ImGui::PopStyleColor();

            display_y += 22 * zoom;
        }

        display_y += 8 * zoom;
        bool mute_on = effect_->params()[0].value >= 0.5f;
        const char* mute_label = mute_on ? "[MUTE ON]" : "[MUTE OFF]";
        ImVec2 ml_size = ImGui::CalcTextSize(mute_label);
        ImGui::SetCursorScreenPos(ImVec2(cx - ml_size.x * 0.5f, display_y));
        ImGui::PushStyleColor(ImGuiCol_Text,
            mute_on ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        ImGui::TextUnformatted(mute_label);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(cx - ml_size.x * 0.5f, display_y));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##tuner_mute_toggle", ml_size);
        if (ImGui::IsItemClicked()) {
            float new_val = mute_on ? 0.0f : 1.0f;
            effect_->params()[0].value = new_val;
            engine_.push_param_change(index_, 0, new_val);
        }
        if (ImGui::IsItemHovered()) {
            if (!effect_->params()[0].tooltip.empty()) {
                ImGui::SetTooltip("Click to toggle mute\n\n%s", effect_->params()[0].tooltip.c_str());
            } else {
                ImGui::SetTooltip("Click to toggle mute");
            }
        }
    }
}

void PedalWidget::render_ir_cabinet_display(ImVec2 p0, float pedal_width, float zoom) {
    auto* ir_cab = dynamic_cast<CabinetSim*>(effect_.get());
    if (ir_cab) {
        float cx = p0.x + pedal_width * 0.5f;
        float display_y = p0.y + 50 * zoom;

        float btn_w = pedal_width - 30 * zoom;
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 15 * zoom, display_y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.20f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.30f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.42f, 0.20f, 1.0f));
        char load_id[64];
        snprintf(load_id, sizeof(load_id), "Load IR##ir_load_%d", index_);
        if (ImGui::Button(load_id, ImVec2(btn_w, 22 * zoom))) {
            std::string path = show_open_dialog("Load Impulse Response",
                                               "WAV Audio", "wav");
            if (!path.empty()) {
                ir_cab->load_ir(path);
            }
        }
        ImGui::PopStyleColor(3);

        display_y += 28 * zoom;

        if (ir_cab->has_ir()) {
            const std::string& ir_name = ir_cab->ir_name();
            std::string display_name = ir_name;
            if (display_name.size() > 20) {
                display_name = display_name.substr(0, 17) + "...";
            }
            ImVec2 name_size = ImGui::CalcTextSize(display_name.c_str());
            ImGui::SetCursorScreenPos(ImVec2(cx - name_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextPrimary());
            ImGui::TextUnformatted(display_name.c_str());
            ImGui::PopStyleColor();

            display_y += 18 * zoom;

            char dur_buf[32];
            snprintf(dur_buf, sizeof(dur_buf), "%.1f ms", ir_cab->ir_duration_ms());
            ImVec2 dur_size = ImGui::CalcTextSize(dur_buf);
            ImGui::SetCursorScreenPos(ImVec2(cx - dur_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
            ImGui::TextUnformatted(dur_buf);
            ImGui::PopStyleColor();

            display_y += 22 * zoom;

            ImGui::SetCursorScreenPos(ImVec2(p0.x + 15 * zoom, display_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.12f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.15f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.20f, 0.15f, 1.0f));
            char clear_id[64];
            snprintf(clear_id, sizeof(clear_id), "Clear##ir_clear_%d", index_);
            if (ImGui::Button(clear_id, ImVec2(btn_w, 20 * zoom))) {
                ir_cab->clear_ir();
            }
            ImGui::PopStyleColor(3);
        } else {
            const char* no_ir = "No IR loaded";
            ImVec2 ni_size = ImGui::CalcTextSize(no_ir);
            ImGui::SetCursorScreenPos(ImVec2(cx - ni_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim());
            ImGui::TextUnformatted(no_ir);
            ImGui::PopStyleColor();
        }
    }
}

void PedalWidget::render_looper_display(ImVec2 p0, float pedal_width, float zoom) {
    auto* looper = dynamic_cast<Looper*>(effect_.get());
    if (!looper) return;

    float cx = p0.x + pedal_width * 0.5f;
    float display_y = p0.y + 55 * zoom;

    Looper::State st = looper->state();
    bool has_loop = looper->has_loop();
    int loop_len = looper->loop_length_samples();
    int play_pos = looper->playhead_samples();

    const char* state_label = "EMPTY";
    ImVec4 state_col = Theme::TextDim();
    switch (st) {
        case Looper::State::Empty:      state_label = "EMPTY";  state_col = Theme::TextDim(); break;
        case Looper::State::Idle:       state_label = "STOP";   state_col = Theme::TextSecondary(); break;
        case Looper::State::Recording:  state_label = "REC";    state_col = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); break;
        case Looper::State::Playing:    state_label = "PLAY";   state_col = ImVec4(0.2f, 0.9f, 0.3f, 1.0f); break;
        case Looper::State::Overdubbing:state_label = "DUB";    state_col = ImVec4(0.95f, 0.80f, 0.25f, 1.0f); break;
    }

    ImVec2 st_size = ImGui::CalcTextSize(state_label);
    ImGui::SetCursorScreenPos(ImVec2(cx - st_size.x * 0.5f, display_y));
    ImGui::PushStyleColor(ImGuiCol_Text, state_col);
    ImGui::TextUnformatted(state_label);
    ImGui::PopStyleColor();

    display_y += 18 * zoom;

    float bar_w = pedal_width - 30 * zoom;
    float progress = 0.0f;
    if (has_loop && loop_len > 0) {
        progress = clamp(static_cast<float>(play_pos) / static_cast<float>(loop_len), 0.0f, 1.0f);
    }
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 15 * zoom, display_y));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.11f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, state_col);
    ImGui::ProgressBar(progress, ImVec2(bar_w, 8 * zoom), "");
    ImGui::PopStyleColor(2);

    display_y += 16 * zoom;

    float btn_w_total = bar_w;
    float btn_gap = 8.0f * zoom;
    float btn_w = (btn_w_total - btn_gap) * 0.5f;
    float btn_h = 22.0f * zoom;

    // Row 1: Record / Play
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 15 * zoom, display_y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
    char rec_id[64];
    std::snprintf(rec_id, sizeof(rec_id), "Record##looper_rec_%d", index_);
    if (ImGui::Button(rec_id, ImVec2(btn_w, btn_h))) {
        looper->request_record_toggle();
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Start/stop recording (new loop)");

    ImGui::SameLine(0.0f, btn_gap);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.30f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.42f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.55f, 0.28f, 1.0f));
    char play_id[64];
    std::snprintf(play_id, sizeof(play_id), "Play/Stop##looper_play_%d", index_);
    if (ImGui::Button(play_id, ImVec2(btn_w, btn_h))) {
        looper->request_play_toggle();
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle playback (keeps loop in memory)");

    display_y += btn_h + 6 * zoom;

    // Row 2: Overdub / Clear
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 15 * zoom, display_y));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.26f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.34f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.52f, 0.45f, 0.15f, 1.0f));
    char dub_id[64];
    std::snprintf(dub_id, sizeof(dub_id), "Overdub##looper_dub_%d", index_);
    if (ImGui::Button(dub_id, ImVec2(btn_w, btn_h))) {
        looper->request_overdub_toggle();
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle overdub mode (record over existing loop)");

    ImGui::SameLine(0.0f, btn_gap);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.12f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.15f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.20f, 0.15f, 1.0f));
    char clr_id[64];
    std::snprintf(clr_id, sizeof(clr_id), "Clear##looper_clear_%d", index_);
    if (ImGui::Button(clr_id, ImVec2(btn_w, btn_h))) {
        looper->request_clear();
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear loop from memory");

    display_y += btn_h + 8 * zoom;

    // Loop Level slider (param 0)
    if (!effect_->params().empty()) {
        float& level = effect_->params()[0].value;
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 15 * zoom, display_y));
        ImGui::SetNextItemWidth(bar_w);
        char slider_id[64];
        std::snprintf(slider_id, sizeof(slider_id), "##looper_level_%d", index_);
        if (ImGui::SliderFloat(slider_id, &level, 0.0f, 1.0f, "Loop Level: %.2f")) {
            level = clamp(level, 0.0f, 1.0f);
            engine_.push_param_change(index_, 0, level);
        }
        if (ImGui::IsItemActivated()) {
            popup_active_param_index_ = 0;
            popup_param_value_before_edit_ = level;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && popup_active_param_index_ == 0) {
            if (level != popup_param_value_before_edit_) {
                commit_param_change(0, popup_param_value_before_edit_, level);
            }
            popup_active_param_index_ = -1;
        }
        if (ImGui::IsItemHovered() && !effect_->params()[0].tooltip.empty()) {
            ImGui::SetTooltip("%s", effect_->params()[0].tooltip.c_str());
        }
    }
}

void PedalWidget::render_multiband_compressor_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom) {
    auto* mb_comp = dynamic_cast<MultiBandCompressor*>(effect_.get());
    if (!mb_comp) return;

    auto& params = effect_->params();
    if (params.size() < 18) return;

    // Outer boundaries
    ImVec2 p1 = ImVec2(p0.x + pedal_width, p0.y + Theme::PEDAL_HEIGHT * zoom);

    // Dynamic horizontal divider separating header/plate and controls
    dl->AddLine(ImVec2(p0.x + 8.0f * zoom, p0.y + 48.0f * zoom), ImVec2(p1.x - 8.0f * zoom, p0.y + 48.0f * zoom), Theme::BORDER_DARK, 1.0f * zoom);

    float col_width = (pedal_width - 24.0f * zoom) / 3.0f;

    // --- REUSABLE KNOB HELPER (LAMBDA) ---
    auto render_mb_knob = [&](ImDrawList* dl, ImVec2 center, int pi, float radius, const char* label_prefix) {
        auto& param = params[pi];
        char label[64];
        std::snprintf(label, sizeof(label), "##knob_%s_%d_%d_%s", effect_->name(), index_, pi, label_prefix);

        float r = radius * zoom;
        float knob_hit_size = r * Theme::KNOB_HIT_MULT;

        ImGui::SetCursorScreenPos(ImVec2(center.x - knob_hit_size * 0.5f, center.y - knob_hit_size * 0.5f));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(label, ImVec2(knob_hit_size, knob_hit_size));

        bool is_hovered = ImGui::IsItemHovered();
        bool is_active = ImGui::IsItemActive();

        float range = param.max_val - param.min_val;

        if (is_active && !knob_was_active_) {
            active_param_index_ = pi;
            param_value_before_drag_ = param.value;
        }

        if (is_active) {
            float mdy = ImGui::GetIO().MouseDelta.y;
            if (mdy != 0.0f) {
                float sensitivity = 0.005f;
                float value_delta = -mdy * sensitivity * range;
                if (ImGui::GetIO().KeyShift) value_delta *= 0.2f;
                if (ImGui::GetIO().KeyCtrl)  value_delta *= 3.0f;

                float new_val = clamp(param.value + value_delta, param.min_val, param.max_val);
                if (new_val != param.value) {
                    param.value = new_val;
                    engine_.push_param_change(index_, pi, new_val);
                }
            }
        }

        if (knob_was_active_ && !is_active && active_param_index_ == pi) {
            float new_val = param.value;
            if (new_val != param_value_before_drag_) {
                commit_param_change(pi, param_value_before_drag_, new_val);
            }
            active_param_index_ = -1;
            knob_was_active_ = false;
        }

        if (active_param_index_ == pi) {
            knob_was_active_ = is_active;
        }

        if (is_hovered && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f) {
            float old_val = param.value;
            float step = range * 0.03f;
            if (ImGui::GetIO().KeyShift) step *= 0.2f;
            float new_val = clamp(param.value + ImGui::GetIO().MouseWheel * step, param.min_val, param.max_val);
            if (new_val != old_val) {
                param.value = new_val;
                engine_.push_param_change(index_, pi, new_val);
                commit_param_change(pi, old_val, new_val);
            }
        }

        if (is_hovered && ImGui::IsMouseDoubleClicked(0)) {
            float old_val = param.value;
            float new_val = param.default_val;
            if (new_val != old_val) {
                param.value = new_val;
                engine_.push_param_change(index_, pi, new_val);
                commit_param_change(pi, old_val, new_val);
            }
        }

        if (is_hovered && ImGui::IsMouseClicked(1)) {
            ImGui::OpenPopup(label);
        }
        if (ImGui::BeginPopup(label)) {
            ImGui::Text("%s", param.name.c_str());
            ImGui::SetNextItemWidth(120);
            float slider_val = param.value;
            if (ImGui::SliderFloat("##edit", &slider_val, param.min_val, param.max_val, "%.2f")) {
                param.value = slider_val;
                engine_.push_param_change(index_, pi, slider_val);
            }
            if (ImGui::IsItemActivated()) {
                popup_active_param_index_ = pi;
                popup_param_value_before_edit_ = param.value;
            }
            if (ImGui::IsItemDeactivatedAfterEdit() && popup_active_param_index_ == pi) {
                if (param.value != popup_param_value_before_edit_) {
                    engine_.push_param_change(index_, pi, param.value);
                    commit_param_change(pi, popup_param_value_before_edit_, param.value);
                }
                popup_active_param_index_ = -1;
            }
            if (ImGui::Button("Reset")) {
                float old_val = param.value;
                float new_val = param.default_val;
                if (new_val != old_val) {
                    param.value = new_val;
                    engine_.push_param_change(index_, pi, new_val);
                    commit_param_change(pi, old_val, new_val);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::Separator();
            ImGui::TextColored(Theme::Gold(), "MIDI Control");
            if (gui_midi_) {
                if (gui_midi_->render_remove_mapping_item(effect_->name(), param.name)) {
                    ImGui::CloseCurrentPopup();
                }
                if (gui_midi_->render_learn_menu_item(effect_->name(), param.name)) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::Spacing();
                if (gui_midi_->render_remove_bypass_item(effect_->name())) {
                    ImGui::CloseCurrentPopup();
                }
                if (gui_midi_->render_learn_bypass_item(effect_->name())) {
                    ImGui::CloseCurrentPopup();
                }
            } else {
                ImGui::TextDisabled("MIDI manager not available");
            }
            ImGui::EndPopup();
        }

        // Draw track
        float normalized = (param.value - param.min_val) / range;
        constexpr float ARC_START = 2.356f;
        constexpr float ARC_RANGE = 4.712f;
        float track_radius = r + 2.5f * zoom;
        int segments = 30;
        for (int s = 0; s < segments; ++s) {
            float t0 = static_cast<float>(s) / segments;
            float t1 = static_cast<float>(s + 1) / segments;
            float a0 = ARC_START + t0 * ARC_RANGE;
            float a1 = ARC_START + t1 * ARC_RANGE;

            bool filled = t0 <= normalized;
            ImU32 seg_color = filled ? ImGui::ColorConvertFloat4ToU32(led_color_) : Theme::KNOB_TRACK_OFF;

            dl->AddLine(
                ImVec2(center.x + std::cos(a0) * track_radius, center.y + std::sin(a0) * track_radius),
                ImVec2(center.x + std::cos(a1) * track_radius, center.y + std::sin(a1) * track_radius),
                seg_color, 2.0f * zoom);
        }

        ImU32 knob_bg = is_active ? Theme::KNOB_ACTIVE : (is_hovered ? Theme::KNOB_HOVER : Theme::KNOB_FACE);
        dl->AddCircleFilled(center, r, Theme::KNOB_BG);
        dl->AddCircleFilled(center, r - 1.0f * zoom, knob_bg);

#ifndef AMPLITRON_NO_MIDI
        if (gui_midi_ && gui_midi_->midi().is_learning() &&
            gui_midi_->midi().learn_effect_name() == effect_->name() &&
            gui_midi_->midi().learn_param_name() == param.name) {
            float time = static_cast<float>(ImGui::GetTime());
            float alpha = (std::sin(time * 2.0f * 3.14159f * 10.0f) + 1.0f) * 0.5f;
            ImU32 outline_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.6f, 1.0f, 0.4f + alpha * 0.6f));
            dl->AddCircle(center, r + 3.0f * zoom, outline_col, 0, 2.0f * zoom);
        }
#endif

        float pointer_angle = ARC_START + normalized * ARC_RANGE;
        float ptr_inner = r * 0.25f;
        float ptr_outer = r - 2.0f * zoom;
        ImVec2 ptr_from = ImVec2(center.x + std::cos(pointer_angle) * ptr_inner, center.y + std::sin(pointer_angle) * ptr_inner);
        ImVec2 ptr_to = ImVec2(center.x + std::cos(pointer_angle) * ptr_outer, center.y + std::sin(pointer_angle) * ptr_outer);
        ImU32 ptr_color = is_active ? Theme::ACCENT_GOLD_HOT : Theme::ACCENT_GOLD;
        dl->AddLine(ptr_from, ptr_to, ptr_color, 2.0f * zoom);

        // Tooltip
        if (is_hovered || is_active) {
            std::string val_str = Theme::formatParameterValue(param.value, param.unit);
            std::string min_str = Theme::formatParameterValue(param.min_val, param.unit);
            std::string max_str = Theme::formatParameterValue(param.max_val, param.unit);
            std::string midi_info = gui_midi_ ? gui_midi_->get_mapping_info(effect_->name(), param.name) : "";
            
            if (param.tooltip.empty()) {
                ImGui::SetTooltip("%s: %s\nRange: [%s, %s]%s", param.name.c_str(), val_str.c_str(), min_str.c_str(), max_str.c_str(), midi_info.c_str());
            } else {
                ImGui::SetTooltip("%s: %s\nRange: [%s, %s]\n\n%s%s", param.name.c_str(), val_str.c_str(), min_str.c_str(), max_str.c_str(), param.tooltip.c_str(), midi_info.c_str());
            }
        }

        // Labels
        const char* short_name = param.name.c_str();
        if (std::strncmp(short_name, "Low ", 4) == 0) short_name += 4;
        else if (std::strncmp(short_name, "Mid ", 4) == 0) short_name += 4;
        else if (std::strncmp(short_name, "High ", 5) == 0) short_name += 5;

        ImVec2 text_size = ImGui::CalcTextSize(short_name);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f,
                    ImVec2(center.x - text_size.x * 0.5f, center.y + r + 5.0f * zoom),
                    Theme::TEXT_SECONDARY, short_name);

        std::string val_display = Theme::formatParameterValue(param.value, param.unit);
        ImVec2 val_size = ImGui::CalcTextSize(val_display.c_str());
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.75f,
                    ImVec2(center.x - val_size.x * 0.5f, center.y - r - 13.0f * zoom),
                    is_active ? Theme::ACCENT_GOLD_HOT : Theme::TEXT_DIM, val_display.c_str());
    };

    // --- REUSABLE SLIDER HELPER (LAMBDA) ---
    auto render_xover_slider = [&](ImDrawList* dl, float track_x, int pi, const char* label_prefix, bool ticks_on_left) {
        auto& param = params[pi];
        char label[64];
        std::snprintf(label, sizeof(label), "##slider_%s_%d_%d_%s", effect_->name(), index_, pi, label_prefix);

        float track_top = p0.y + 90.0f * zoom;
        float track_bottom = p0.y + 260.0f * zoom;
        float range = param.max_val - param.min_val;
        float normalized = (param.value - param.min_val) / range;
        float handle_y = track_bottom - normalized * (track_bottom - track_top);

        // Click-and-drag detection box
        ImGui::SetCursorScreenPos(ImVec2(track_x - 12.0f * zoom, track_top));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(label, ImVec2(24.0f * zoom, track_bottom - track_top));

        bool is_hovered = ImGui::IsItemHovered();
        bool is_active = ImGui::IsItemActive();

        if (is_active && !knob_was_active_) {
            active_param_index_ = pi;
            param_value_before_drag_ = param.value;
        }

        if (is_active) {
            float my = ImGui::GetIO().MousePos.y;
            float norm = (track_bottom - my) / (track_bottom - track_top);
            norm = clamp(norm, 0.0f, 1.0f);
            float new_val = param.min_val + norm * range;

            // Prevent crossover overlap
            if (pi == 0) {
                float high_val = params[1].value;
                if (new_val >= high_val) new_val = high_val - 10.0f;
            } else if (pi == 1) {
                float low_val = params[0].value;
                if (new_val <= low_val) new_val = low_val + 10.0f;
            }

            if (new_val != param.value) {
                param.value = new_val;
                engine_.push_param_change(index_, pi, new_val);
            }
        }

        if (knob_was_active_ && !is_active && active_param_index_ == pi) {
            float new_val = param.value;
            if (new_val != param_value_before_drag_) {
                commit_param_change(pi, param_value_before_drag_, new_val);
            }
            active_param_index_ = -1;
            knob_was_active_ = false;
        }

        if (active_param_index_ == pi) {
            knob_was_active_ = is_active;
        }

        if (is_hovered && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f) {
            float old_val = param.value;
            float step = range * 0.02f;
            if (ImGui::GetIO().KeyShift) step *= 0.2f;
            float new_val = clamp(param.value + ImGui::GetIO().MouseWheel * step, param.min_val, param.max_val);

            // Prevent crossover overlap
            if (pi == 0) {
                float high_val = params[1].value;
                if (new_val >= high_val) new_val = high_val - 10.0f;
            } else if (pi == 1) {
                float low_val = params[0].value;
                if (new_val <= low_val) new_val = low_val + 10.0f;
            }

            if (new_val != old_val) {
                param.value = new_val;
                engine_.push_param_change(index_, pi, new_val);
                commit_param_change(pi, old_val, new_val);
            }
        }

        if (is_hovered && ImGui::IsMouseDoubleClicked(0)) {
            float old_val = param.value;
            float new_val = param.default_val;

            // Prevent crossover overlap on reset
            if (pi == 0) {
                float high_val = params[1].value;
                if (new_val >= high_val) new_val = high_val - 10.0f;
            } else if (pi == 1) {
                float low_val = params[0].value;
                if (new_val <= low_val) new_val = low_val + 10.0f;
            }

            if (new_val != old_val) {
                param.value = new_val;
                engine_.push_param_change(index_, pi, new_val);
                commit_param_change(pi, old_val, new_val);
            }
        }

        // Draw track vertical line
        dl->AddRectFilled(ImVec2(track_x - 1.5f * zoom, track_top), ImVec2(track_x + 1.5f * zoom, track_bottom), Theme::KNOB_TRACK_OFF, 1.5f * zoom);

        // Draw Ticks & Labels
        if (pi == 0) { // Low crossover (50 to 1000 Hz)
            float tick_hzs[] = { 50.0f, 200.0f, 500.0f, 1000.0f };
            for (float hz : tick_hzs) {
                float norm = (hz - param.min_val) / range;
                float ty = track_bottom - norm * (track_bottom - track_top);
                dl->AddLine(ImVec2(track_x - 4.0f * zoom, ty), ImVec2(track_x, ty), Theme::BORDER_LIGHT, 1.0f * zoom);
                
                char tick_lbl[16];
                if (hz >= 1000.0f) std::snprintf(tick_lbl, sizeof(tick_lbl), "1k");
                else std::snprintf(tick_lbl, sizeof(tick_lbl), "%.0f", hz);

                ImVec2 tsz = ImGui::CalcTextSize(tick_lbl);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.65f,
                            ImVec2(track_x - 6.0f * zoom - tsz.x, ty - tsz.y * 0.5f),
                            Theme::TEXT_DIM, tick_lbl);
            }
        } else { // High crossover (1000 to 15000 Hz)
            float tick_hzs[] = { 1000.0f, 4000.0f, 8000.0f, 12000.0f, 15000.0f };
            for (float hz : tick_hzs) {
                float norm = (hz - param.min_val) / range;
                float ty = track_bottom - norm * (track_bottom - track_top);
                dl->AddLine(ImVec2(track_x, ty), ImVec2(track_x + 4.0f * zoom, ty), Theme::BORDER_LIGHT, 1.0f * zoom);

                char tick_lbl[16];
                if (hz >= 1000.0f) std::snprintf(tick_lbl, sizeof(tick_lbl), "%.0fk", hz / 1000.0f);
                else std::snprintf(tick_lbl, sizeof(tick_lbl), "%.0f", hz);

                ImVec2 tsz = ImGui::CalcTextSize(tick_lbl);
                dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.65f,
                            ImVec2(track_x + 6.0f * zoom, ty - tsz.y * 0.5f),
                            Theme::TEXT_DIM, tick_lbl);
            }
        }

        // Draw pill handle
        ImVec2 handle_center = ImVec2(track_x, handle_y);
        ImU32 handle_bg = is_active ? Theme::KNOB_ACTIVE : (is_hovered ? Theme::KNOB_HOVER : Theme::KNOB_FACE);
        ImU32 border_col = (is_active || is_hovered) ? Theme::ACCENT_GOLD_HOT : Theme::ACCENT_GOLD;

        dl->AddRectFilled(ImVec2(track_x - 8.0f * zoom, handle_y - 5.0f * zoom), ImVec2(track_x + 8.0f * zoom, handle_y + 5.0f * zoom), Theme::KNOB_BG, 3.0f * zoom);
        dl->AddRectFilled(ImVec2(track_x - 7.0f * zoom, handle_y - 4.0f * zoom), ImVec2(track_x + 7.0f * zoom, handle_y + 4.0f * zoom), handle_bg, 2.0f * zoom);
        dl->AddRect(ImVec2(track_x - 8.0f * zoom, handle_y - 5.0f * zoom), ImVec2(track_x + 8.0f * zoom, handle_y + 5.0f * zoom), border_col, 3.0f * zoom, 0, 1.5f * zoom);
        dl->AddCircleFilled(handle_center, 2.0f * zoom, border_col);

        // Value text at the top of the track
        std::string val_str = Theme::formatParameterValue(param.value, param.unit);
        ImVec2 vsz = ImGui::CalcTextSize(val_str.c_str());
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.75f,
                    ImVec2(track_x - vsz.x * 0.5f, track_top - vsz.y - 4.0f * zoom),
                    is_active ? Theme::ACCENT_GOLD_HOT : Theme::TEXT_SECONDARY, val_str.c_str());

        // Header text above the value
        const char* s_name = (pi == 0) ? "Low X" : "High X";
        ImVec2 nsz = ImGui::CalcTextSize(s_name);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.75f,
                    ImVec2(track_x - nsz.x * 0.5f, track_top - vsz.y - nsz.y - 6.0f * zoom),
                    Theme::TEXT_DIM, s_name);

        if (is_hovered || is_active) {
            std::string midi_info = gui_midi_ ? gui_midi_->get_mapping_info(effect_->name(), param.name) : "";
            ImGui::SetTooltip("%s: %s\nRange: [%s, %s]%s\n\nDrag vertically to adjust\nShift=fine, Ctrl=coarse\nDbl-click to reset",
                              param.name.c_str(), val_str.c_str(),
                              Theme::formatParameterValue(param.min_val, param.unit).c_str(),
                              Theme::formatParameterValue(param.max_val, param.unit).c_str(),
                              midi_info.c_str());
        }
    };

    // --- RENDER 3 COLUMNS & THEIR METERS/KNOBS ---
    const char* titles[3] = { "LOW BAND", "MID BAND", "HIGH BAND" };
    int band_param_offsets[3] = { 2, 7, 12 };

    for (int b = 0; b < 3; ++b) {
        float col_left = p0.x + 12.0f * zoom + b * col_width;
        float col_center = col_left + col_width * 0.5f;

        // Title
        ImVec2 tsz = ImGui::CalcTextSize(titles[b]);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.9f,
                    ImVec2(col_center - tsz.x * 0.5f, p0.y + 55.0f * zoom),
                    Theme::TEXT_PRIMARY, titles[b]);

        // Horizontal Gain Reduction Meter
        float meter_y = p0.y + 76.0f * zoom;
        float meter_h = 7.0f * zoom;
        float meter_w = col_width - 24.0f * zoom;
        float meter_x = col_left + 12.0f * zoom;

        dl->AddRectFilled(ImVec2(meter_x, meter_y), ImVec2(meter_x + meter_w, meter_y + meter_h), Theme::METER_BG, 3.0f * zoom);
        dl->AddRect(ImVec2(meter_x, meter_y), ImVec2(meter_x + meter_w, meter_y + meter_h), Theme::BORDER_DARK, 3.0f * zoom, 0, 1.0f * zoom);

        float gr_db = mb_comp->get_gain_reduction_db(b);
        float norm_gr = clamp(gr_db / 20.0f, 0.0f, 1.0f);

        if (norm_gr > 0.0f) {
            float fill_x1 = meter_x + meter_w;
            float fill_x0 = meter_x + meter_w - norm_gr * meter_w;
            ImU32 fill_color = Theme::METER_GREEN;
            if (gr_db > 12.0f) fill_color = Theme::METER_RED;
            else if (gr_db > 6.0f) fill_color = Theme::METER_YELLOW;

            dl->AddRectFilled(ImVec2(fill_x0, meter_y + 1.0f * zoom), ImVec2(fill_x1 - 1.0f * zoom, meter_y + meter_h - 1.0f * zoom), fill_color, 2.0f * zoom);
        }

        // GR Meter Ticks
        float tick_dbs[] = { 0.0f, -3.0f, -6.0f, -12.0f, -20.0f };
        for (float db : tick_dbs) {
            float t_norm = -db / 20.0f;
            float tx = meter_x + meter_w * (1.0f - t_norm);
            dl->AddLine(ImVec2(tx, meter_y), ImVec2(tx, meter_y + meter_h + 2.0f * zoom), Theme::BORDER_MID, 1.0f * zoom);
        }

        // Render Knobs
        int p_offset = band_param_offsets[b];
        float k_radius = 12.0f;

        float kx_left = col_left + col_width * 0.28f;
        float kx_right = col_left + col_width * 0.72f;

        // Row 1: Threshold & Ratio
        render_mb_knob(dl, ImVec2(kx_left, p0.y + 120.0f * zoom), p_offset + 0, k_radius, "thresh");
        render_mb_knob(dl, ImVec2(kx_right, p0.y + 120.0f * zoom), p_offset + 1, k_radius, "ratio");

        // Row 2: Attack & Release
        render_mb_knob(dl, ImVec2(kx_left, p0.y + 185.0f * zoom), p_offset + 2, k_radius, "attack");
        render_mb_knob(dl, ImVec2(kx_right, p0.y + 185.0f * zoom), p_offset + 3, k_radius, "release");

        // Row 3: Makeup (Centered)
        render_mb_knob(dl, ImVec2(col_center, p0.y + 248.0f * zoom), p_offset + 4, k_radius, "makeup");
    }

    // --- RENDER 2 INTERACTIVE CROSSOVER SLIDERS ---
    float x1 = p0.x + 12.0f * zoom + col_width;
    float x2 = p0.x + 12.0f * zoom + 2.0f * col_width;

    render_xover_slider(dl, x1, 0, "low", true);
    render_xover_slider(dl, x2, 1, "high", false);

    // --- RENDER GLOBAL OUT GAIN ---
    render_mb_knob(dl, ImVec2(p0.x + pedal_width - 40.0f * zoom, p0.y + Theme::PEDAL_HEIGHT * zoom - Theme::SWITCH_BOTTOM_OFFSET * zoom + 10.0f * zoom), 17, 13.0f, "outgain");
}

} // namespace Amplitron
