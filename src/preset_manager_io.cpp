#include "preset_manager.h"
#include "preset_json.h"
#include "common.h"
#include "audio/audio_graph.h"
#include "audio/effect_factory.h"
#include "audio/effects/cabinet_sim.h"
#include "preset_manager_impl.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <map>
#include <nlohmann/json.hpp>
#include <utility>

namespace Amplitron {

namespace {

bool is_valid_routing_type(int value) {
    return value == static_cast<int>(NodeRoutingType::StandardEffect) ||
           value == static_cast<int>(NodeRoutingType::Splitter) ||
           value == static_cast<int>(NodeRoutingType::Mixer);
}

bool load_graph_preset(const nlohmann::json& j,
                       const std::string& filepath,
                       AudioEngine& engine,
                       MidiManager* midi_manager,
                       std::string& error) {
    if (!j.contains("nodes") || !j["nodes"].is_array() ||
        !j.contains("links") || !j["links"].is_array()) {
        error = "Graph preset is missing nodes or links: " + filepath;
        std::cerr << error << std::endl;
        return false;
    }

    try {
        AudioGraph graph;
        std::vector<std::shared_ptr<Effect>> loaded_effects;
        std::map<int, int> pin_map;

        for (const auto& node_j : j["nodes"]) {
            if (!node_j.contains("id") || !node_j.contains("routing_type")) {
                error = "Graph preset node is missing id or routing_type: " + filepath;
                std::cerr << error << std::endl;
                return false;
            }

            std::shared_ptr<Effect> effect = nullptr;
            if (node_j.contains("effect") && node_j["effect"].is_object()) {
                nlohmann::json effect_j = node_j["effect"];
                std::string type = effect_j.value("type", std::string{});
                if (type == "IR Cabinet") {
                    type = "Cabinet";
                }

                if (!type.empty()) {
                    effect = EffectFactory::instance().create(type);
                    if (!effect) {
                        error = "Unknown graph preset effect type: " + type;
                        std::cerr << error << std::endl;
                        return false;
                    }

                    effect->set_params(effect_j);

                    if (effect_j.contains("metadata") && effect_j["metadata"].is_object()) {
                        std::string ir_path = effect_j["metadata"].value("ir_path", std::string{});
                        if (!ir_path.empty()) {
                            auto* cab = dynamic_cast<CabinetSim*>(effect.get());
                            if (cab && !cab->load_ir(ir_path)) {
                                std::cerr << "Cabinet: could not load IR file: "
                                          << ir_path << std::endl;
                            }
                        }
                    }

                    loaded_effects.push_back(effect);
                }
            }

            int routing_type_raw = node_j.value("routing_type", -1);
            if (!is_valid_routing_type(routing_type_raw)) {
                error = "Graph preset node has invalid routing_type: " + filepath;
                std::cerr << error << std::endl;
                return false;
            }
            NodeRoutingType routing_type = static_cast<NodeRoutingType>(routing_type_raw);
            std::string default_name = effect ? std::string(effect->name()) : std::string("Node");
            std::string node_name = node_j.value("name", default_name);
            int new_id = graph.add_node(node_name, routing_type, effect);

            graph.set_node_as_input(new_id, node_j.value("is_graph_input", false));
            graph.set_node_as_output(new_id, node_j.value("is_graph_output", false));
            graph.set_node_position(new_id,
                                    node_j.value("x", 0.0f),
                                    node_j.value("y", 0.0f));

            if (node_j.contains("input_gains") && node_j["input_gains"].is_array()) {
                for (size_t i = 0; i < node_j["input_gains"].size(); ++i) {
                    if (node_j["input_gains"][i].is_number()) {
                        graph.set_node_input_gain(new_id, i,
                                                  node_j["input_gains"][i].get<float>());
                    }
                }
            }

            const auto* new_node = graph.find_node(new_id);
            if (!new_node) {
                error = "Could not create graph preset node: " + node_name;
                std::cerr << error << std::endl;
                return false;
            }

            if (node_j.contains("input_pin_ids") && node_j["input_pin_ids"].is_array()) {
                const auto& old_pins = node_j["input_pin_ids"];
                for (size_t i = 0; i < old_pins.size() && i < new_node->input_pin_ids.size(); ++i) {
                    int old_pin = old_pins[i].get<int>();
                    auto [_, inserted] = pin_map.emplace(old_pin, new_node->input_pin_ids[i]);
                    if (!inserted) {
                        error = "Graph preset contains duplicate pin id: " + filepath;
                        std::cerr << error << std::endl;
                        return false;
                    }
                }
            }
            if (node_j.contains("output_pin_ids") && node_j["output_pin_ids"].is_array()) {
                const auto& old_pins = node_j["output_pin_ids"];
                for (size_t i = 0; i < old_pins.size() && i < new_node->output_pin_ids.size(); ++i) {
                    int old_pin = old_pins[i].get<int>();
                    auto [_, inserted] = pin_map.emplace(old_pin, new_node->output_pin_ids[i]);
                    if (!inserted) {
                        error = "Graph preset contains duplicate pin id: " + filepath;
                        std::cerr << error << std::endl;
                        return false;
                    }
                }
            }
        }

        for (const auto& link_j : j["links"]) {
            int old_src = link_j.value("source_pin_id", -1);
            int old_dst = link_j.value("dest_pin_id", -1);
            if (pin_map.find(old_src) == pin_map.end() ||
                pin_map.find(old_dst) == pin_map.end()) {
                error = "Graph preset link references a missing pin: " + filepath;
                std::cerr << error << std::endl;
                return false;
            }

            if (graph.add_link(pin_map[old_src], pin_map[old_dst]) == -1) {
                error = "Graph preset contains an invalid link: " + filepath;
                std::cerr << error << std::endl;
                return false;
            }
        }

        if (!graph.rebuild_topology()) {
            error = "Graph preset contains an invalid topology: " + filepath;
            std::cerr << error << std::endl;
            return false;
        }

        if (j.contains("input_gain") && j["input_gain"].is_number()) {
            engine.set_input_gain(j["input_gain"].get<float>());
        }
        if (j.contains("output_gain") && j["output_gain"].is_number()) {
            engine.set_output_gain(j["output_gain"].get<float>());
        }
        engine.replace_graph(std::move(graph), std::move(loaded_effects));

        if (midi_manager) {
            midi_manager->clear_mappings();
        }

        std::cout << "Graph preset loaded: "
                  << j.value("name", std::string{"Unnamed Graph Preset"})
                  << " (" << filepath << ")" << std::endl;
        return true;
    } catch (const nlohmann::json::exception& e) {
        error = "Failed to parse graph preset: " + std::string(e.what());
        std::cerr << error << std::endl;
        return false;
    }
}

} // namespace

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

    nlohmann::json parsed = nlohmann::json::parse(json, nullptr, false);
    bool is_graph_preset = false;
    if (!parsed.is_discarded()) {
        is_graph_preset = parsed.contains("nodes") && parsed["nodes"].is_array() &&
                          parsed.contains("links") && parsed["links"].is_array();
        if (!is_graph_preset && parsed.contains("format_version") &&
            parsed["format_version"].is_number_integer()) {
            is_graph_preset = parsed["format_version"].get<int>() == 2;
        }
    }
    if (is_graph_preset) {
        std::string graph_error;
        bool loaded = load_graph_preset(parsed, filepath, engine, midi_manager,
                                        graph_error);
        if (!loaded) {
            last_error_ = graph_error;
        }
        return loaded;
    }

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
