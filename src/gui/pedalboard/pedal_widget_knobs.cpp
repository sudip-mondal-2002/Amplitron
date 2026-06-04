#include "gui/pedalboard/pedal_widget.h"
#include "gui/views/gui_midi.h"
#include "midi/midi_manager.h"
#include "audio/engine/audio_engine.h"
#include "gui/theme/theme.h"
#include "gui/components/knob.h"

#include <cmath>
#include <algorithm>

namespace Amplitron {

void PedalWidget::render_knobs(ImDrawList* dl, ImVec2 p0, float pedal_width, bool is_amp, bool is_tuner, bool is_ir_cab, float zoom) {
    float knob_y_start = p0.y + Theme::KNOB_Y_START * zoom;
    if (is_ir_cab) knob_y_start = p0.y + 180 * zoom;
    auto& params = effect_->params();
    int num_params = is_tuner ? 0 : static_cast<int>(params.size());
    int param_offset = 0;
    if (is_amp) {
        param_offset = 1;
        num_params = std::max(0, num_params - 1);
    }

    float knob_radius    = Theme::KNOB_RADIUS * zoom;
    float knob_spacing_x = Theme::KNOB_SPACING_X * zoom;
    float knob_spacing_y = Theme::KNOB_SPACING_Y * zoom;

    float knob_grid_left = p0.x + (pedal_width - 2.0f * knob_spacing_x) * 0.5f;

    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 6.2831853f;
    constexpr float ARC_START = 2.356f;
    constexpr float ARC_RANGE = 4.712f;
    float knob_hit_size = knob_radius * Theme::KNOB_HIT_MULT;

    // --- REUSABLE KNOB HELPER (LAMBDA) ---
    auto render_custom_knob = [&](ImVec2 knob_center, int pi, bool disabled) {
        char label[64];
        snprintf(label, sizeof(label), "##knob_%s_%d_%d", effect_->name(), index_, pi);

        if (!disabled) {
            ImGui::SetCursorScreenPos(ImVec2(
                knob_center.x - knob_hit_size * 0.5f,
                knob_center.y - knob_hit_size * 0.5f));
            ImGui::SetNextItemAllowOverlap();
            ImGui::InvisibleButton(label, ImVec2(knob_hit_size, knob_hit_size));

            bool is_hovered = ImGui::IsItemHovered();
            bool is_active = ImGui::IsItemActive();

            float range = params[pi].max_val - params[pi].min_val;

            if (is_active && !knob_was_active_) {
                active_param_index_ = pi;
                param_value_before_drag_ = params[pi].value;
            }

            if (is_active) {
                float mdx = ImGui::GetIO().MouseDelta.x;
                float mdy = ImGui::GetIO().MouseDelta.y;

                if (mdx != 0.0f || mdy != 0.0f) {
                    ImVec2 mouse = ImGui::GetIO().MousePos;
                    float dx = mouse.x - knob_center.x;
                    float dy = mouse.y - knob_center.y;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    float value_delta = 0.0f;

                    if (dist > 5.0f && dist < knob_radius * 5.0f) {
                        float prev_x = mouse.x - mdx;
                        float prev_y = mouse.y - mdy;
                        float curr_angle = std::atan2(
                            mouse.y - knob_center.y, mouse.x - knob_center.x);
                        float prev_angle = std::atan2(
                            prev_y - knob_center.y, prev_x - knob_center.x);

                        float angle_delta = curr_angle - prev_angle;
                        if (angle_delta > PI)  angle_delta -= TWO_PI;
                        if (angle_delta < -PI) angle_delta += TWO_PI;

                        value_delta = (angle_delta / ARC_RANGE) * range;
                    } else {
                        float sensitivity = 0.007f;
                        value_delta = -mdy * sensitivity * range;
                    }

                    if (ImGui::GetIO().KeyShift) value_delta *= 0.2f;
                    if (ImGui::GetIO().KeyCtrl)  value_delta *= 3.0f;

                    float new_val = clamp(params[pi].value + value_delta, params[pi].min_val, params[pi].max_val);
                    if (new_val != params[pi].value) {
                        params[pi].value = new_val;
                        engine_.push_param_change(index_, pi, new_val);
                    }
                }
            }

            if (knob_was_active_ && !is_active && active_param_index_ == pi) {
                float new_val = params[pi].value;
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
                float old_val = params[pi].value;
                float step = range * 0.03f;
                if (ImGui::GetIO().KeyShift) step *= 0.2f;
                float new_val = clamp(params[pi].value + ImGui::GetIO().MouseWheel * step,
                                       params[pi].min_val, params[pi].max_val);
                if (new_val != old_val) {
                    params[pi].value = new_val;
                    engine_.push_param_change(index_, pi, new_val);
                    commit_param_change(pi, old_val, new_val);
                }
            }

            if (is_hovered && ImGui::IsMouseDoubleClicked(0)) {
                float old_val = params[pi].value;
                float new_val = params[pi].default_val;
                if (new_val != old_val) {
                    params[pi].value = new_val;
                    engine_.push_param_change(index_, pi, new_val);
                    commit_param_change(pi, old_val, new_val);
                }
            }

            if (is_hovered && ImGui::IsMouseClicked(1)) {
                ImGui::OpenPopup(label);
            }
            if (ImGui::BeginPopup(label)) {
                ImGui::Text("%s", params[pi].name.c_str());
                ImGui::SetNextItemWidth(120);
                float slider_val = params[pi].value;
                if (ImGui::SliderFloat("##edit", &slider_val,
                                   params[pi].min_val, params[pi].max_val, "%.2f")) {
                    params[pi].value = slider_val;
                    engine_.push_param_change(index_, pi, slider_val);
                }
                if (ImGui::IsItemActivated()) {
                    popup_active_param_index_ = pi;
                    popup_param_value_before_edit_ = params[pi].value;
                }
                if (ImGui::IsItemDeactivatedAfterEdit() && popup_active_param_index_ == pi) {
                    if (params[pi].value != popup_param_value_before_edit_) {
                        engine_.push_param_change(index_, pi, params[pi].value);
                        commit_param_change(pi, popup_param_value_before_edit_, params[pi].value);
                    }
                    popup_active_param_index_ = -1;
                }
                if (ImGui::Button("Reset")) {
                    float old_val = params[pi].value;
                    float new_val = params[pi].default_val;
                    if (new_val != old_val) {
                        params[pi].value = new_val;
                        engine_.push_param_change(index_, pi, new_val);
                        commit_param_change(pi, old_val, new_val);
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Separator();
                ImGui::TextColored(Theme::Gold(), "MIDI Control");
                
                if (gui_midi_) {
                    if (gui_midi_->render_remove_mapping_item(effect_->name(), params[pi].name)) {
                        ImGui::CloseCurrentPopup();
                    }
                    if (gui_midi_->render_learn_menu_item(effect_->name(), params[pi].name)) {
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
        }

        // Draw track
        float range = params[pi].max_val - params[pi].min_val;
        float normalized = (params[pi].value - params[pi].min_val) / range;
        float track_radius = knob_radius + 3 * zoom;
        int segments = 40;
        for (int s = 0; s < segments; ++s) {
            float t0 = static_cast<float>(s) / segments;
            float t1 = static_cast<float>(s + 1) / segments;
            float a0 = ARC_START + t0 * ARC_RANGE;
            float a1 = ARC_START + t1 * ARC_RANGE;

            bool filled = t0 <= normalized;
            ImU32 seg_color = disabled ? Theme::KNOB_TRACK_OFF :
                              (filled ? ImGui::ColorConvertFloat4ToU32(led_color_) : Theme::KNOB_TRACK_OFF);

            dl->AddLine(
                ImVec2(knob_center.x + std::cos(a0) * track_radius,
                       knob_center.y + std::sin(a0) * track_radius),
                ImVec2(knob_center.x + std::cos(a1) * track_radius,
                       knob_center.y + std::sin(a1) * track_radius),
                seg_color, 3.0f * zoom);
        }

        bool is_active = (active_param_index_ == pi) && !disabled;
        ImU32 knob_bg = disabled ? Theme::KNOB_FACE :
                        (is_active ? Theme::KNOB_ACTIVE : Theme::KNOB_FACE);
        dl->AddCircleFilled(knob_center, knob_radius, Theme::KNOB_BG);
        dl->AddCircleFilled(knob_center, knob_radius - 1 * zoom, knob_bg);

        // Flash blue border if currently learning this parameter
#ifndef AMPLITRON_NO_MIDI
        if (!disabled && gui_midi_ && gui_midi_->midi().is_learning() &&
            gui_midi_->midi().learn_effect_name() == effect_->name() &&
            gui_midi_->midi().learn_param_name() == params[pi].name) {
            float time = static_cast<float>(ImGui::GetTime());
            float alpha = (std::sin(time * 2.0f * 3.14159f * 10.0f) + 1.0f) * 0.5f;
            ImU32 outline_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.6f, 1.0f, 0.4f + alpha * 0.6f));
            dl->AddCircle(knob_center, knob_radius + 3 * zoom, outline_col, 0, 2.0f * zoom);
        }
#endif

        float pointer_angle = ARC_START + normalized * ARC_RANGE;
        float ptr_inner = knob_radius * 0.25f;
        float ptr_outer = knob_radius - 2 * zoom;
        ImVec2 ptr_from = ImVec2(
            knob_center.x + std::cos(pointer_angle) * ptr_inner,
            knob_center.y + std::sin(pointer_angle) * ptr_inner);
        ImVec2 ptr_to = ImVec2(
            knob_center.x + std::cos(pointer_angle) * ptr_outer,
            knob_center.y + std::sin(pointer_angle) * ptr_outer);

        ImU32 ptr_color = disabled ? Theme::TEXT_DIM : (is_active ? Theme::ACCENT_GOLD_HOT : Theme::ACCENT_GOLD);
        dl->AddLine(ptr_from, ptr_to, ptr_color, 2.5f * zoom);
        dl->AddCircleFilled(ptr_to, 3.0f * zoom, ptr_color);

        // Draw lock icon if disabled/synced
        if (disabled) {
            dl->AddCircle(knob_center, 4.0f * zoom, Theme::ACCENT_GOLD, 0, 1.5f * zoom);
            dl->AddLine(ImVec2(knob_center.x - 2.0f * zoom, knob_center.y),
                        ImVec2(knob_center.x + 2.0f * zoom, knob_center.y), Theme::ACCENT_GOLD, 1.5f * zoom);
        }

        // Labels
        const char* pname = params[pi].name.c_str();
        ImVec2 text_size = ImGui::CalcTextSize(pname);
        ImGui::SetCursorScreenPos(ImVec2(
            knob_center.x - text_size.x * 0.5f,
            knob_center.y + knob_radius + 8 * zoom));
        ImGui::PushStyleColor(ImGuiCol_Text, disabled ? Theme::TextDim() : Theme::TextSecondary());
        ImGui::TextUnformatted(pname);
        ImGui::PopStyleColor();

        std::string val_display = Theme::formatParameterValue(params[pi].value, params[pi].unit);
        ImVec2 val_size = ImGui::CalcTextSize(val_display.c_str());
        ImGui::SetCursorScreenPos(ImVec2(
            knob_center.x - val_size.x * 0.5f,
            knob_center.y - knob_radius - 20 * zoom));
        ImGui::PushStyleColor(ImGuiCol_Text, disabled ? Theme::TextDim() : (is_active ? Theme::GoldHot() : Theme::TextDim()));
        ImGui::TextUnformatted(val_display.c_str());
        ImGui::PopStyleColor();
    };

    // --- CHECK FOR DELAY OR CHORUS CUSTOM LAYOUTS ---
    bool is_delay = (std::strcmp(effect_->name(), "Delay") == 0);
    bool is_chorus = (std::strcmp(effect_->name(), "Chorus") == 0);

    if (is_delay && params.size() >= 6) {
        float kx_left = knob_grid_left + 0 * knob_spacing_x;
        float kx_right = knob_grid_left + 1 * knob_spacing_x;
        float ky_row1 = knob_y_start + 0 * knob_spacing_y;
        float ky_row2 = knob_y_start + 1 * knob_spacing_y;
        float ky_row3 = knob_y_start + 2 * knob_spacing_y;

        // Row 1 Left: Time knob
        bool sync_on = (params[4].value >= 0.5f);
        ImVec2 time_center = ImVec2(kx_left + knob_spacing_x * 0.5f, ky_row1 + knob_radius + 2 * zoom);
        render_custom_knob(time_center, 0, sync_on);

        // Row 1 Right: Sync / Tap / BPM
        float rx = kx_right + 10.0f * zoom;
        float ry = ky_row1 + 10.0f * zoom;
        
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        bool sync_val = sync_on;
        char sync_lbl[64];
        snprintf(sync_lbl, sizeof(sync_lbl), "Sync##sync_%d", index_);
        if (ImGui::Checkbox(sync_lbl, &sync_val)) {
            float new_val = sync_val ? 1.0f : 0.0f;
            params[4].value = new_val;
            engine_.push_param_change(index_, 4, new_val);
            commit_param_change(4, sync_on ? 1.0f : 0.0f, new_val);
        }

        ry += 22.0f * zoom;
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        char tap_lbl[64];
        snprintf(tap_lbl, sizeof(tap_lbl), "Tap##tap_%d", index_);
        if (ImGui::Button(tap_lbl, ImVec2(60.0f * zoom, 20.0f * zoom))) {
            pedal_tap_tempo_.tap(std::chrono::steady_clock::now());
            float new_bpm = pedal_tap_tempo_.get_bpm(std::chrono::steady_clock::now());
            if (new_bpm > 0.0f) {
                engine_.set_global_bpm(new_bpm);
                if (!sync_on) {
                    params[4].value = 1.0f;
                    engine_.push_param_change(index_, 4, 1.0f);
                    commit_param_change(4, 0.0f, 1.0f);
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Tap tempo repeatedly to sync delay");
        }

        ry += 24.0f * zoom;
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Gold());
        float global_bpm = engine_.get_global_bpm();
        ImGui::Text("♫ = %.1f", global_bpm);
        ImGui::PopStyleColor();

        // Row 2: Feedback & Tone
        ImVec2 fb_center = ImVec2(kx_left + knob_spacing_x * 0.5f, ky_row2 + knob_radius + 2 * zoom);
        render_custom_knob(fb_center, 1, false);

        ImVec2 tone_center = ImVec2(kx_right + knob_spacing_x * 0.5f, ky_row2 + knob_radius + 2 * zoom);
        render_custom_knob(tone_center, 2, false);

        // Row 3: Level & Subdivision
        ImVec2 lvl_center = ImVec2(kx_left + knob_spacing_x * 0.5f, ky_row3 + knob_radius + 2 * zoom);
        render_custom_knob(lvl_center, 3, false);

        float sub_x = kx_right + 5.0f * zoom;
        float sub_y = ky_row3 + 12.0f * zoom;
        ImGui::SetCursorScreenPos(ImVec2(sub_x, sub_y));
        ImGui::SetNextItemWidth(80.0f * zoom);
        int subdiv = static_cast<int>(params[5].value);
        const char* subdiv_items[] = { "1/4", "1/8", "1/16", "1/8D", "1/8T" };
        char subdiv_lbl[64];
        snprintf(subdiv_lbl, sizeof(subdiv_lbl), "##subdiv_%d", index_);
        if (ImGui::Combo(subdiv_lbl, &subdiv, subdiv_items, IM_ARRAYSIZE(subdiv_items))) {
            float new_val = static_cast<float>(subdiv);
            params[5].value = new_val;
            engine_.push_param_change(index_, 5, new_val);
            commit_param_change(5, params[5].value, new_val);
        }
        ImGui::SetCursorScreenPos(ImVec2(sub_x + 5.0f * zoom, sub_y + 24.0f * zoom));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
        ImGui::Text("Division");
        ImGui::PopStyleColor();

        return;
    }
    else if (is_chorus && params.size() >= 5) {
        float kx_left = knob_grid_left + 0 * knob_spacing_x;
        float kx_right = knob_grid_left + 1 * knob_spacing_x;
        float ky_row1 = knob_y_start + 0 * knob_spacing_y;
        float ky_row2 = knob_y_start + 1 * knob_spacing_y;
        float ky_row3 = knob_y_start + 2 * knob_spacing_y;

        // Row 1 Left: Rate knob
        bool sync_on = (params[3].value >= 0.5f);
        ImVec2 rate_center = ImVec2(kx_left + knob_spacing_x * 0.5f, ky_row1 + knob_radius + 2 * zoom);
        render_custom_knob(rate_center, 0, sync_on);

        // Row 1 Right: Sync / Tap / BPM
        float rx = kx_right + 10.0f * zoom;
        float ry = ky_row1 + 10.0f * zoom;
        
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        bool sync_val = sync_on;
        char sync_lbl[64];
        snprintf(sync_lbl, sizeof(sync_lbl), "Sync##sync_%d", index_);
        if (ImGui::Checkbox(sync_lbl, &sync_val)) {
            float new_val = sync_val ? 1.0f : 0.0f;
            params[3].value = new_val;
            engine_.push_param_change(index_, 3, new_val);
            commit_param_change(3, sync_on ? 1.0f : 0.0f, new_val);
        }

        ry += 22.0f * zoom;
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        char tap_lbl[64];
        snprintf(tap_lbl, sizeof(tap_lbl), "Tap##tap_%d", index_);
        if (ImGui::Button(tap_lbl, ImVec2(60.0f * zoom, 20.0f * zoom))) {
            pedal_tap_tempo_.tap(std::chrono::steady_clock::now());
            float new_bpm = pedal_tap_tempo_.get_bpm(std::chrono::steady_clock::now());
            if (new_bpm > 0.0f) {
                engine_.set_global_bpm(new_bpm);
                if (!sync_on) {
                    params[3].value = 1.0f;
                    engine_.push_param_change(index_, 3, 1.0f);
                    commit_param_change(3, 0.0f, 1.0f);
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Tap tempo repeatedly to sync chorus");
        }

        ry += 24.0f * zoom;
        ImGui::SetCursorScreenPos(ImVec2(rx, ry));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Gold());
        float global_bpm = engine_.get_global_bpm();
        ImGui::Text("♫ = %.1f", global_bpm);
        ImGui::PopStyleColor();

        // Row 2: Depth & Level
        ImVec2 depth_center = ImVec2(kx_left + knob_spacing_x * 0.5f, ky_row2 + knob_radius + 2 * zoom);
        render_custom_knob(depth_center, 1, false);

        ImVec2 lvl_center = ImVec2(kx_right + knob_spacing_x * 0.5f, ky_row2 + knob_radius + 2 * zoom);
        render_custom_knob(lvl_center, 2, false);

        // Row 3: Subdivision
        float sub_x = kx_left + 35.0f * zoom;
        float sub_y = ky_row3 + 12.0f * zoom;
        ImGui::SetCursorScreenPos(ImVec2(sub_x, sub_y));
        ImGui::SetNextItemWidth(80.0f * zoom);
        int subdiv = static_cast<int>(params[4].value);
        const char* subdiv_items[] = { "1/4", "1/8", "1/16", "1/8D", "1/8T" };
        char subdiv_lbl[64];
        snprintf(subdiv_lbl, sizeof(subdiv_lbl), "##subdiv_%d", index_);
        if (ImGui::Combo(subdiv_lbl, &subdiv, subdiv_items, IM_ARRAYSIZE(subdiv_items))) {
            float new_val = static_cast<float>(subdiv);
            params[4].value = new_val;
            engine_.push_param_change(index_, 4, new_val);
            commit_param_change(4, params[4].value, new_val);
        }
        ImGui::SetCursorScreenPos(ImVec2(sub_x + 5.0f * zoom, sub_y + 24.0f * zoom));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
        ImGui::Text("Division");
        ImGui::PopStyleColor();

        return;
    }

    for (int i = 0; i < num_params && i < 6; ++i) {
        int pi = i + param_offset;
        int col = i % 2;
        int row = i / 2;

        bool is_last_alone = (i == num_params - 1) && (num_params % 2 == 1);
        float kx = is_last_alone
            ? p0.x + (pedal_width - knob_spacing_x) * 0.5f
            : knob_grid_left + col * knob_spacing_x;
        float ky = knob_y_start + row * knob_spacing_y;

        ImVec2 knob_center = ImVec2(kx + knob_spacing_x * 0.5f, ky + knob_radius + 2 * zoom);

        char label[64];
        snprintf(label, sizeof(label), "##knob_%s_%d_%d", effect_->name(), index_, pi);

        KnobProps props;
        props.name = params[pi].name;
        props.value = params[pi].value;
        props.min_val = params[pi].min_val;
        props.max_val = params[pi].max_val;
        props.default_val = params[pi].default_val;
        props.unit = params[pi].unit;
        props.tooltip = params[pi].tooltip;

        // MIDI learn integration
        if (gui_midi_) {
            props.is_learning = gui_midi_->midi().is_learning() &&
                                gui_midi_->midi().learn_effect_name() == effect_->name() &&
                                gui_midi_->midi().learn_param_name() == params[pi].name;
            props.midi_info = gui_midi_->get_mapping_info(effect_->name(), params[pi].name);

            // Capture pointers for lambda closures
            std::string eff_name = effect_->name();
            std::string param_name = params[pi].name;
            GuiMidi* gm = gui_midi_;

            props.on_midi_learn_param = [gm, eff_name, param_name]() {
                gm->manager().start_learn(MidiTargetType::EffectParam, eff_name, param_name);
            };
            props.on_midi_clear_param = [gm, eff_name, param_name]() {
                gm->manager().remove_mapping_for_param(eff_name, param_name);
            };
            props.on_midi_learn_bypass = [gm, eff_name]() {
                gm->manager().start_learn(MidiTargetType::EffectBypass, eff_name, "");
            };
            props.on_midi_clear_bypass = [gm, eff_name]() {
                int remove_idx = -1;
                const auto& mappings = gm->manager().mappings();
                for (int i = 0; i < static_cast<int>(mappings.size()); ++i) {
                    if (mappings[i].target_type == MidiTargetType::EffectBypass &&
                        mappings[i].effect_name == eff_name) {
                        remove_idx = i;
                        break;
                    }
                }
                if (remove_idx >= 0) {
                    gm->manager().remove_mapping(remove_idx);
                }
            };
        }

        props.led_color = led_color_;

        // Events committed back to effect and engine
        props.on_value_changed = [this, pi](float new_val) {
            effect_->params()[pi].value = new_val;
            engine_.push_param_change(index_, pi, new_val);
        };
        props.on_value_committed = [this, pi](float old_val, float new_val) {
            commit_param_change(pi, old_val, new_val);
        };

        KnobComponent::render(label, props, zoom, knob_center);
    }
}

} // namespace Amplitron
