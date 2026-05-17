#include "gui/pedal_widget.h"
#include "audio/audio_engine.h"
#include "audio/effects/tuner.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/ir_cabinet.h"
#include "audio/effects/looper.h"
#include "gui/file_dialog.h"
#include "gui/theme.h"

#include <cstdio>
#include <cmath>

namespace Amplitron {

void PedalWidget::render_amp_cabinet(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, float pedal_height) {
    ImU32 cab_body = IM_COL32(30, 22, 16, 255);
    ImU32 cab_border = IM_COL32(90, 70, 40, 255);
    ImU32 cab_grille = IM_COL32(18, 14, 10, 255);
    ImU32 cab_grille_line = IM_COL32(38, 30, 22, 180);

    dl->AddRectFilled(p0, p1, cab_body, Theme::ROUNDING_MD);
    dl->AddRect(p0, p1, cab_border, Theme::ROUNDING_MD, 0, 2.5f);

    dl->AddRectFilled(
        ImVec2(p0.x + 6, p0.y + 6),
        ImVec2(p1.x - 6, p0.y + 10),
        Theme::ACCENT_GOLD_DIM, 2.0f);

    ImVec2 plate_p0 = ImVec2(p0.x + 8, p0.y + 14);
    ImVec2 plate_p1 = ImVec2(p1.x - 8, p0.y + 50);
    dl->AddRectFilled(plate_p0, plate_p1,
        IM_COL32(46, 38, 28, 220), Theme::ROUNDING_SM);
    dl->AddRect(plate_p0, plate_p1,
        IM_COL32(70, 58, 38, 180), Theme::ROUNDING_SM, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(p0.x + 12, p0.y + 18));
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
    ImGui::SetCursorScreenPos(ImVec2(mn_x, p0.y + 33));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextPrimary());
    ImGui::Text("%s", model_name);
    ImGui::PopStyleColor();

    float led_x = p1.x - 22;
    float led_y = p0.y + 26;
    dl->AddCircleFilled(ImVec2(led_x, led_y), 5, Theme::LED_GREEN);
    dl->AddCircleFilled(ImVec2(led_x, led_y), 8, Theme::LED_GREEN_GLOW & 0x30FFFFFF);

    float grille_top = p1.y - 100;
    float grille_bottom = p1.y - 12;
    float grille_left = p0.x + 12;
    float grille_right = p1.x - 12;

    dl->AddRectFilled(
        ImVec2(grille_left, grille_top),
        ImVec2(grille_right, grille_bottom),
        cab_grille, Theme::ROUNDING_SM);
    dl->AddRect(
        ImVec2(grille_left, grille_top),
        ImVec2(grille_right, grille_bottom),
        IM_COL32(50, 40, 28, 180), Theme::ROUNDING_SM, 0, 1.0f);

    for (float gy = grille_top + 6; gy < grille_bottom - 4; gy += 5.0f) {
        dl->AddLine(
            ImVec2(grille_left + 4, gy),
            ImVec2(grille_right - 4, gy),
            cab_grille_line, 1.0f);
    }

    dl->AddRectFilled(
        ImVec2(p0.x + 6, p1.y - 10),
        ImVec2(p1.x - 6, p1.y - 6),
        Theme::ACCENT_GOLD_DIM, 2.0f);
}

