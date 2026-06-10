/**
 * @file test_file_dialog.cpp
 * @brief Tests for file dialog headless mock paths.
 *
 * Under AMPLITRON_HEADLESS the native dialog functions are replaced by
 * controllable mocks.  These tests exercise every reachable code path
 * in file_dialog_native_open.cpp (the mock implementation), as well as
 * the stub implementations in file_dialog_native.cpp (save) and
 * file_dialog_native_folder.cpp (folder).
 */

#include <string>

#include "gui/dialogs/file_dialog.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

// Forward-declare the mock setter (defined in file_dialog_native_open.cpp
// under AMPLITRON_HEADLESS).
namespace Amplitron {
void set_mock_open_dialog_path(const std::string& path);
}

// =============================================================================
// show_open_dialog — headless mock
// =============================================================================

TEST(file_dialog_open_returns_empty_by_default) {
    // Without setting a mock path, the dialog must return "".
    Amplitron::set_mock_open_dialog_path("");
    std::string result = show_open_dialog("Open File", "WAV Audio", "wav");
    ASSERT_EQ(result, "");
}

TEST(file_dialog_open_returns_mock_path) {
    // After setting a mock path, show_open_dialog must return it verbatim.
    Amplitron::set_mock_open_dialog_path("/tmp/test_file.wav");
    std::string result = show_open_dialog("Open File", "WAV Audio", "wav");
    ASSERT_EQ(result, "/tmp/test_file.wav");
}

TEST(file_dialog_open_ignores_title_and_filter_params) {
    // The mock path must be returned regardless of the title/filter arguments.
    Amplitron::set_mock_open_dialog_path("my_ir.wav");
    ASSERT_EQ(show_open_dialog("Load Impulse Response", "WAV Audio", "wav"), "my_ir.wav");
    ASSERT_EQ(show_open_dialog("Select File", "All Files", "*"), "my_ir.wav");
    ASSERT_EQ(show_open_dialog("", "", ""), "my_ir.wav");
}

TEST(file_dialog_open_can_be_reset) {
    // Set a path, verify it is returned, reset, verify empty.
    Amplitron::set_mock_open_dialog_path("first.wav");
    ASSERT_EQ(show_open_dialog(), "first.wav");

    Amplitron::set_mock_open_dialog_path("");
    ASSERT_EQ(show_open_dialog(), "");
}

TEST(file_dialog_open_can_be_overwritten) {
    // Setting a new mock path overwrites the previous one.
    Amplitron::set_mock_open_dialog_path("a.wav");
    ASSERT_EQ(show_open_dialog(), "a.wav");

    Amplitron::set_mock_open_dialog_path("b.wav");
    ASSERT_EQ(show_open_dialog(), "b.wav");
}

TEST(file_dialog_open_default_params) {
    // Call with default arguments (title="Open File", desc="WAV Audio",
    // ext="wav").
    Amplitron::set_mock_open_dialog_path("default_args.wav");
    ASSERT_EQ(show_open_dialog(), "default_args.wav");
}

TEST(file_dialog_open_unicode_path) {
    // The mock must handle paths with unicode characters.
    const std::string unicode_path = "/tmp/日本語ファイル.wav";
    Amplitron::set_mock_open_dialog_path(unicode_path);
    ASSERT_EQ(show_open_dialog("Open", "WAV", "wav"), unicode_path);
}

TEST(file_dialog_open_path_with_spaces) {
    // The mock must handle paths containing spaces.
    const std::string spaced_path = "/my folder/my file.wav";
    Amplitron::set_mock_open_dialog_path(spaced_path);
    ASSERT_EQ(show_open_dialog("Open", "WAV", "wav"), spaced_path);
}

TEST(file_dialog_open_path_with_special_chars) {
    // The mock must handle paths with special characters (quotes,
    // backslashes).
    const std::string special_path = R"(C:\Users\test\"file".wav)";
    Amplitron::set_mock_open_dialog_path(special_path);
    ASSERT_EQ(show_open_dialog("Open", "WAV", "wav"), special_path);
}

TEST(file_dialog_open_consecutive_calls_same_result) {
    // Multiple consecutive calls without resetting should return the same mock
    // path.
    Amplitron::set_mock_open_dialog_path("stable.wav");
    ASSERT_EQ(show_open_dialog(), "stable.wav");
    ASSERT_EQ(show_open_dialog(), "stable.wav");
    ASSERT_EQ(show_open_dialog(), "stable.wav");
}

// =============================================================================
// show_save_dialog — headless stub (always returns "")
// =============================================================================

TEST(file_dialog_save_returns_empty_in_headless) {
    std::string result = show_save_dialog("recording.wav", "WAV Audio", "wav");
    ASSERT_EQ(result, "");
}

TEST(file_dialog_save_default_params) {
    // Call with default arguments.
    ASSERT_EQ(show_save_dialog(), "");
}

TEST(file_dialog_save_custom_params) {
    // Custom arguments should still return "" in headless mode.
    ASSERT_EQ(show_save_dialog("my_preset.json", "Preset Files", "json"), "");
}

// =============================================================================
// show_folder_dialog — headless stub (always returns "")
// =============================================================================

TEST(file_dialog_folder_returns_empty_in_headless) {
    std::string result = show_folder_dialog("Select Folder");
    ASSERT_EQ(result, "");
}

TEST(file_dialog_folder_default_params) {
    // Call with default argument.
    ASSERT_EQ(show_folder_dialog(), "");
}

TEST(file_dialog_folder_custom_title) { ASSERT_EQ(show_folder_dialog("Choose IR Directory"), ""); }
