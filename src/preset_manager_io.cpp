#include "preset_manager.h"
#include "preset_json.h"
#include "audio/effect_factory.h"
#include "audio/effects/cabinet_sim.h"
#include "preset_manager_impl.h"
#include "gui/gui_graph_state.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <unordered_map>

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

bool PresetManager::is_legacy_preset(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        file >> j;
        if (!j.contains("routing")) {
            return true;
        }
        std::string routing = j["routing"].get<std::string>();
        return routing == "linear";
    } catch (...) {
        return false;
    }
}

PresetData PresetManager::convert_linear_to_graph(const PresetData& legacy) {
    PresetData graph_preset;
    graph_preset.name = legacy.name;
    graph_preset.description = legacy.description;
    graph_preset.input_gain = legacy.input_gain;
    graph_preset.output_gain = legacy.output_gain;
    graph_preset.routing = "graph";
    graph_preset.midi_mappings = legacy.midi_mappings;

    // Create Input node
    PresetData::EffectData input_node;
    input_node.type = "Input";
    input_node.enabled = true;
    input_node.mix = 1.0f;
    input_node.node_id = 1;
    input_node.position_x = 40.0f;
    input_node.position_y = 150.0f;
    input_node.routing_type = 0; // StandardEffect
    input_node.is_graph_input = true;
    input_node.is_graph_output = legacy.effects.empty();
    graph_preset.effects.push_back(input_node);

    // Create effect nodes
    for (size_t i = 0; i < legacy.effects.size(); ++i) {
        const auto& legacy_fx = legacy.effects[i];
        PresetData::EffectData fx = legacy_fx;
        fx.node_id = static_cast<int>(i + 2);
        fx.position_x = 40.0f + static_cast<float>(i + 1) * 220.0f;
        fx.position_y = 150.0f;
        fx.routing_type = 0;
        fx.is_graph_input = false;
        fx.is_graph_output = (i == legacy.effects.size() - 1);
        graph_preset.effects.push_back(fx);
    }

    // Create links
    for (size_t i = 0; i < legacy.effects.size(); ++i) {
        PresetData::LinkData link;
        link.source_node_id = static_cast<int>(i + 1);
        link.source_pin_index = 0;
        link.dest_node_id = static_cast<int>(i + 2);
        link.dest_pin_index = 0;
        graph_preset.links.push_back(link);
    }

    return graph_preset;
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
    preset.routing = "graph"; // Saved presets are always graph presets now

    auto& ui_state = GuiGraphState::get_instance();

    for (const auto& node : engine.graph().get_nodes()) {
        PresetData::EffectData fd;
        fd.node_id = node.id;
        fd.routing_type = static_cast<int>(node.routing_type);
        fd.is_graph_input = node.is_graph_input;
        fd.is_graph_output = node.is_graph_output;

        // Lookup position from ui_state if present
        auto pos_it = ui_state.node_positions.find(node.id);
        if (pos_it != ui_state.node_positions.end()) {
            fd.position_x = pos_it->second.position.x;
            fd.position_y = pos_it->second.position.y;
        } else {
            fd.position_x = 0.0f;
            fd.position_y = 0.0f;
        }

        if (node.pedal) {
            fd.type = node.pedal->name();
            fd.enabled = node.pedal->is_enabled();
            fd.mix = node.pedal->get_mix();
            for (auto& p : node.pedal->params()) {
                fd.params.push_back({p.name, p.value});
            }

            if (std::strcmp(node.pedal->name(), "Cabinet") == 0) {
                auto* cab = dynamic_cast<CabinetSim*>(node.pedal.get());
                if (cab && cab->has_ir()) {
                    fd.metadata["ir_path"] = cab->ir_path();
                }
            }
        } else {
            fd.type = node.name;
            fd.enabled = true;
            fd.mix = 1.0f;
        }
        preset.effects.push_back(fd);
    }

    for (const auto& link : engine.graph().get_links()) {
        PresetData::LinkData ld;
        ld.source_node_id = engine.graph().get_node_from_pin(link.source_pin_id);
        ld.dest_node_id = engine.graph().get_node_from_pin(link.dest_pin_id);

        const DSPNode* src_node = engine.graph().find_node(ld.source_node_id);
        const DSPNode* dest_node = engine.graph().find_node(ld.dest_node_id);

        ld.source_pin_index = 0;
        if (src_node) {
            auto it = std::find(src_node->output_pin_ids.begin(), src_node->output_pin_ids.end(), link.source_pin_id);
            if (it != src_node->output_pin_ids.end()) {
                ld.source_pin_index = static_cast<int>(std::distance(src_node->output_pin_ids.begin(), it));
            }
        }

        ld.dest_pin_index = 0;
        if (dest_node) {
            auto it = std::find(dest_node->input_pin_ids.begin(), dest_node->input_pin_ids.end(), link.dest_pin_id);
            if (it != dest_node->input_pin_ids.end()) {
                ld.dest_pin_index = static_cast<int>(std::distance(dest_node->input_pin_ids.begin(), it));
            }
        }
        preset.links.push_back(ld);
    }

    preset.midi_mappings = midi_mappings;

    return save_preset_data(filepath, preset);
}

