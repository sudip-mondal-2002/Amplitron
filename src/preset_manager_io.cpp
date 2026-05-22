#include "preset_manager.h"
#include "preset_json.h"
#include "audio/effect_factory.h"
#include "audio/effects/cabinet_sim.h"
#include "preset_manager_impl.h"
#include <iostream>
#include <cstring>

namespace Amplitron {

std::vector<std::string> PresetManager::list_presets() {
    std::vector<std::string> result;

    append_json_files(get_presets_dir(), result);

    std::string sys_dir = get_system_presets_dir();
    std::string user_dir = get_presets_dir();
    if (!sys_dir.empty() && dir_exists(sys_dir) && sys_dir != user_dir) {
        append_json_files(sys_dir, result);
    }

    return result;
}

bool PresetManager::save_preset_data(const std::string& filepath,
                                     const PresetData& preset) {
    std::string json = to_json_ext(preset);

    std::ofstream file(filepath);
    if (!file.is_open()) {
        last_error_ = "Could not open file for writing: " + filepath;
        std::cerr << last_error_ << std::endl;
        return false;
    }

    file << json;
    file.close();

    std::cout << "Preset saved: " << filepath << std::endl;
    return true;
}

bool PresetManager::save_preset(const std::string& filepath,
                                const std::string& preset_name,
                                const std::string& description,
                                AudioEngine& engine,
                                const std::vector<MidiMapping>& midi_mappings) {
    PresetData preset;
    preset.name = preset_name;
    preset.description = description;
    preset.input_gain = engine.get_input_gain();
    preset.output_gain = engine.get_output_gain();

    for (auto& fx : engine.effects()) {
        PresetData::EffectData fd;
        fd.type = fx->type_id();
        fd.enabled = fx->is_enabled();
        fd.mix = fx->get_mix();
        for (auto& p : fx->params()) {
            fd.params.push_back({p.name, p.value});
        }

        if (std::strcmp(fx->name(), "Cabinet") == 0) {
            auto* cab = dynamic_cast<CabinetSim*>(fx.get());
            if (cab && cab->has_ir()) {
                fd.metadata["ir_path"] = cab->ir_path();
            }
        }

        preset.effects.push_back(fd);
    }

    preset.midi_mappings = midi_mappings;

    return save_preset_data(filepath, preset);
}

bool PresetManager::load_preset(const std::string& filepath,
                                AudioEngine& engine,
                                MidiManager* midi_manager) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        last_error_ = "Could not open file: " + filepath;
        std::cerr << last_error_ << std::endl;
        return false;
    }

    std::string json((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    file.close();

    PresetData preset;
    if (!from_json_ext(json, preset)) {
        last_error_ = "Failed to parse preset file: " + filepath;
        std::cerr << last_error_ << std::endl;
        return false;
    }

    engine.clear_effects();

    engine.set_input_gain(preset.input_gain);
    engine.set_output_gain(preset.output_gain);

    std::vector<std::shared_ptr<Effect>> loaded_effects;
    for (auto& fd : preset.effects) {
        auto fx = EffectFactory::instance().create(fd.type);
        if (!fx) {
            std::cerr << "Unknown effect type: " << fd.type << std::endl;
            continue;
        }

        fx->set_enabled(fd.enabled);
        fx->set_mix(fd.mix);

        auto& fxparams = fx->params();
        for (auto& saved_param : fd.params) {
            for (auto& ep : fxparams) {
                if (ep.name == saved_param.first) {
                    ep.value = clamp(saved_param.second, ep.min_val, ep.max_val);
                    break;
                }
            }
        }

        // Migrate old "IR Cabinet" type to "Cabinet" (IRCabinet was removed)
        if (fd.type == "IR Cabinet") {
            fd.type = "Cabinet";
            fx = EffectFactory::instance().create(fd.type);
            if (!fx) {
                std::cerr << "Failed to create Cabinet effect for migrated IR Cabinet preset" << std::endl;
                continue;
            }
            fx->set_enabled(fd.enabled);
            fx->set_mix(fd.mix);
            for (auto& saved_param : fd.params) {
                for (auto& ep : fx->params()) {
                    if (ep.name == saved_param.first) {
                        ep.value = clamp(saved_param.second, ep.min_val, ep.max_val);
                        break;
                    }
                }
            }
        }

        auto it = fd.metadata.find("ir_path");
        if (it != fd.metadata.end() && !it->second.empty()) {
            auto* cab = dynamic_cast<CabinetSim*>(fx.get());
            if (cab) {
                if (!cab->load_ir(it->second)) {
                    std::cerr << "Cabinet: could not load IR file: "
                              << it->second << std::endl;
                }
            }
        }

        loaded_effects.push_back(fx);
    }

    engine.add_initial_effects(loaded_effects);

    if (midi_manager) {
        midi_manager->clear_mappings();
        for (const auto& mapping : preset.midi_mappings) {
            midi_manager->add_mapping(mapping);
        }
    }

    std::cout << "Preset loaded: " << preset.name << " (" << filepath << ")" << std::endl;
    return true;
}

} // namespace Amplitron
