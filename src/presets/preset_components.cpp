#include "presets/preset_components.h"
#include "preset_json.h"
#include "preset_manager.h"
#include <fstream>
#include <filesystem>

namespace Amplitron {

std::string PresetSerializer::serialize(const PresetData& preset) {
    return to_json_ext(preset);
}

bool PresetSerializer::deserialize(const std::string& json_str, PresetData& preset) {
    return from_json_ext(json_str, preset);
}

bool PresetStorage::save(const std::string& filepath, const std::string& data) {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << data;
    file.close();
    return true;
}

std::string PresetStorage::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";
    std::string data((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
    file.close();
    return data;
}

std::vector<std::string> PresetStorage::list() {
    std::vector<std::string> result;
    
    auto append_files = [](const std::string& dir, std::vector<std::string>& res) {
        try {
            if (!dir.empty() && std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".json") {
                        res.push_back(entry.path().string());
                    }
                }
            }
        } catch (...) {
            // Ignore errors
        }
    };

    std::string user_dir = PresetManager::get_presets_dir();
    append_files(user_dir, result);

    std::string sys_dir = PresetManager::get_system_presets_dir();
    if (!sys_dir.empty() && sys_dir != user_dir) {
        append_files(sys_dir, result);
    }

    return result;
}

bool PresetStorage::remove(const std::string& filepath) {
    try {
        if (std::filesystem::exists(filepath)) {
            return std::filesystem::remove(filepath);
        }
    } catch (...) {
        // Ignore errors
    }
    return false;
}

std::string PresetMigrator::migrate(const std::string& raw_json) {
    return PresetManager::apply_migrations(raw_json);
}

} // namespace Amplitron
