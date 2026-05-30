#include "test_framework.h"
#include "test_fixtures.h"
#include "session_manager.h"
#include <filesystem>
#include <thread>
#include <chrono>

using namespace Amplitron;
namespace fs = std::filesystem;

TEST_F(PresetTest, session_manager_basic_lifecycle) {
    // 1. Construct SessionManager (fails back to relative paths if SDL is uninitialized or headless)
    SessionManager manager("TestOrg", "TestApp");
    
    // Clean up any stray test files at start
    manager.clearSession();
    ASSERT_FALSE(manager.hasUnsavedSession());

    // 2. Setup JSON state to save
    nlohmann::json state;
    state["format_version"] = 2;
    state["name"] = "Autosave Test Session";
    state["input_gain"] = 0.5f;

    // 3. Save session
    manager.saveSession(state);
    ASSERT_TRUE(manager.hasUnsavedSession());

    // 4. Load session and verify content
    nlohmann::json loaded = manager.loadSession();
    ASSERT_EQ(loaded["format_version"].get<int>(), 2);
    ASSERT_EQ(loaded["name"].get<std::string>(), "Autosave Test Session");
    ASSERT_NEAR(loaded["input_gain"].get<float>(), 0.5f, 0.01f);

    // 5. Clear session and verify removal
    manager.clearSession();
    ASSERT_FALSE(manager.hasUnsavedSession());
}

TEST_F(PresetTest, session_manager_should_save_timer) {
    SessionManager manager("TestOrg", "TestApp");
    
    // By default, since the last save time was set to now in the constructor, 
    // shouldSave() should return false immediately.
    ASSERT_FALSE(manager.shouldSave());
}

TEST_F(PresetTest, session_manager_corrupted_load_handling) {
    SessionManager manager("TestOrg", "TestApp");
    manager.clearSession();

    // Write a corrupted/empty file to the path to check parser robustness
    // Let's create it manually
    std::string bad_path = "autosave.json";
    register_temp_file(bad_path);
    register_temp_file("autosave.tmp");

    std::ofstream f(bad_path);
    f << "{ invalid_json_here }";
    f.close();

    // SessionManager loadSession returns empty JSON or throws parse_error.
    // Let's verify it behaves deterministically (either returning empty/null or throwing)
    bool threw_or_empty = false;
    try {
        nlohmann::json state = manager.loadSession();
        if (state.empty() || state.is_null()) {
            threw_or_empty = true;
        }
    } catch (...) {
        threw_or_empty = true;
    }
    ASSERT_TRUE(threw_or_empty);

    manager.clearSession();
}
