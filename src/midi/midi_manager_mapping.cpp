#include "midi/midi_manager.h"
#include "audio/engine/audio_engine.h"

namespace Amplitron {

namespace {
int find_node_id_for_effect(IAudioEngine& engine, const std::shared_ptr<Effect>& effect, int fallback_id) {
    for (const auto& node : engine.graph().get_nodes()) {
        if (node.pedal == effect) {
            return node.id;
        }
    }
    return fallback_id;
}
}

// ---------------------------------------------------------------------------
// Mapping management
// ---------------------------------------------------------------------------

void MidiManager::add_mapping(const MidiMapping& mapping) {
    // Remove any existing mapping with the same CC + channel
    for (auto it = mappings_.begin(); it != mappings_.end(); ++it) {
        if (it->cc_number == mapping.cc_number &&
            it->midi_channel == mapping.midi_channel) {
            mappings_.erase(it);
            break;
        }
    }
    mappings_.push_back(mapping);
}

void MidiManager::remove_mapping(int index) {
    if (index >= 0 && index < static_cast<int>(mappings_.size())) {
        mappings_.erase(mappings_.begin() + index);
    }
}

void MidiManager::remove_mapping_for_param(const std::string& effect_name,
                                           const std::string& param_name) {
    for (auto it = mappings_.begin(); it != mappings_.end(); ++it) {
        if (it->target_type == MidiTargetType::EffectParam &&
            it->effect_name == effect_name &&
            it->param_name == param_name) {
            mappings_.erase(it);
            return;
        }
    }
}

void MidiManager::clear_mappings() {
    mappings_.clear();
}

void MidiManager::install_default_mappings() {
    MidiMapping cc7;
    cc7.cc_number = 7;
    cc7.midi_channel = -1;
    cc7.target_type = MidiTargetType::OutputGain;
    cc7.mode = MidiMappingMode::Continuous;
    add_mapping(cc7);

    MidiMapping cc11;
    cc11.cc_number = 11;
    cc11.midi_channel = -1;
    cc11.target_type = MidiTargetType::InputGain;
    cc11.mode = MidiMappingMode::Continuous;
    add_mapping(cc11);

    MidiMapping cc64;
    cc64.cc_number = 64;
    cc64.midi_channel = -1;
    cc64.target_type = MidiTargetType::EffectBypass;
    cc64.mode = MidiMappingMode::Toggle;
    cc64.effect_name = "AmpSimulator";
    add_mapping(cc64);

    MidiMapping cc74;
    cc74.cc_number = 74;
    cc74.midi_channel = -1;
    cc74.target_type = MidiTargetType::EffectParam;
    cc74.mode = MidiMappingMode::Continuous;
    cc74.effect_name = "WahPedal";
    cc74.param_name = "Sweep";
    add_mapping(cc74);

#ifdef __EMSCRIPTEN__
    // Web-specific MIDI defaults
    
    // CC11 (Expression pedal) → Output Gain
    MidiMapping cc11_output;
    cc11_output.cc_number = 11;
    cc11_output.midi_channel = -1;  // Respond on any channel
    cc11_output.target_type = MidiTargetType::OutputGain;
    cc11_output.mode = MidiMappingMode::Continuous;
    add_mapping(cc11_output);
    
    // CC7 (Volume) → Also Output Gain (alternative)
    MidiMapping cc7_output;
    cc7_output.cc_number = 7;
    cc7_output.midi_channel = -1;
    cc7_output.target_type = MidiTargetType::OutputGain;
    cc7_output.mode = MidiMappingMode::Continuous;
    add_mapping(cc7_output);
    
    // CC64 (Sustain/Damper pedal) → Bypass toggle
    // (Already implemented as EffectBypass for AmpSimulator above, 
    // but redefined here explicitly for Web defaults)

    // CC64 (Sustain) → acts as bypass via OutputGain toggle (web fallback)
    
    // CC1 (Modulation) → EffectParam (e.g., Chorus Depth)
    MidiMapping cc1_mod;
    cc1_mod.cc_number = 1;
    cc1_mod.midi_channel = -1;
    cc1_mod.target_type = MidiTargetType::EffectParam;
    cc1_mod.mode = MidiMappingMode::Continuous;
    cc1_mod.effect_name = "Chorus";
    cc1_mod.param_name = "Depth";
    add_mapping(cc1_mod);

    MidiMapping cc64_bypass;
    cc64_bypass.cc_number = 64;
    cc64_bypass.midi_channel = -1;
    cc64_bypass.target_type = MidiTargetType::EffectBypass;
    cc64_bypass.mode = MidiMappingMode::Toggle;
    cc64_bypass.effect_name = "AmpSimulator";
    add_mapping(cc64_bypass);
#endif
}

// ---------------------------------------------------------------------------
// MIDI Learn
// ---------------------------------------------------------------------------

void MidiManager::start_learn(MidiTargetType type,
                              const std::string& effect_name,
                              const std::string& param_name) {
    learn_active_ = true;
    learn_target_type_ = type;
    learn_effect_name_ = effect_name;
    learn_param_name_ = param_name;
}

void MidiManager::cancel_learn() {
    learn_active_ = false;
    learn_effect_name_.clear();
    learn_param_name_.clear();
}

std::string MidiManager::learn_status() const {
    if (!learn_active_) return "";

    std::string target;
    switch (learn_target_type_) {
        case MidiTargetType::EffectParam:
            target = learn_effect_name_ + " > " + learn_param_name_;
            break;
        case MidiTargetType::EffectBypass:
            target = learn_effect_name_ + " (bypass)";
            break;
        case MidiTargetType::InputGain:
            target = "Input Gain";
            break;
        case MidiTargetType::OutputGain:
            target = "Output Gain";
            break;
    }
    return "MIDI Learn: move a CC for \"" + target + "\"...";
}

// ---------------------------------------------------------------------------
// Poll — called from GUI thread each frame
// ---------------------------------------------------------------------------

void MidiManager::inject_event(const MidiEvent& event) {
    midi_queue_.try_push(event);
}

void MidiManager::poll(IAudioEngine& engine) {
    MidiEvent event{};
    while (midi_queue_.try_pop(event)) {
        uint8_t cc_number = event.data1;
        uint8_t cc_value  = event.data2;
        int channel = event.status & 0x0F;

        // MIDI Learn: capture the first CC and create a mapping
        if (learn_active_) {
            MidiMapping mapping;
            mapping.cc_number = cc_number;
            mapping.midi_channel = channel;
            mapping.target_type = learn_target_type_;
            mapping.mode = (learn_target_type_ == MidiTargetType::EffectBypass)
                             ? MidiMappingMode::Toggle
                             : MidiMappingMode::Continuous;
            mapping.effect_name = learn_effect_name_;
            mapping.param_name  = learn_param_name_;
            add_mapping(mapping);
            learn_active_ = false;
            continue;
        }

        // Normal mode: resolve mapping and apply
        for (const auto& m : mappings_) {
            if (m.cc_number != cc_number) continue;
            if (m.midi_channel >= 0 && m.midi_channel != channel) continue;
            apply_mapping(m, cc_value, engine);
        }
    }
}

void MidiManager::apply_mapping(const MidiMapping& mapping, int cc_value,
                                IAudioEngine& engine) {
    float normalized = static_cast<float>(cc_value) / 127.0f;

    switch (mapping.target_type) {
        case MidiTargetType::InputGain: {
            float gain = normalized * 2.0f;
            engine.set_input_gain(gain);
            break;
        }
        case MidiTargetType::OutputGain: {
            float gain = normalized * 2.0f;
            engine.set_output_gain(gain);
            break;
        }
        case MidiTargetType::EffectBypass: {
            // Find the effect by name
            auto& effects = engine.effects();
            for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
                if (effects[i]->name() == mapping.effect_name) {
                    bool is_pressed = (cc_value >= 64);

                    // Toggle on either edge: press (false→true) or release (true→false)
                    if (is_pressed != mapping.last_state) {
                        effects[i]->set_enabled(!effects[i]->is_enabled());
                        int node_id = find_node_id_for_effect(engine, effects[i], i);
                        engine.push_effect_enabled(node_id, effects[i]->is_enabled() ? 1.0f : 0.0f);
                    }

                    // Update state for next event
                    mapping.last_state = is_pressed;
                    break;
                }
            }
            break;
        }
        case MidiTargetType::EffectParam: {
            // Check if it's a Mixer Gain mapping
            if (mapping.effect_name.find("Mixer_") == 0) {
                int node_id = -1;
                try { node_id = std::stoi(mapping.effect_name.substr(6)); } catch(...) {}
                if (node_id != -1) {
                    int pin_idx = -1;
                    if (mapping.param_name.find("Gain ") == 0) {
                        try { pin_idx = std::stoi(mapping.param_name.substr(5)); } catch(...) {}
                    }
                    if (pin_idx != -1) {
                        float gain = normalized * 2.0f;
                        engine.graph().set_mixer_input_gain(node_id, pin_idx, gain);
                        engine.push_mixer_gain_change(node_id, pin_idx, gain);
                        break;
                    }
                }
            }

            // Find the effect by name, then the param by name
            auto& effects = engine.effects();
            for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
                if (effects[i]->name() != mapping.effect_name) continue;

                auto& params = effects[i]->params();
                for (int p = 0; p < static_cast<int>(params.size()); ++p) {
                    if (params[p].name != mapping.param_name) continue;

                    float value = params[p].min_val +
                                  normalized * (params[p].max_val - params[p].min_val);
                    params[p].value = value;  // GUI sync
                    int node_id = find_node_id_for_effect(engine, effects[i], i);
                    engine.push_param_change(node_id, p, value);  // Audio sync
                    break;
                }
                break;  // Only map to the first matching effect
            }
            break;
        }
    }
}

} // namespace Amplitron
