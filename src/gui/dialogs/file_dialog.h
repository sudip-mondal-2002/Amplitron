#pragma once

#include <string>

namespace Amplitron {

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

} // namespace Amplitron
