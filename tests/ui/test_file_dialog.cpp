/**
 * @file test_file_dialog.cpp
 * @brief Tests for file dialog native paths using system mocks.
 */

#include <fstream>
#include <string>

#include "fixtures/file_dialog_mock.h"
#include "gui/dialogs/file_dialog.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

namespace {
std::string read_mock_script() {
    std::ifstream f("dialog_mock_script.txt");
    if (!f.is_open()) return "";
    std::string result((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return result;
}
}  // namespace

// =============================================================================
// Helper test setups
// =============================================================================

class FileDialogNativeTest : public Test {
   public:
    void SetUp() override { Amplitron::TestMocks::reset(); }
    void TearDown() override { Amplitron::TestMocks::reset(); }
};

// =============================================================================
// Platform-specific Native Dialog Tests
// =============================================================================

#ifndef _WIN32

// POSIX (macOS & Linux) Tests
TEST_F(FileDialogNativeTest, ShowOpenDialogNativeSuccess) {
    Amplitron::TestMocks::set_mock_result("/path/to/my_ir.wav");
    std::string result = show_open_dialog("Choose IR", "Impulse Response", "wav");

    ASSERT_EQ(result, "/path/to/my_ir.wav");

#ifdef __APPLE__
    // On macOS: uses fork + execl to run AppleScript
    std::string script = read_mock_script();
    ASSERT_NE(script.find("choose file of type {\"wav\"}"), std::string::npos);
    ASSERT_NE(script.find("with prompt \"Choose IR\""), std::string::npos);
#else
    // On Linux: uses popen to run zenity/kdialog
    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_GE(cmds.size(), 1UL);
    ASSERT_NE(cmds[0].find("zenity --file-selection"), std::string::npos);
    ASSERT_NE(cmds[0].find("--title='Choose IR'"), std::string::npos);
#endif
}

TEST_F(FileDialogNativeTest, ShowOpenDialogNativeSanitization) {
    Amplitron::TestMocks::set_mock_result("/some/file.wav");
    // Test sanitization of special characters in title and extensions
    std::string result = show_open_dialog("Prompt \"with\" 'quotes' \\", "Desc", "wav");
    ASSERT_EQ(result, "/some/file.wav");

#ifdef __APPLE__
    std::string script = read_mock_script();
    // macOS sanitizes double quotes and backslashes for AppleScript
    ASSERT_NE(script.find("with prompt \"Prompt \\\"with\\\" 'quotes' \\\\\""), std::string::npos);
#else
    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_GE(cmds.size(), 1UL);
    // Linux sanitizes single quotes for shell
    ASSERT_NE(cmds[0].find("Prompt \"with\" '\\''quotes'\\'' \\"), std::string::npos);
#endif
}

#ifndef __APPLE__
TEST_F(FileDialogNativeTest, ShowOpenDialogLinuxZenityFallback) {
    Amplitron::TestMocks::set_mock_result("/path/to/kdialog_file.wav");
    // Simulate zenity failing (status 1) to trigger kdialog fallback
    Amplitron::TestMocks::set_exit_status(1);

    std::string result = show_open_dialog("Select IR", "WAV", "wav");
    ASSERT_EQ(result, "/path/to/kdialog_file.wav");

    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_EQ(cmds.size(), 2UL);
    ASSERT_NE(cmds[0].find("zenity"), std::string::npos);
    ASSERT_NE(cmds[1].find("kdialog"), std::string::npos);
}
#endif

TEST_F(FileDialogNativeTest, ShowSaveDialogNativeSuccess) {
    Amplitron::TestMocks::set_mock_result("/recordings/my_take");
    std::string result = show_save_dialog("my_take.wav", "WAV Audio", "wav");

    // Extension should be appended if missing
    ASSERT_EQ(result, "/recordings/my_take.wav");

#ifdef __APPLE__
    // macOS save dialog uses popen
    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_GE(cmds.size(), 1UL);
    ASSERT_NE(cmds[0].find("osascript"), std::string::npos);
    ASSERT_NE(cmds[0].find("choose file name"), std::string::npos);
#else
    // Linux save dialog uses popen (zenity)
    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_GE(cmds.size(), 1UL);
    ASSERT_NE(cmds[0].find("zenity --file-selection --save"), std::string::npos);
#endif
}

#ifndef __APPLE__
TEST_F(FileDialogNativeTest, ShowSaveDialogLinuxZenityFallback) {
    Amplitron::TestMocks::set_mock_result("/kdialog/save");
    Amplitron::TestMocks::set_exit_status(1);

    std::string result = show_save_dialog("recording.wav", "WAV", "wav");
    ASSERT_EQ(result, "/kdialog/save.wav");

    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_EQ(cmds.size(), 2UL);
    ASSERT_NE(cmds[0].find("zenity"), std::string::npos);
    ASSERT_NE(cmds[1].find("kdialog"), std::string::npos);
}
#endif

TEST_F(FileDialogNativeTest, ShowFolderDialogNativeSuccess) {
    Amplitron::TestMocks::set_mock_result("/path/to/folder/");
    std::string result = show_folder_dialog("Choose Folder");

    // Trailing slash should be stripped (macOS) or returned correctly
#ifdef __APPLE__
    ASSERT_EQ(result, "/path/to/folder");
    std::string script = read_mock_script();
    ASSERT_NE(script.find("choose folder with prompt \"Choose Folder\""), std::string::npos);
#else
    ASSERT_EQ(result, "/path/to/folder/");
    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_GE(cmds.size(), 1UL);
    ASSERT_NE(cmds[0].find("zenity --file-selection --directory"), std::string::npos);
#endif
}

#ifndef __APPLE__
TEST_F(FileDialogNativeTest, ShowFolderDialogLinuxZenityFallback) {
    Amplitron::TestMocks::set_mock_result("/kdialog/folder");
    // Set exit code to something other than 0 or 1 (e.g. 127 for missing command)
    Amplitron::TestMocks::set_exit_status(127);

    std::string result = show_folder_dialog("My Folder");
    ASSERT_EQ(result, "/kdialog/folder");

    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_EQ(cmds.size(), 2UL);
    ASSERT_NE(cmds[0].find("zenity"), std::string::npos);
    ASSERT_NE(cmds[1].find("kdialog"), std::string::npos);
}

TEST_F(FileDialogNativeTest, ShowFolderDialogLinuxZenityCancel) {
    Amplitron::TestMocks::set_mock_result("/should/not/matter");
    // Exit status 1 means the user cancelled in zenity — return empty.
    Amplitron::TestMocks::set_exit_status(1);

    std::string result = show_folder_dialog("Cancel Me");
    ASSERT_EQ(result, "");

    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_EQ(cmds.size(), 1UL);
    ASSERT_NE(cmds[0].find("zenity"), std::string::npos);
}
#endif

#else

// Windows Native Dialog Tests
TEST_F(FileDialogNativeTest, ShowOpenDialogWindowsSuccess) {
    Amplitron::TestMocks::set_mock_result("C:\\presets\\preset.json");
    std::string result = show_open_dialog("Open Preset", "Presets", "json");

    ASSERT_EQ(result, "C:\\presets\\preset.json");
}

TEST_F(FileDialogNativeTest, ShowSaveDialogWindowsSuccess) {
    Amplitron::TestMocks::set_mock_result("C:\\recordings\\take1.wav");
    std::string result = show_save_dialog("take1.wav", "WAV", "wav");

    ASSERT_EQ(result, "C:\\recordings\\take1.wav");
}

TEST_F(FileDialogNativeTest, ShowFolderDialogWindowsSuccess) {
    Amplitron::TestMocks::set_mock_result("C:\\ir_folders");
    std::string result = show_folder_dialog("Select Directory");

    ASSERT_EQ(result, "C:\\ir_folders");
    const auto& cmds = Amplitron::TestMocks::get_executed_commands();
    ASSERT_EQ(cmds.size(), 1UL);
    ASSERT_EQ(cmds[0], "Select Directory");
}

#endif
