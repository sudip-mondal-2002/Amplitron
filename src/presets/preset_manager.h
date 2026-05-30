#pragma once

#include "common.h"
#include "audio/engine/audio_engine.h"
#include "midi/midi_manager.h"
#include <nlohmann/json_fwd.hpp>
#include <fstream>
#include <map>
#include <sstream>

namespace Amplitron {

constexpr int CURRENT_PRESET_VERSION = 2;

struct PresetData {
    std::string name;
    std::string description;
    float input_gain = 0.7f;
    float output_gain = 0.8f;

    struct EffectData {
        std::string type;
        bool enabled = false;
        float mix = 1.0f;
        std::vector<std::pair<std::string, float>> params;
        std::map<std::string, std::string> metadata;
    };
    std::vector<EffectData> effects;
    std::vector<MidiMapping> midi_mappings;

    std::string routing = "linear";

    struct NodeData {
        std::string id;
        std::string type;
        float x = 0.0f;
        float y = 0.0f;
        bool enabled = true;
        float mix = 1.0f;
        int num_inputs = 0;
        std::vector<std::pair<std::string, float>> params;
        std::map<std::string, std::string> metadata;
    };
    std::vector<NodeData> nodes;

    struct LinkData {
        std::string src_pin;
        std::string dst_pin;
    };
    std::vector<LinkData> links;
};

// Serialization helpers are declared here so translation units that include
// preset_manager.h (including tests) can use preset JSON APIs without relying
// on direct inclusion order of preset_json.h.
std::string to_json_ext(const PresetData& preset);
bool from_json_ext(const std::string& json_str, PresetData& preset);

// nlohmann ADL hooks for PresetData and nested EffectData.
void to_json(nlohmann::json& j, const PresetData::EffectData& fx);
void from_json(const nlohmann::json& j, PresetData::EffectData& fx);
void to_json(nlohmann::json& j, const PresetData& preset);
void from_json(const nlohmann::json& j, PresetData& preset);

// Directory helper exposed for tests and diagnostics.
std::string get_user_presets_dir();

class PresetManager {
public:
    // Save provided preset data to JSON file
    static bool save_preset_data(const std::string& filepath,
                                 const PresetData& preset);

    // Save current engine state to JSON file
    static bool save_preset(const std::string& filepath,
                            const std::string& preset_name,
                            const std::string& description,
                            AudioEngine& engine,
                            const std::vector<MidiMapping>& midi_mappings = {});

    // Load preset from JSON file and apply to engine
    static bool load_preset(const std::string& filepath,
                            AudioEngine& engine,
                            MidiManager* midi_manager = nullptr);

    // Serialize current graph to JSON
    static std::string graph_to_json(const AudioGraph& graph);

    // Load graph from JSON
    static bool graph_from_json(const std::string& json, AudioGraph& graph);

    static std::string get_presets_dir();
    static void set_presets_dir(const std::string& dir);
    static const std::string& custom_presets_dir() { return custom_presets_dir_; }

    static void save_config();
    static void load_config();
    static std::vector<std::string> list_presets();
    static const std::string& last_error() { return last_error_; }

    // Public migration hooks so test targets can validate behavior
    static std::string apply_migrations(const std::string& raw_json_string);

private:
    static std::string last_error_;
    static std::string custom_presets_dir_;

    static void save_factory_presets(const std::string& dir);
    static std::string get_config_path();
    static std::string get_system_presets_dir();
};

} // namespace Amplitron