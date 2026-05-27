#include "preset_manager.h"
#include "preset_manager_impl.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#define MKDIR(path) _mkdir(path)
#elif defined(__APPLE__)
#include <dirent.h>
#include <mach-o/dyld.h>
#define MKDIR(path) mkdir(path, 0755)
#else
#include <dirent.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#ifdef _EMSCRIPTEN_
#include <emscripten.h>

namespace Amplitron {
    std::string PresetManager::get_user_presets_dir() {
        char* result = (char*)EM_ASM_PTR({
            return stringToNewUTF8(
                window._amplitronPresetDir || 'preset'
            );
        });
        std::string dir(result);
        free(result);
        return dir;
    }
}
#endif

namespace Amplitron {

void append_json_files(const std::string& dir,
                       std::vector<std::string>& result) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            const auto& path = entry.path();
            if (path.extension() == ".json") {
                result.push_back(path.string());
            }
        }
    } catch (...) {
        // Ignore invalid or non-existent directories
    }
}

std::string get_bundled_presets_dir() {
#ifdef _WIN32
    auto has_json_presets = [](const std::string& dir) -> bool {
        std::vector<std::string> files;
        append_json_files(dir, files);
        return !files.empty();
    };

    char path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, path, sizeof(path))) {
        std::filesystem::path exe_path(path);
        std::filesystem::path exe_dir = exe_path.parent_path();

        // Installed/bundled layout: presets next to the executable.
        std::string bundled = (exe_dir / "presets").string();
        if (dir_exists(bundled) && has_json_presets(bundled)) return bundled;

        // Dev/CMake layout: executable under build dir, presets at repo root.
        std::string repo_root_presets = (exe_dir / ".." / "presets").string();
        if (dir_exists(repo_root_presets) && has_json_presets(repo_root_presets)) return repo_root_presets;
    }

    // Fallback: relative to current working directory (useful for local runs).
    if (dir_exists("presets") && has_json_presets("presets")) return "presets";
    return "presets";
#elif defined(__APPLE__)
    char exe_path[4096];
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        std::string exe_str = exe_path;
        size_t last_slash = exe_str.find_last_of("/");
        if (last_slash != std::string::npos) {
            std::string bundle_presets = exe_str.substr(0, last_slash) + "/../Resources/presets";
            if (dir_exists(bundle_presets)) {
                return bundle_presets;
            }
        }
    }
    return "presets";
#else
    if (dir_exists("presets")) {
        return "presets";
    }
    return "/usr/share/amplitron/presets";
#endif
}
void PresetManager::set_presets_dir(const std::string& dir) {
    if (dir.empty()) {
        custom_presets_dir_ = "";
        return;
    }
    std::string normalized = dir;
#ifdef _WIN32
    // Allow callers/tests to pass forward slashes; normalize for _findfirst/_mkdir.
    for (char& c : normalized) {
        if (c == '/') c = '\\';
    }
#endif
    try {
        std::filesystem::create_directories(normalized);
    } catch (...) {
        return;
    }
    if (dir_exists(normalized)) {
        custom_presets_dir_ = normalized;
        std::vector<std::string> dir_presets;
        append_json_files(normalized, dir_presets);
        if (dir_presets.empty()) {
            save_factory_presets(normalized);
        }
    }
}

void PresetManager::save_config() {
    std::string path = get_config_path();
    std::ofstream f(path);
    if (!f.is_open()) return;
    f << "{\n";
    f << "  \"presets_dir\": \"";
    for (char c : custom_presets_dir_) {
        if (c == '\\') f << "\\\\";
        else if (c == '"') f << "\\\"";
        else f << c;
    }
    f << "\"\n}\n";
}

void PresetManager::load_config() {
    std::string path = get_config_path();
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    const std::string key = "\"presets_dir\"";
    size_t key_pos = content.find(key);
    if (key_pos == std::string::npos) return;

    size_t colon = content.find(':', key_pos + key.size());
    if (colon == std::string::npos) return;

    size_t quote_open = content.find('"', colon + 1);
    if (quote_open == std::string::npos) return;

    std::string value;
    size_t i = quote_open + 1;
    while (i < content.size() && content[i] != '"') {
        if (content[i] == '\\' && i + 1 < content.size()) {
            ++i;
            if (content[i] == '\\') value += '\\';
            else if (content[i] == '"') value += '"';
            else if (content[i] == 'n') value += '\n';
            else { value += '\\'; value += content[i]; }
        } else {
            value += content[i];
        }
        ++i;
    }

    if (!value.empty() && dir_exists(value)) {
        custom_presets_dir_ = value;
    }
}

std::string PresetManager::get_presets_dir() {
    if (!custom_presets_dir_.empty()) {
        MKDIR(custom_presets_dir_.c_str());
        if (dir_exists(custom_presets_dir_)) {
            return custom_presets_dir_;
        }
    }

    std::string user_dir = get_user_presets_dir();
    if (!user_dir.empty()) {
        try {
            std::filesystem::create_directories(user_dir);
        } catch (...) {
            // Fall through to local fallback if creation fails
        }
        if (dir_exists(user_dir)) {
            return user_dir;
        }
    }

    std::string dir = "presets";
    MKDIR(dir.c_str());
    return dir;
}

void PresetManager::save_factory_presets(const std::string& dir) {
    std::string src_dir = get_bundled_presets_dir();
    if (!dir_exists(src_dir)) {
        std::cerr << "Bundled presets directory not found: " << src_dir << std::endl;
        return;
    }

    std::vector<std::string> preset_files;
    append_json_files(src_dir, preset_files);

    for (const auto& src_path : preset_files) {
        size_t last_slash = src_path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ?
                                src_path.substr(last_slash + 1) : src_path;

#ifdef _WIN32
        std::string dest_path = dir + "\\" + filename;
#else
        std::string dest_path = dir + "/" + filename;
#endif

        std::ifstream src_file(src_path);
        if (!src_file.is_open()) {
            std::cerr << "Could not open source preset: " << src_path << std::endl;
            continue;
        }

        std::string content((std::istreambuf_iterator<char>(src_file)),
                           std::istreambuf_iterator<char>());
        src_file.close();

        std::ofstream dest_file(dest_path);
        if (!dest_file.is_open()) {
            std::cerr << "Could not write preset: " << dest_path << std::endl;
            continue;
        }

        dest_file << content;
        dest_file.close();
    }
}

} // namespace Amplitron
