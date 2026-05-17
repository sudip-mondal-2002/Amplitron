/**
 * @file preset_json.cpp
 * @brief Preset serialization / deserialization using nlohmann/json.
 *
 * This replaces the previous hand-rolled string-manipulation parser with a
 * proper, well-tested JSON library (nlohmann/json v3.11+).
 *
 * Design goals
 * ------------
 * 1. **Drop-in replacement** – the on-disk JSON format is unchanged; existing
 *    preset files load without modification.
 * 2. **Standard C++17 interface** – nlohmann ADL hooks (to_json / from_json)
 *    make PresetData and EffectData first-class nlohmann types, so callers
 *    can write `nlohmann::json j = preset;` directly.
 * 3. **Robust error handling** – every parse operation is wrapped in
 *    try/catch; failures propagate through PresetManager::last_error().
 * 4. **Preserves midi_mappings and metadata** – nothing that the old parser
 *    supported is dropped.
 */

#include "preset_json.h"
#include "midi/midi_manager.h"

#include <ctime>
#include <iostream>
#include <sstream>

namespace Amplitron {

// ============================================================
// ADL hook: EffectData  ←→  nlohmann::json
// ============================================================

void to_json(nlohmann::json& j, const PresetData::EffectData& fx) {
    // Build the flat params object: { "Drive": 2.0, "Tone": 0.6, ... }
    nlohmann::json params_obj = nlohmann::json::object();
    for (const auto& [name, value] : fx.params) {
        params_obj[name] = value;
    }

    j = {
        {"type",    fx.type},
        {"enabled", fx.enabled},
        {"mix",     fx.mix},
        {"params",  params_obj},
    };

    // Optional metadata sub-object (e.g. IR cabinet file path)
    if (!fx.metadata.empty()) {
        j["metadata"] = fx.metadata;
    }
}

void from_json(const nlohmann::json& j, PresetData::EffectData& fx) {
    fx.type    = j.value("type",    std::string{});
    fx.enabled = j.value("enabled", false);
    fx.mix     = j.value("mix",     1.0f);

    if (j.contains("params") && j["params"].is_object()) {
        for (const auto& [key, val] : j["params"].items()) {
            if (val.is_number()) {
                fx.params.push_back({key, val.get<float>()});
            }
        }
    }

    if (j.contains("metadata") && j["metadata"].is_object()) {
        for (const auto& [key, val] : j["metadata"].items()) {
            if (val.is_string()) {
                fx.metadata[key] = val.get<std::string>();
            }
        }
    }
}

// ============================================================
// ADL hook: MidiMapping  ←→  nlohmann::json
// ============================================================

static void to_json_midi(nlohmann::json& j, const MidiMapping& m) {
    j = {
        {"cc",      m.cc_number},
        {"channel", m.midi_channel},
        {"target",  static_cast<int>(m.target_type)},
        {"mode",    static_cast<int>(m.mode)},
        {"effect",  m.effect_name},
        {"param",   m.param_name},
    };
}

static void from_json_midi(const nlohmann::json& j, MidiMapping& m) {
    m.cc_number    = j.value("cc",      0);
    m.midi_channel = j.value("channel", -1);
    m.target_type  = static_cast<MidiTargetType>(j.value("target", 0));
    m.mode         = static_cast<MidiMappingMode>(j.value("mode",   0));
    m.effect_name  = j.value("effect",  std::string{});
    m.param_name   = j.value("param",   std::string{});
}

// ============================================================
// ADL hook: PresetData  ←→  nlohmann::json
// ============================================================

void to_json(nlohmann::json& j, const PresetData& preset) {
    // Generate an ISO-8601 timestamp
    std::time_t now = std::time(nullptr);
    char timebuf[64] = {};
    std::tm tm_info{};
#ifdef _WIN32
    localtime_s(&tm_info, &now);
#else
    localtime_r(&now, &tm_info);
#endif
    std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &tm_info);

    // Build effects array using the EffectData ADL hook
    nlohmann::json effects_arr = nlohmann::json::array();
    for (const auto& fx : preset.effects) {
        nlohmann::json jfx;
        to_json(jfx, fx);          // explicit call since we're not in ADL context
        effects_arr.push_back(std::move(jfx));
    }

    // Build midi_mappings array
    nlohmann::json midi_arr = nlohmann::json::array();
    for (const auto& m : preset.midi_mappings) {
        nlohmann::json jm;
        to_json_midi(jm, m);
        midi_arr.push_back(std::move(jm));
    }

    j = {
        {"format_version", 1},
        {"name",           preset.name},
        {"description",    preset.description},
        {"saved_at",       timebuf},
        {"input_gain",     preset.input_gain},
        {"output_gain",    preset.output_gain},
        {"effects",        std::move(effects_arr)},
        {"midi_mappings",  std::move(midi_arr)},
    };
}

void from_json(const nlohmann::json& j, PresetData& preset) {
    preset.name         = j.value("name",         std::string{});
    preset.description  = j.value("description",  std::string{});
    preset.input_gain   = j.value("input_gain",   0.7f);
    preset.output_gain  = j.value("output_gain",  0.8f);

    if (j.contains("effects") && j["effects"].is_array()) {
        for (const auto& jfx : j["effects"]) {
            PresetData::EffectData fx;
            from_json(jfx, fx);
            if (!fx.type.empty()) {
                preset.effects.push_back(std::move(fx));
            }
        }
    }

    if (j.contains("midi_mappings") && j["midi_mappings"].is_array()) {
        for (const auto& jm : j["midi_mappings"]) {
            MidiMapping m;
            from_json_midi(jm, m);
            preset.midi_mappings.push_back(m);
        }
    }
}

// ============================================================
// Public helpers used by PresetManager
// ============================================================

std::string to_json_ext(const PresetData& preset) {
    nlohmann::json j;
    to_json(j, preset);
    // dump(4) produces the same indented, human-readable format as before
    return j.dump(4) + "\n";
}

bool from_json_ext(const std::string& json_str, PresetData& preset) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        from_json(j, preset);
        return true;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[preset_json] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

} // namespace Amplitron
