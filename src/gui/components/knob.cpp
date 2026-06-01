#include "gui/components/knob.h"
#include "gui/theme/theme.h"
#include "common.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace Amplitron {

// Static variables to track active knob drag states across frames for accurate undo commitment
static bool s_knob_was_active = false;
static float s_param_value_before_drag = 0.0f;
static std::string s_active_knob_id = "";

static float s_popup_param_value_before_edit = 0.0f;
static std::string s_active_popup_id = "";

void KnobComponent::render(const char* imgui_id, const KnobProps& props, float zoom, ImVec2 center) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float knob_radius = Theme::KNOB_RADIUS * zoom;
    float knob_hit_size = knob_radius * Theme::KNOB_HIT_MULT;

    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 6.28318530f;
    constexpr float ARC_START = 2.356f;
    constexpr float ARC_RANGE = 4.712f;

    ImGui::SetCursorScreenPos(ImVec2(
        center.x - knob_hit_size * 0.5f,
        center.y - knob_hit_size * 0.5f));
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton(imgui_id, ImVec2(knob_hit_size, knob_hit_size));

    bool is_hovered = ImGui::IsItemHovered();
    bool is_active = ImGui::IsItemActive();

    float range = props.max_val - props.min_val;
    if (range <= 0.0001f) range = 1.0f;

    // 1. Mouse Drag Start
    if (is_active && !s_knob_was_active) {
        s_knob_was_active = true;
        s_param_value_before_drag = props.value;
        s_active_knob_id = imgui_id;
    }

    // 2. Mouse Panning/Rotary Drag Action
    if (is_active && s_active_knob_id == imgui_id) {
        float mdx = ImGui::GetIO().MousePos.x - ImGui::GetIO().MousePosPrev.x;
        float mdy = ImGui::GetIO().MousePos.y - ImGui::GetIO().MousePosPrev.y;

        if (mdx != 0.0f || mdy != 0.0f) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            float dx = mouse.x - center.x;
            float dy = mouse.y - center.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            float value_delta = 0.0f;
            if (dist > 5.0f && dist < knob_radius * 5.0f) {
                float prev_x = mouse.x - mdx;
                float prev_y = mouse.y - mdy;
                float curr_angle = std::atan2(mouse.y - center.y, mouse.x - center.x);
                float prev_angle = std::atan2(prev_y - center.y, prev_x - center.x);

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

            float new_val = clamp(props.value + value_delta, props.min_val, props.max_val);
            if (new_val != props.value && props.on_value_changed) {
                props.on_value_changed(new_val);
            }
        }
    }

    // 3. Mouse Drag Stop & Commit Undo
    if (s_knob_was_active && !is_active && s_active_knob_id == imgui_id) {
        float new_val = props.value;
        if (new_val != s_param_value_before_drag && props.on_value_committed) {
            props.on_value_committed(s_param_value_before_drag, new_val);
        }
        s_knob_was_active = false;
        s_active_knob_id = "";
    }

    // 4. Scroll Wheel Adjustments
    if (is_hovered && std::fabs(ImGui::GetIO().MouseWheel) > 0.0f) {
        float old_val = props.value;
        float step = range * 0.03f;
        if (ImGui::GetIO().KeyShift) step *= 0.2f;
        float new_val = clamp(props.value + ImGui::GetIO().MouseWheel * step, props.min_val, props.max_val);
        if (new_val != old_val) {
            if (props.on_value_changed) props.on_value_changed(new_val);
            if (props.on_value_committed) props.on_value_committed(old_val, new_val);
        }
    }

    // 5. Double Click Reset to Default
    if (is_hovered && ImGui::IsMouseDoubleClicked(0)) {
        float old_val = props.value;
        float new_val = props.default_val;
        if (new_val != old_val) {
            if (props.on_value_changed) props.on_value_changed(new_val);
            if (props.on_value_committed) props.on_value_committed(old_val, new_val);
        }
    }

    // 6. Right-Click context popup sliders and MIDI options
    char popup_id[128];
    std::snprintf(popup_id, sizeof(popup_id), "Popup_%s", imgui_id);

    if (is_hovered && ImGui::IsMouseClicked(1)) {
        ImGui::OpenPopup(popup_id);
    }

    if (ImGui::BeginPopup(popup_id)) {
        ImGui::Text("%s", props.name.c_str());
        ImGui::SetNextItemWidth(120);

        float slider_val = props.value;
        ImGui::SliderFloat("##edit", &slider_val, props.min_val, props.max_val, "%.2f");
        if (slider_val != props.value && props.on_value_changed) {
            props.on_value_changed(slider_val);
        }

        if (ImGui::IsItemActivated()) {
            s_active_popup_id = popup_id;
            s_popup_param_value_before_edit = props.value;
        }

        if (ImGui::IsItemDeactivatedAfterEdit() && s_active_popup_id == popup_id) {
            if (props.value != s_popup_param_value_before_edit && props.on_value_committed) {
                props.on_value_committed(s_popup_param_value_before_edit, props.value);
            }
            s_active_popup_id = "";
        }

        if (ImGui::Button("Reset")) {
            float old_val = props.value;
            float new_val = props.default_val;
            if (new_val != old_val) {
                if (props.on_value_changed) props.on_value_changed(new_val);
                if (props.on_value_committed) props.on_value_committed(old_val, new_val);
            }
            ImGui::CloseCurrentPopup();
        }

        // MIDI learn integrations
        ImGui::Separator();
        ImGui::TextColored(Theme::Gold(), "MIDI Control");

        if (props.on_midi_learn_param) {
            if (props.midi_info.empty()) {
                if (ImGui::MenuItem("MIDI Learn Parameter")) {
                    props.on_midi_learn_param();
                    ImGui::CloseCurrentPopup();
                }
            } else {
                if (ImGui::MenuItem("Remove MIDI Mapping")) {
                    props.on_midi_clear_param();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::Spacing();
            if (ImGui::MenuItem("MIDI Learn Bypass Toggle")) {
                props.on_midi_learn_bypass();
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Remove Bypass Mapping")) {
                props.on_midi_clear_bypass();
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::TextDisabled("MIDI mapping not available");
        }

        ImGui::EndPopup();
    }

    // 7. Visual calculations and drawings
    float normalized = (props.value - props.min_val) / range;
    float track_radius = knob_radius + 3 * zoom;
    int segments = 40;

    for (int s = 0; s < segments; ++s) {
        float t0 = static_cast<float>(s) / segments;
        float t1 = static_cast<float>(s + 1) / segments;
        float a0 = ARC_START + t0 * ARC_RANGE;
        float a1 = ARC_START + t1 * ARC_RANGE;

        bool filled = t0 <= normalized;
        ImU32 seg_color = filled ? ImGui::ColorConvertFloat4ToU32(props.led_color) : Theme::KNOB_TRACK_OFF;

        dl->AddLine(
            ImVec2(center.x + std::cos(a0) * track_radius, center.y + std::sin(a0) * track_radius),
            ImVec2(center.x + std::cos(a1) * track_radius, center.y + std::sin(a1) * track_radius),
            seg_color, 3.0f * zoom);
    }

    ImU32 knob_bg = is_active ? Theme::KNOB_ACTIVE : (is_hovered ? Theme::KNOB_HOVER : Theme::KNOB_FACE);
    dl->AddCircleFilled(center, knob_radius, Theme::KNOB_BG);
    dl->AddCircleFilled(center, knob_radius - 1 * zoom, knob_bg);

    // Flash blue outline if learning MIDI
    if (props.is_learning) {
        float time = static_cast<float>(ImGui::GetTime());
        float alpha = (std::sin(time * 2.0f * 3.14159f * 10.0f) + 1.0f) * 0.5f;
        ImU32 outline_col = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.6f, 1.0f, 0.4f + alpha * 0.6f));
        dl->AddCircle(center, knob_radius + 4.0f * zoom, outline_col, 0, 3.0f * zoom);
    }

    // Drawing the pointer pointer dot/line
    float pointer_angle = ARC_START + normalized * ARC_RANGE;
    float ptr_inner = knob_radius * 0.25f;
    float ptr_outer = knob_radius - 3.0f * zoom;
    ImVec2 ptr_from = ImVec2(center.x + std::cos(pointer_angle) * ptr_inner, center.y + std::sin(pointer_angle) * ptr_inner);
    ImVec2 ptr_to = ImVec2(center.x + std::cos(pointer_angle) * ptr_outer, center.y + std::sin(pointer_angle) * ptr_outer);

    ImU32 ptr_color = is_active ? Theme::ACCENT_GOLD_HOT : Theme::ACCENT_GOLD;
    dl->AddLine(ptr_from, ptr_to, ptr_color, 2.5f * zoom);
    dl->AddCircleFilled(ptr_to, 3.0f * zoom, ptr_color);

    // Tooltips
    if (is_hovered || is_active) {
        std::string val_str = Theme::formatParameterValue(props.value, props.unit);
        std::string min_str = Theme::formatParameterValue(props.min_val, props.unit);
        std::string max_str = Theme::formatParameterValue(props.max_val, props.unit);

        if (props.tooltip.empty()) {
            ImGui::SetTooltip("%s: %s\nRange: [%s, %s]%s\n\nRotate or drag to adjust\nScroll wheel also works\nShift=fine  Ctrl=coarse\nDbl-click=reset  Right-click=edit/MIDI",
                props.name.c_str(), val_str.c_str(), min_str.c_str(), max_str.c_str(), props.midi_info.c_str());
        } else {
            ImGui::SetTooltip("%s: %s\nRange: [%s, %s]\n\n%s%s\n\nRotate or drag to adjust\nScroll wheel also works\nShift=fine  Ctrl=coarse\nDbl-click=reset  Right-click=edit/MIDI",
                props.name.c_str(), val_str.c_str(), min_str.c_str(), max_str.c_str(), props.tooltip.c_str(), props.midi_info.c_str());
        }
    }

    // Parameter names and values text
    ImVec2 text_size = ImGui::CalcTextSize(props.name.c_str());
    ImGui::SetCursorScreenPos(ImVec2(center.x - text_size.x * 0.5f, center.y + knob_radius + 8 * zoom));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextSecondary());
    ImGui::TextUnformatted(props.name.c_str());
    ImGui::PopStyleColor();

    std::string val_display = Theme::formatParameterValue(props.value, props.unit);
    ImVec2 val_size = ImGui::CalcTextSize(val_display.c_str());
    ImGui::SetCursorScreenPos(ImVec2(center.x - val_size.x * 0.5f, center.y - knob_radius - 20 * zoom));
    ImGui::PushStyleColor(ImGuiCol_Text, is_active ? Theme::GoldHot() : Theme::TextDim());
    ImGui::TextUnformatted(val_display.c_str());
    ImGui::PopStyleColor();
}

} // namespace Amplitron
