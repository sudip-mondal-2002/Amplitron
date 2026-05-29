#include "gui/pedalboard/pedal_widget.h"
#include "gui/views/gui_midi.h"
#include "audio/engine/audio_engine.h"
#include "gui/theme/theme.h"
#include "gui/commands/command.h"
#include "gui/commands/command_history.h"
#include "audio/effects/tuner.h"
#include "audio/effects/amp_simulator.h"
#include "gui/dialogs/file_dialog.h"
#include "gui/components/led.h"
#include "gui/components/footswitch.h"
#include "gui/components/screen.h"
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
bool PedalWidget::render(float zoom) {
    bool should_remove = false;

    ImGui::PushID(index_);

    bool is_amp = (std::strcmp(effect_->name(), "Amp Sim") == 0);
    bool is_mb_comp = (std::strcmp(effect_->name(), "MultiBand Compressor") == 0);
    bool enabled = effect_->is_enabled();
    bool is_looper = !is_amp && (std::strcmp(effect_->name(), "Looper") == 0);

    float pedal_width = is_mb_comp ? (Theme::PEDAL_WIDTH * 2.2f * zoom) : (Theme::PEDAL_WIDTH * zoom);
    float pedal_height = Theme::PEDAL_HEIGHT * zoom;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Pedal body
    ImVec2 p0 = cursor;
    ImVec2 p1 = ImVec2(cursor.x + pedal_width, cursor.y + pedal_height);

    // Shadow
    dl->AddRectFilled(
        ImVec2(p0.x + 4 * zoom, p0.y + 4 * zoom),
        ImVec2(p1.x + 4 * zoom, p1.y + 4 * zoom),
        Theme::PEDAL_SHADOW, Theme::ROUNDING_MD * zoom
    );

    if (is_amp) {
        render_amp_cabinet(dl, p0, p1, pedal_width, pedal_height, zoom);
    } else {
        render_standard_pedal(dl, p0, p1, pedal_width, enabled, zoom);
    }

    // Dim the pedal body when bypassed so the inactive state is immediately obvious
    if (!enabled && !is_amp) {
        dl->AddRectFilled(p0, p1, Theme::PEDAL_BYPASS_OVERLAY, Theme::ROUNDING_MD * zoom);
    }

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
        render_knobs(dl, p0, pedal_width, is_amp, is_tuner, is_ir_cab, zoom);
    }

    render_footswitch_and_extras(dl, p0, p1, pedal_width, pedal_height, is_amp, enabled, should_remove, zoom);

    ImGui::PopID();
    return should_remove;
}

void PedalWidget::render_standard_pedal(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, bool enabled, float zoom) {
    // ========== STANDARD PEDAL VISUAL ==========
    ImU32 body_color = ImGui::ColorConvertFloat4ToU32(pedal_color_);
    dl->AddRectFilled(p0, p1, body_color, Theme::ROUNDING_MD * zoom);
    dl->AddRect(p0, p1, Theme::PEDAL_BORDER, Theme::ROUNDING_MD * zoom, 0, 2.0f * zoom);

    // Metallic top plate
    ImVec2 plate_p0 = ImVec2(p0.x + 8 * zoom, p0.y + 8 * zoom);
    ImVec2 plate_p1 = ImVec2(p1.x - 8 * zoom, p0.y + 45 * zoom);
    dl->AddRectFilled(plate_p0, plate_p1,
        Theme::PEDAL_PLATE, Theme::ROUNDING_SM * zoom);

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

void PedalWidget::render_footswitch_and_extras(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, float pedal_height, bool is_amp, bool enabled, bool& should_remove, float zoom) {
    (void)p1;
    (void)should_remove;
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
    auto cmd = std::make_unique<ParameterChangeCommand>(
        engine_, effect_, param_index, old_val, new_val);
    history_->push_executed(std::move(cmd));
}

} // namespace Amplitron