// Helper for configuring effect settings
static std::shared_ptr<Effect> create_and_configure_effect(PresetData::EffectData& fd, float sample_rate) {
    if (fd.type == "IR Cabinet") {
        fd.type = "Cabinet";
    }

    auto fx = EffectFactory::instance().create(fd.type);
    if (!fx) {
        std::cerr << "Unknown effect type: " << fd.type << std::endl;
        return nullptr;
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
    return fx;
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

    engine.set_input_gain(preset.input_gain);
    engine.set_output_gain(preset.output_gain);

    if (preset.routing == "linear") {
        // Compatibility mode / Legacy loading: reset graph and node positions
        engine.graph() = AudioGraph();
        GuiGraphState::get_instance().node_positions.clear();
        engine.effects().clear();

        for (auto& fd : preset.effects) {
            auto fx = create_and_configure_effect(fd, static_cast<float>(engine.get_sample_rate()));
            if (fx) {
                engine.effects().push_back(fx);
            }
        }

        // Trigger linear setup and auto-wiring
        engine.restore_effects_state(engine.effects());
    } else {
        // Graph Preset loading
        engine.graph() = AudioGraph();
        GuiGraphState::get_instance().node_positions.clear();
        engine.effects().clear();

        std::unordered_map<int, int> node_map; // maps old_node_id -> new_node_id

        for (auto& fd : preset.effects) {
            int new_node_id = -1;
            if (fd.routing_type == static_cast<int>(NodeRoutingType::StandardEffect)) {
                if (fd.type == "Input") {
                    new_node_id = engine.graph().add_node("Input", NodeRoutingType::StandardEffect, nullptr);
                } else {
                    auto fx = create_and_configure_effect(fd, static_cast<float>(engine.get_sample_rate()));
                    if (fx) {
                        new_node_id = engine.graph().add_node(fd.type, NodeRoutingType::StandardEffect, fx);
                        engine.effects().push_back(fx);
                    }
                }
            } else if (fd.routing_type == static_cast<int>(NodeRoutingType::Splitter)) {
                new_node_id = engine.graph().add_node("Splitter", NodeRoutingType::Splitter, nullptr);
            } else if (fd.routing_type == static_cast<int>(NodeRoutingType::Mixer)) {
                new_node_id = engine.graph().add_node("Mixer", NodeRoutingType::Mixer, nullptr);
            }

            if (new_node_id != -1) {
                node_map[fd.node_id] = new_node_id;

                if (fd.is_graph_input) {
                    engine.graph().set_node_as_input(new_node_id, true);
                }
                if (fd.is_graph_output) {
                    engine.graph().set_node_as_output(new_node_id, true);
                }

                // Restore node UI layout position
                GuiGraphState::get_instance().node_positions[new_node_id] = { ImVec2(fd.position_x, fd.position_y), false };
            }
        }

        // Wire links
        for (const auto& link : preset.links) {
            auto src_it = node_map.find(link.source_node_id);
            auto dest_it = node_map.find(link.dest_node_id);
            if (src_it != node_map.end() && dest_it != node_map.end()) {
                int new_src_node_id = src_it->second;
                int new_dest_node_id = dest_it->second;

                const DSPNode* src_node = engine.graph().find_node(new_src_node_id);
                const DSPNode* dest_node = engine.graph().find_node(new_dest_node_id);

                int src_pin_id = -1;
                if (src_node && link.source_pin_index >= 0 && link.source_pin_index < static_cast<int>(src_node->output_pin_ids.size())) {
                    src_pin_id = src_node->output_pin_ids[link.source_pin_index];
                }

                int dest_pin_id = -1;
                if (dest_node && link.dest_pin_index >= 0 && link.dest_pin_index < static_cast<int>(dest_node->input_pin_ids.size())) {
                    dest_pin_id = dest_node->input_pin_ids[link.dest_pin_index];
                }

                if (src_pin_id != -1 && dest_pin_id != -1) {
                    engine.graph().add_link(src_pin_id, dest_pin_id);
                }
            }
        }

        engine.commit_graph_changes();
    }

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
