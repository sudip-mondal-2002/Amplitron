#include "gui/pedal_widget.h"
#include "gui/gui_midi.h"
#include "midi/midi_manager.h"
#include "audio/audio_engine.h"
#include "gui/theme.h"

#include <cmath>

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
    float knob_hit_size  = knob_radius * Theme::KNOB_HIT_MULT;

    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 6.28318530f;
    constexpr float ARC_START = 2.356f;
    constexpr float ARC_RANGE = 4.712f;

    float knob_grid_left = p0.x + (pedal_width - 2.0f * knob_spacing_x) * 0.5f;

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

        // ============================================================
        // RIGHT-CLICK POPUP — ENHANCED WITH MIDI LEARN
        // ============================================================
        if (is_hovered && ImGui::IsMouseClicked(1)) {
            ImGui::OpenPopup(label);
        }
        if (ImGui::BeginPopup(label)) {
            // --- Parameter name and value editor ---
            ImGui::Text("%s", params[pi].name.c_str());
            ImGui::SetNextItemWidth(120);
            float slider_val = params[pi].value;
            ImGui::SliderFloat("##edit", &slider_val,
                               params[pi].min_val, params[pi].max_val, "%.2f");
            if (slider_val != params[pi].value) {
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

            // ============================================================
            // MIDI CONTROL SECTION
            // ============================================================
            ImGui::Separator();
            ImGui::TextColored(Theme::Gold(), "MIDI Control");
            
            if (gui_midi_) {
                // Parameter Range mapping
                if (gui_midi_->render_remove_mapping_item(effect_->name(), params[pi].name)) {
                    ImGui::CloseCurrentPopup();
                }
                if (gui_midi_->render_learn_menu_item(effect_->name(), params[pi].name)) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Spacing();

                // Bypass Toggle mapping
                if (gui_midi_->render_remove_bypass_item(effect_->name())) {
                    ImGui::CloseCurrentPopup();
                }
                if (gui_midi_->render_learn_bypass_item(effect_->name())) {
                    ImGui::CloseCurrentPopup();
                }
            } else {
                ImGui::TextDisabled("MIDI manager not available");
            }

            // ============================================================
            // END MIDI SECTION
            // ============================================================

            ImGui::EndPopup();
        }

        float normalized = (params[pi].value - params[pi].min_val) / range;

        float track_radius = knob_radius + 3 * zoom;
        int segments = 40;
        for (int s = 0; s < segments; ++s) {
            float t0 = static_cast<float>(s) / segments;
            float t1 = static_cast<float>(s + 1) / segments;
            float a0 = ARC_START + t0 * ARC_RANGE;
            float a1 = ARC_START + t1 * ARC_RANGE;

            bool filled = t0 <= normalized;
            ImU32 seg_color = filled ?
                ImGui::ColorConvertFloat4ToU32(led_color_) :
                Theme::KNOB_TRACK_OFF;

            dl->AddLine(
                ImVec2(knob_center.x + std::cos(a0) * track_radius,
                       knob_center.y + std::sin(a0) * track_radius),
                ImVec2(knob_center.x + std::cos(a1) * track_radius,
                       knob_center.y + std::sin(a1) * track_radius),
                seg_color, 3.0f * zoom);
        }

        ImU32 knob_bg = is_active ? Theme::KNOB_ACTIVE :
                        is_hovered ? Theme::KNOB_HOVER :
                                     Theme::KNOB_FACE;
        dl->AddCircleFilled(knob_center, knob_radius, Theme::KNOB_BG);
        dl->AddCircleFilled(knob_center, knob_radius - 1 * zoom, knob_bg);

        // Flash blue border if currently learning this parameter
#ifndef AMPLITRON_NO_MIDI
        if (gui_midi_ && gui_midi_->midi().is_learning() &&
            gui_midi_->midi().learn_effect_name() == effect_->name() &&
            gui_midi_->midi().learn_param_name() == params[pi].name) {
            float time = static_cast<float>(ImGui::GetTime());
            constexpr float LEARN_FLASH_HZ = 10.0f;
            float alpha = (std::sin(time * 2.0f * 3.14159f * LEARN_FLASH_HZ) + 1.0f) * 0.5f;
            ImU32 outline_col = ImGui::ColorConvertFloat4ToU32(
                ImVec4(0.2f, 0.6f, 1.0f, 0.4f + alpha * 0.6f));
            dl->AddCircle(knob_center, knob_radius + 4.0f * zoom, outline_col, 0, 3.0f * zoom);
        }
#endif

        float pointer_angle = ARC_START + normalized * ARC_RANGE;
        float ptr_inner = knob_radius * 0.25f;
        float ptr_outer = knob_radius - 3.0f * zoom;
        ImVec2 ptr_from = ImVec2(
            knob_center.x + std::cos(pointer_angle) * ptr_inner,
            knob_center.y + std::sin(pointer_angle) * ptr_inner);
        ImVec2 ptr_to = ImVec2(
            knob_center.x + std::cos(pointer_angle) * ptr_outer,
            knob_center.y + std::sin(pointer_angle) * ptr_outer);

        ImU32 ptr_color = is_active ?
            Theme::ACCENT_GOLD_HOT :
            Theme::ACCENT_GOLD;
        dl->AddLine(ptr_from, ptr_to, ptr_color, 2.5f * zoom);
        dl->AddCircleFilled(ptr_to, 3.0f * zoom, ptr_color);

        // Enhanced tooltip with MIDI info
        if (is_hovered || is_active) {
            std::string val_str  = Theme::formatParameterValue(params[pi].value, params[pi].unit);
            std::string min_str  = Theme::formatParameterValue(params[pi].min_val, params[pi].unit);
            std::string max_str  = Theme::formatParameterValue(params[pi].max_val, params[pi].unit);
            
            // Check for MIDI mapping to show in tooltip
            std::string midi_info = "";
            if (gui_midi_) {
                midi_info = gui_midi_->get_mapping_info(effect_->name(), params[pi].name);
            }
            
            if (params[pi].tooltip.empty()) {
                ImGui::SetTooltip("%s: %s\nRange: [%s, %s]%s\n\nRotate or drag to adjust\nScroll wheel also works\nShift=fine  Ctrl=coarse\nDbl-click=reset  Right-click=edit/MIDI",
                    params[pi].name.c_str(), val_str.c_str(), min_str.c_str(), max_str.c_str(),
                    midi_info.c_str());
            } else {
                ImGui::SetTooltip("%s: %s\nRange: [%s, %s]\n\n%s%s\n\nRotate or drag to adjust\nScroll wheel also works\nShift=fine  Ctrl=coarse\nDbl-click=reset  Right-click=edit/MIDI",
                    params[pi].name.c_str(), val_str.c_str(), min_str.c_str(), max_str.c_str(),
                    params[pi].tooltip.c_str(), midi_info.c_str());
            }
        }

        const char* pname = params[pi].name.c_str();
        ImVec2 text_size = ImGui::CalcTextSize(pname);
        ImGui::SetCursorScreenPos(ImVec2(
            knob_center.x - text_size.x * 0.5f,
            knob_center.y + knob_radius + 8 * zoom));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
        ImGui::TextUnformatted(pname);
        ImGui::PopStyleColor();

        std::string val_display = Theme::formatParameterValue(params[pi].value, params[pi].unit);
        ImVec2 val_size = ImGui::CalcTextSize(val_display.c_str());
        ImGui::SetCursorScreenPos(ImVec2(
            knob_center.x - val_size.x * 0.5f,
            knob_center.y - knob_radius - 20 * zoom));
        ImGui::PushStyleColor(ImGuiCol_Text,
            is_active ? Theme::GoldHot() :
                        Theme::TextDim());
        ImGui::TextUnformatted(val_display.c_str());
        ImGui::PopStyleColor();
    }
}

} // namespace Amplitron
