// =============================================================================
// Web / Emscripten file dialog stub
//
// No native file dialogs in a browser environment — returns empty strings.
// =============================================================================
#include "gui/dialogs/file_dialog.h"

namespace Amplitron {

std::string show_save_dialog(const std::string& /*default_name*/,
                             const std::string& /*filter_desc*/,
                             const std::string& /*filter_ext*/) {
    return "";
}

std::string show_open_dialog(const std::string& /*title*/,
                             const std::string& /*filter_desc*/,
                             const std::string& /*filter_ext*/) {
    return "";
}

std::string show_folder_dialog(const std::string& /*title*/) {
    return "";
}

} // namespace Amplitron
