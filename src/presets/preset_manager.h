#pragma once

#include <fstream>
#include <map>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <sstream>

#include "audio/engine/i_audio_engine.h"
#include "common.h"
#include "midi/i_midi_manager.h"
#include "presets/i_preset_manager.h"

namespace Amplitron {

class IPresetSerializer;
class IPresetStorage;
class IPresetMigrator;

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

// Serialization helpers
std::string to_json_ext(const PresetData& preset);
bool from_json_ext(const std::string& json_str, PresetData& preset);

// nlohmann ADL hooks for PresetData and nested EffectData.
void to_json(nlohmann::json& j, const PresetData::EffectData& fx);
void from_json(const nlohmann::json& j, PresetData::EffectData& fx);
void to_json(nlohmann::json& j, const PresetData& preset);
void from_json(const nlohmann::json& j, PresetData& preset);

// Directory helper exposed for tests and diagnostics.
std::string get_user_presets_dir();

class PresetManager : public IPresetManager {
   public:
    friend class PresetSerializer;
    friend class PresetStorage;
    friend class PresetMigrator;

    PresetManager();
    PresetManager(std::unique_ptr<IPresetSerializer> serializer,
                  std::unique_ptr<IPresetStorage> storage,
                  std::unique_ptr<IPresetMigrator> migrator);
    ~PresetManager() override;

    // Save provided preset data to JSON file
    bool save_preset_data(const std::string& filepath, const PresetData& preset) override;

    // Save current engine state to JSON file
    bool save_preset(const std::string& filepath, const std::string& preset_name,
                     const std::string& description, IAudioEngine& engine,
                     const std::vector<MidiMapping>& midi_mappings = {}) override;

    // Load preset from JSON file and apply to engine
    bool load_preset(const std::string& filepath, IAudioEngine& engine,
                     IMidiManager* midi_manager = nullptr) override;

    // Serialize full preset to JSON string
    static std::string preset_to_json_string(IAudioEngine& engine);

    // Serialize current graph to JSON
    static std::string graph_to_json(const AudioGraph& graph);

    // Load graph from JSON
    static bool graph_from_json(const std::string& json, AudioGraph& graph);

    std::vector<std::string> list_presets() override;
    bool delete_preset(const std::string& filepath) override;
    std::string get_last_error() const override;
    std::string get_presets_directory() const override;

    // Directory helpers / global config
    static std::string get_presets_dir();
    static void set_presets_dir(const std::string& dir);
    static const std::string& custom_presets_dir() { return custom_presets_dir_; }

    static void save_config();
    static void load_config();

    // Public migration hooks so test targets can validate behavior
    static std::string apply_migrations(const std::string& raw_json_string);

   private:
    std::unique_ptr<IPresetSerializer> serializer_;
    std::unique_ptr<IPresetStorage> storage_;
    std::unique_ptr<IPresetMigrator> migrator_;

    std::string last_error_;
    static std::string custom_presets_dir_;

    static void save_factory_presets(const std::string& dir);
    static std::string get_config_path();
    static std::string get_system_presets_dir();
};

// Compatibility alias to avoid breaking code referencing PresetManagerService
using PresetManagerService = PresetManager;

}  // namespace Amplitron
