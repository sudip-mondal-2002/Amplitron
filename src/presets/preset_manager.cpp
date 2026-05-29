#include "preset_manager.h"
#include "preset_manager_impl.h"
#include <iostream>
#include <ctime>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define MKDIR(path) _mkdir(path)
#define STAT_STRUCT struct _stat
#define STAT_FN     _stat
#elif defined(__APPLE__)
#include <dirent.h>
#include <mach-o/dyld.h>
#define MKDIR(path) mkdir(path, 0755)
#define STAT_STRUCT struct stat
#define STAT_FN     stat
#else
#include <dirent.h>
#define MKDIR(path) mkdir(path, 0755)
#define STAT_STRUCT struct stat
#define STAT_FN     stat
#endif

namespace Amplitron {

std::string PresetManager::last_error_;
std::string PresetManager::custom_presets_dir_;

bool dir_exists(const std::string& path) {
    STAT_STRUCT st;
    return STAT_FN(path.c_str(), &st) == 0 &&
#ifdef _WIN32
           (st.st_mode & _S_IFDIR);
#else
           S_ISDIR(st.st_mode);
#endif
}

std::string PresetManager::get_system_presets_dir() {
#ifdef _WIN32
    const char* pd = std::getenv("ProgramData");
    return pd ? std::string(pd) + "\\Amplitron\\presets" : "";
#elif defined(__APPLE__)
    return "/Library/Application Support/Amplitron/presets";
#else
    return "/usr/share/amplitron/presets";
#endif
}

std::string PresetManager::get_config_path() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "amplitron_config.json";
    std::string dir = std::string(appdata) + "\\Amplitron";
    MKDIR(dir.c_str());
    return dir + "\\config.json";
#else
    const char* home = std::getenv("HOME");
    if (!home) return "amplitron_config.json";
    std::string config_dir = std::string(home) + "/.config/amplitron";
    try {
        std::filesystem::create_directories(config_dir);
    } catch (...) {}
    return config_dir + "/config.json";
#endif
}

std::string get_user_presets_dir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "";
    return std::string(appdata) + "\\Amplitron\\presets";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/Library/Application Support/Amplitron/presets";
#else
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.config/amplitron/presets";
#endif
}

// Clean, robust string migration implementation (No JSON header dependencies)
std::string PresetManager::apply_migrations(const std::string& raw_json_string) {
    // Find the absolute root opening of the JSON payload
    size_t root_start = raw_json_string.find('{');
    if (root_start == std::string::npos) {
        return raw_json_string;
    }

    // Look for a "version" key strictly near the root area (e.g., within the first 100 characters)
    // This stops nested effect parameters from accidentally triggering a false positive bypass.
    size_t version_pos = raw_json_string.find("\"version\"");
    bool is_root_version = (version_pos != std::string::npos && (version_pos - root_start) < 100);

    if (!is_root_version) {
        std::cout << "[Preset Migration] Upgrading legacy unversioned preset format to Version " 
                  << CURRENT_PRESET_VERSION << std::endl;

        std::string patched = raw_json_string;
        size_t last_bracket = patched.find_last_of('}');
        
        if (last_bracket != std::string::npos && last_bracket > root_start) {
            // Find out if there is any content between the root brackets to avoid trailing comma bugs
            size_t content_check = patched.find_first_not_of(" \t\n\r", root_start + 1);
            bool is_empty_json = (content_check == last_bracket);

            // Construct the exact version upgrade block using our header constant dynamically
            std::string migration_patch;
            if (!is_empty_json) {
                migration_patch += ",\n";
            }
            migration_patch += "  \"version\": " + std::to_string(CURRENT_PRESET_VERSION) + ",\n";
            migration_patch += "  \"input_gain\": 0.7,\n";
            migration_patch += "  \"output_gain\": 0.8\n";

            patched.insert(last_bracket, migration_patch);
            return patched;
        }
    }

    return raw_json_string;
}

} // namespace Amplitron