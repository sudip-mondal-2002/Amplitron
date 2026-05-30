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