void PedalWidget::render_tuner_display(ImDrawList* dl, ImVec2 p0, float pedal_width) {
    auto* tuner = dynamic_cast<TunerPedal*>(effect_.get());
    if (tuner) {
        float cx = p0.x + pedal_width * 0.5f;

        bool has_signal = tuner->signal_detected.load(std::memory_order_relaxed);
        int note_idx = tuner->detected_note.load(std::memory_order_relaxed);
        int octave = tuner->detected_octave.load(std::memory_order_relaxed);
        float cents = tuner->detected_cents.load(std::memory_order_relaxed);
        float freq = tuner->detected_freq.load(std::memory_order_relaxed);

        float display_y = p0.y + 55;

        if (has_signal && note_idx >= 0) {
            char note_buf[16];
            snprintf(note_buf, sizeof(note_buf), "%s%d",
                     TunerPedal::note_name(note_idx), octave);
            ImVec2 note_size = ImGui::CalcTextSize(note_buf);
            float note_x = cx - note_size.x * 1.5f;
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 2.0f,
                ImVec2(note_x, display_y),
                Theme::TEXT_PRIMARY, note_buf);

            display_y += 45;

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

            display_y += 22;

            float bar_w = pedal_width - 30;
            float bar_h = 10;
            float bar_x = p0.x + 15;
            float bar_y = display_y;
            dl->AddRectFilled(
                ImVec2(bar_x, bar_y),
                ImVec2(bar_x + bar_w, bar_y + bar_h),
                Theme::KNOB_BG, 3.0f);
            float center_x = bar_x + bar_w * 0.5f;
            dl->AddLine(
                ImVec2(center_x, bar_y - 1),
                ImVec2(center_x, bar_y + bar_h + 1),
                Theme::TEXT_DIM, 1.5f);
            float needle_norm = clamp(cents / 50.0f, -1.0f, 1.0f);
            float needle_x = center_x + needle_norm * (bar_w * 0.5f);
            ImU32 needle_col = ImGui::ColorConvertFloat4ToU32(cents_col);
            dl->AddRectFilled(
                ImVec2(needle_x - 3, bar_y - 2),
                ImVec2(needle_x + 3, bar_y + bar_h + 2),
                needle_col, 2.0f);

            display_y += bar_h + 14;

            char freq_buf[32];
            snprintf(freq_buf, sizeof(freq_buf), "%.1f Hz", freq);
            ImVec2 freq_size = ImGui::CalcTextSize(freq_buf);
            ImGui::SetCursorScreenPos(ImVec2(cx - freq_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
            ImGui::TextUnformatted(freq_buf);
            ImGui::PopStyleColor();

            display_y += 22;
        } else {
            const char* no_sig = "---";
            ImVec2 ns_size = ImGui::CalcTextSize(no_sig);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 2.0f,
                ImVec2(cx - ns_size.x * 1.5f, display_y),
                Theme::TEXT_DIM, no_sig);
            display_y += 45;

            const char* waiting = "Play a note...";
            ImVec2 wt_size = ImGui::CalcTextSize(waiting);
            ImGui::SetCursorScreenPos(ImVec2(cx - wt_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim());
            ImGui::TextUnformatted(waiting);
            ImGui::PopStyleColor();

            display_y += 22;
        }

        display_y += 8;
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

void PedalWidget::render_ir_cabinet_display(ImVec2 p0, float pedal_width) {
    auto* ir_cab = dynamic_cast<IRCabinet*>(effect_.get());
    if (ir_cab) {
        float cx = p0.x + pedal_width * 0.5f;
        float display_y = p0.y + 50;

        float btn_w = pedal_width - 30;
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 15, display_y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.20f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.30f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.42f, 0.20f, 1.0f));
        char load_id[64];
        snprintf(load_id, sizeof(load_id), "Load IR##ir_load_%d", index_);
        if (ImGui::Button(load_id, ImVec2(btn_w, 22))) {
            std::string path = show_open_dialog("Load Impulse Response",
                                               "WAV Audio", "wav");
            if (!path.empty()) {
                ir_cab->load_ir(path);
            }
        }
        ImGui::PopStyleColor(3);

        display_y += 28;

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

            display_y += 18;

            char dur_buf[32];
            snprintf(dur_buf, sizeof(dur_buf), "%.1f ms", ir_cab->ir_duration_ms());
            ImVec2 dur_size = ImGui::CalcTextSize(dur_buf);
            ImGui::SetCursorScreenPos(ImVec2(cx - dur_size.x * 0.5f, display_y));
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
            ImGui::TextUnformatted(dur_buf);
            ImGui::PopStyleColor();

            display_y += 22;

            ImGui::SetCursorScreenPos(ImVec2(p0.x + 15, display_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.12f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.15f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.20f, 0.15f, 1.0f));
            char clear_id[64];
            snprintf(clear_id, sizeof(clear_id), "Clear##ir_clear_%d", index_);
            if (ImGui::Button(clear_id, ImVec2(btn_w, 20))) {
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

void PedalWidget::render_looper_display(ImVec2 p0, float pedal_width) {
    auto* looper = dynamic_cast<Looper*>(effect_.get());
    if (!looper) return;

    float cx = p0.x + pedal_width * 0.5f;
    float display_y = p0.y + 55;

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

    display_y += 18;

    float bar_w = pedal_width - 30;
    float progress = 0.0f;
    if (has_loop && loop_len > 0) {
        progress = clamp(static_cast<float>(play_pos) / static_cast<float>(loop_len), 0.0f, 1.0f);
    }
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 15, display_y));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.11f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, state_col);
    ImGui::ProgressBar(progress, ImVec2(bar_w, 8), "");
    ImGui::PopStyleColor(2);

    display_y += 16;

    float btn_w_total = bar_w;
    float btn_gap = 8.0f;
    float btn_w = (btn_w_total - btn_gap) * 0.5f;
    float btn_h = 22.0f;

    // Row 1: Record / Play
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 15, display_y));
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

    display_y += btn_h + 6;

    // Row 2: Overdub / Clear
    ImGui::SetCursorScreenPos(ImVec2(p0.x + 15, display_y));
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

    display_y += btn_h + 8;

    // Loop Level slider (param 0)
    if (!effect_->params().empty()) {
        float& level = effect_->params()[0].value;
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 15, display_y));
        ImGui::SetNextItemWidth(bar_w);
        char slider_id[64];
        std::snprintf(slider_id, sizeof(slider_id), "##looper_level_%d", index_);
        float old_val = level;
        if (ImGui::SliderFloat(slider_id, &level, 0.0f, 1.0f, "Loop Level: %.2f")) {
            level = clamp(level, 0.0f, 1.0f);
            engine_.push_param_change(index_, 0, level);
            commit_param_change(0, old_val, level);
        }
        if (ImGui::IsItemHovered() && !effect_->params()[0].tooltip.empty()) {
            ImGui::SetTooltip("%s", effect_->params()[0].tooltip.c_str());
        }
    }
}

} // namespace Amplitron
