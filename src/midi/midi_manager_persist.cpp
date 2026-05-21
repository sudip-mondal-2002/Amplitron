#include "midi/midi_manager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace Amplitron {

// ---------------------------------------------------------------------------
// Persistence — midi_config.json
// ---------------------------------------------------------------------------

std::string MidiManager::get_config_path() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "midi_config.json";
    std::string dir = std::string(appdata) + "\\Amplitron";
    try {
        std::filesystem::create_directories(dir);
    } catch (...) {
        // Ignore errors — will fall back to local path if creation fails
    }
    return dir + "\\midi_config.json";
#else
    const char* home = std::getenv("HOME");
    if (!home) return "midi_config.json";
    std::string config_dir = std::string(home) + "/.config/amplitron";
    try {
        std::filesystem::create_directories(config_dir);
    } catch (...) {
        // Ignore errors — will fall back to local path if creation fails
    }
    return config_dir + "/midi_config.json";
#endif
}

std::string MidiManager::mappings_to_json() const {
    nlohmann::ordered_json root = nlohmann::ordered_json::object();
    nlohmann::ordered_json arr  = nlohmann::ordered_json::array();

    for (const auto& m : mappings_) {
        nlohmann::ordered_json jm = nlohmann::ordered_json::object();
        jm["cc"]      = m.cc_number;
        jm["channel"] = m.midi_channel;
        jm["target"]  = static_cast<int>(m.target_type);
        jm["mode"]    = static_cast<int>(m.mode);
        jm["effect"]  = m.effect_name;
        jm["param"]   = m.param_name;
        arr.push_back(std::move(jm));
    }

    root["mappings"] = std::move(arr);
    return root.dump(2) + "\n";
}

bool MidiManager::mappings_from_json(const std::string& json_str) {
    mappings_.clear();

    try {
        auto j = nlohmann::json::parse(json_str);

        if (!j.contains("mappings") || !j["mappings"].is_array()) {
            return false;
        }

        for (const auto& jm : j["mappings"]) {
            MidiMapping m;
            m.cc_number    = jm.value("cc",      0);
            m.midi_channel = jm.value("channel", -1);
            m.target_type  = static_cast<MidiTargetType>(jm.value("target", 0));
            m.mode         = static_cast<MidiMappingMode>(jm.value("mode",   0));
            m.effect_name  = jm.value("effect",  std::string{});
            m.param_name   = jm.value("param",   std::string{});
            mappings_.push_back(m);
        }

        return true;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[midi_config] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

void MidiManager::save_config() const {
    std::string path = get_config_path();
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << mappings_to_json();
}

void MidiManager::load_config() {
    std::string path = get_config_path();
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    mappings_from_json(content);
}

} // namespace Amplitron

