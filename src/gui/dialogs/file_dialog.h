#pragma once

#include <string>

namespace Amplitron {

#ifdef AMPLITRON_HEADLESS
extern std::string g_mock_save_dialog_result;
extern std::string g_mock_open_dialog_result;
extern std::string g_mock_folder_dialog_result;
#endif

// Cross-platform native save file dialog
// Returns empty string if user cancelled
std::string show_save_dialog(const std::string& default_name = "recording.wav",
                             const std::string& filter_desc = "WAV Audio",
                             const std::string& filter_ext = "wav");

// Cross-platform native file open dialog
// Returns empty string if user cancelled
std::string show_open_dialog(const std::string& title = "Open File",
                             const std::string& filter_desc = "WAV Audio",
                             const std::string& filter_ext = "wav");

// Cross-platform native folder picker dialog
// Returns empty string if user cancelled
std::string show_folder_dialog(const std::string& title = "Select Folder");

// Non-blocking poll for a file path that was asynchronously uploaded by the
// browser (Emscripten / web builds only).  On all native platforms this always
// returns "".  Call once per rendered frame from widgets that need async file
// loading (e.g. the NAM Loader pedal on web).
std::string poll_uploaded_file_path();

}  // namespace Amplitron
