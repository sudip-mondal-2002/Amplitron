#include <filesystem>
#include <fstream>
#include <iostream>

#include "presets/preset_components.h"
#include "presets/preset_manager.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;

TEST_F(PresetTest, test_preset_storage_lifecycle_and_list) {
    // 1. Setup a clean temp directory for custom presets
    std::string temp_dir =
        (std::filesystem::temp_directory_path() / "amplitron_preset_storage_test").string();
    register_temp_dir(temp_dir);

    // Set custom presets directory
    PresetManager::set_presets_dir(temp_dir);

    PresetStorage storage;

    // Save a preset file using storage
    std::string test_preset_path = temp_dir + "/test_preset.json";
    std::string preset_data = "{\"test\": true}";
    bool save_success = storage.save(test_preset_path, preset_data);
    ASSERT_TRUE(save_success);

    // Load it back
    std::string loaded_data = storage.load(test_preset_path);
    ASSERT_EQ(loaded_data, preset_data);

    // Load non-existent file
    std::string missing_data = storage.load(temp_dir + "/does_not_exist.json");
    ASSERT_TRUE(missing_data.empty());

    // List presets - should find the test_preset.json
    auto files = storage.list();
    bool found = false;
    for (const auto& f : files) {
        try {
            if (std::filesystem::equivalent(f, test_preset_path)) {
                found = true;
                break;
            }
        } catch (...) {
            // Path comparison failed or file does not exist
        }
    }
    ASSERT_TRUE(found);

    // Remove the preset
    bool remove_success = storage.remove(test_preset_path);
    ASSERT_TRUE(remove_success);

    // Verify it's deleted
    ASSERT_FALSE(std::filesystem::exists(test_preset_path));

    // Remove non-existent file
    bool remove_fail = storage.remove(temp_dir + "/does_not_exist.json");
    ASSERT_FALSE(remove_fail);

    // Save to invalid path should fail
    std::filesystem::path invalid_path =
        std::filesystem::temp_directory_path() / "nonexistent_xyz" / "preset.json";
    bool invalid_save = storage.save(invalid_path.string(), "{}");
    ASSERT_FALSE(invalid_save);
}
