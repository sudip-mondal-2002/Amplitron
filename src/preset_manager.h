#pragma once

#include "common.h"
#include "audio/audio_engine.h"
#include "midi/midi_manager.h"
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
};

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