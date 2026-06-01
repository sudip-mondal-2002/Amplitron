#ifndef AMPLITRON_PRESET_MANAGER_IMPL_H
#define AMPLITRON_PRESET_MANAGER_IMPL_H

#include <string>
#include <vector>

namespace Amplitron {

// Forward declarations for non-static helper functions
bool dir_exists(const std::string& path);
std::string get_user_presets_dir();
void append_json_files(const std::string& dir, std::vector<std::string>& result);
std::string get_bundled_presets_dir();

} // namespace Amplitron

#endif // AMPLITRON_PRESET_MANAGER_IMPL_H
