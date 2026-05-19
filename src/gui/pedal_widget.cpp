#include "gui/pedal_widget.h"
#include "gui/gui_midi.h"
#include "audio/audio_engine.h"
#include "gui/theme.h"
#include "gui/command.h"
#include "gui/command_history.h"
#include "audio/effects/tuner.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/ir_cabinet.h"
#include "gui/file_dialog.h"
#include <cstring>
#include <cmath>

namespace Amplitron {

/** @brief Construct PedalWidget and look up color scheme for the effect type. */
PedalWidget::PedalWidget(AudioEngine& engine, std::shared_ptr<Effect> effect, int index)
    : engine_(engine), effect_(std::move(effect)), index_(index) {
    assign_colors();
}

/** @brief Look up pedal_color_ and led_color_ from the theme's effect color table. */
void PedalWidget::assign_colors() {
    const auto* entry = get_effect_color(effect_->name());
    pedal_color_ = entry->pedal_color;
    led_color_ = entry->led_color;
}

/** @brief Render the full pedal widget (body, knobs, switch, LED). @return true if remove requested. */
bool PedalWidget::render() {
    bool should_remove = false;

    ImGui::PushID(index_);

    float pedal_width = Theme::PEDAL_WIDTH;
    float pedal_height = Theme::PEDAL_HEIGHT;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool is_amp = (std::strcmp(effect_->name(), "Amp Sim") == 0);
    bool enabled = effect_->is_enabled();
    bool is_looper = !is_amp && (std::strcmp(effect_->name(), "Looper") == 0);

    // Pedal body
    ImVec2 p0 = cursor;
    ImVec2 p1 = ImVec2(cursor.x + pedal_width, cursor.y + pedal_height);

    // Shadow
    dl->AddRectFilled(
        ImVec2(p0.x + 4, p0.y + 4),
        ImVec2(p1.x + 4, p1.y + 4),
        Theme::PEDAL_SHADOW, Theme::ROUNDING_MD
    );

    if (is_amp) {
        render_amp_cabinet(dl, p0, p1, pedal_width, pedal_height);
    } else {
        render_standard_pedal(dl, p0, p1, pedal_width, enabled);
    }

    // Dim the pedal body when bypassed so the inactive state is immediately obvious
    if (!enabled && !is_amp) {
        dl->AddRectFilled(p0, p1, Theme::PEDAL_BYPASS_OVERLAY, Theme::ROUNDING_MD);
    }

    // --- Tuner custom display ---
    bool is_tuner = !is_amp && (std::strcmp(effect_->name(), "Tuner") == 0);
    if (is_tuner) {
        render_tuner_display(dl, p0, pedal_width);
    }

    // --- IR Cabinet custom display ---
    bool is_ir_cab = !is_amp && (std::strcmp(effect_->name(), "IR Cabinet") == 0);
    if (is_ir_cab) {
        render_ir_cabinet_display(p0, pedal_width);
    }

    if (is_looper) {
        render_looper_display(p0, pedal_width);
    } else {
        render_knobs(dl, p0, pedal_width, is_amp, is_tuner, is_ir_cab);
    }

    render_footswitch_and_extras(dl, p0, p1, pedal_width, pedal_height, is_amp, enabled, should_remove);

    // Advance cursor for next pedal
    ImGui::SetCursorScreenPos(ImVec2(p0.x + pedal_width + 15, cursor.y));

    ImGui::PopID();
    return should_remove;
}



void PedalWidget::render_standard_pedal(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, bool enabled) {
    // ========== STANDARD PEDAL VISUAL ==========
        ImU32 body_color = ImGui::ColorConvertFloat4ToU32(pedal_color_);
        dl->AddRectFilled(p0, p1, body_color, Theme::ROUNDING_MD);
        dl->AddRect(p0, p1, Theme::PEDAL_BORDER, Theme::ROUNDING_MD, 0, 2.0f);

        // Metallic top plate
        ImVec2 plate_p0 = ImVec2(p0.x + 8, p0.y + 8);
        ImVec2 plate_p1 = ImVec2(p1.x - 8, p0.y + 45);
        dl->AddRectFilled(plate_p0, plate_p1,
            Theme::PEDAL_PLATE, Theme::ROUNDING_SM);

        // Effect name
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 12, p0.y + 14));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextPrimary());
        ImGui::Text("%s", effect_->name());
        ImGui::PopStyleColor();

        // LED indicator
        float led_x = p0.x + pedal_width - 25;
        float led_y = p0.y + 20;
        ImU32 led_col = enabled ?
            ImGui::ColorConvertFloat4ToU32(led_color_) :
            Theme::LED_OFF;
        dl->AddCircleFilled(ImVec2(led_x, led_y), 6, led_col);
        if (enabled) {
            dl->AddCircleFilled(ImVec2(led_x, led_y), 10,
                IM_COL32(
                    static_cast<int>(led_color_.x * 255),
                    static_cast<int>(led_color_.y * 255),
                    static_cast<int>(led_color_.z * 255),
                    40
                ));
        }

}




void PedalWidget::render_footswitch_and_extras(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, float pedal_height, bool is_amp, bool enabled, bool& should_remove) {
    bool is_looper = !is_amp && (std::strcmp(effect_->name(), "Looper") == 0);

    // LED tooltip — hover area over the LED indicator
    if (!is_amp && !is_looper) {
        float led_x = p0.x + pedal_width - 25;
        float led_y = p0.y + 20;
        ImGui::SetCursorScreenPos(ImVec2(led_x - 10, led_y - 10));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##led_tip", ImVec2(20, 20));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(enabled ? "Effect active" : "Effect bypassed");
        }
    }

    // Footswitch (toggle on/off) — amps are always on, no footswitch
    if (!is_amp && !is_looper) {
        float switch_y = p0.y + pedal_height - Theme::SWITCH_BOTTOM_OFFSET;
        float switch_x = p0.x + (pedal_width - 50) / 2;
        ImGui::SetCursorScreenPos(ImVec2(switch_x, switch_y));

        // Draw footswitch
        ImVec2 sw_center = ImVec2(switch_x + 25, switch_y + 15);
        dl->AddCircleFilled(sw_center, 18, Theme::SWITCH_BODY);
        dl->AddCircle(sw_center, 18, Theme::SWITCH_RING, 0, 2.0f);
        dl->AddCircleFilled(sw_center, 12,
            enabled ? Theme::SWITCH_ACTIVE : Theme::SWITCH_IDLE);

        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##switch", ImVec2(50, 30));
        if (ImGui::IsItemClicked()) {
            bool new_enabled = !enabled;
            effect_->set_enabled(new_enabled);
            engine_.push_effect_enabled(index_, new_enabled ? 1.0f : 0.0f);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(enabled ? "Click to bypass" : "Click to enable");
        }
    }

    // Remove button (small X at top-right) — not shown for amp
    if (!is_amp) {
        ImGui::SetCursorScreenPos(ImVec2(p1.x - 22, p0.y + 2));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.1f, 0.1f, 0.8f));
        char remove_label[32];
        snprintf(remove_label, sizeof(remove_label), "X##rm%d", index_);
        if (ImGui::SmallButton(remove_label)) {
            should_remove = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Remove %s from chain", effect_->name());
        }
        ImGui::PopStyleColor(2);
    }
}

void PedalWidget::commit_param_change(int param_index, float old_val, float new_val) {
    if (!history_) return;
    auto cmd = std::make_unique<ParameterChangeCommand>(
        engine_, effect_, param_index, old_val, new_val);
    history_->push_executed(std::move(cmd));
}

} // namespace Amplitron
