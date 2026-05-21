// =============================================================================
// Native file dialog implementations (Windows, macOS, Linux)
// Save dialog implementation
// =============================================================================

#include "gui/file_dialog.h"
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace Amplitron {

#ifdef _WIN32
std::string show_save_dialog(const std::string& default_name,
                             const std::string& filter_desc,
                             const std::string& filter_ext) {
    char filename[MAX_PATH];
    std::strncpy(filename, default_name.c_str(), MAX_PATH - 1);
    filename[MAX_PATH - 1] = '\0';

    // Build filter string: "WAV Audio (*.wav)\0*.wav\0All Files (*.*)\0*.*\0\0"
    char filter[256];
    std::memset(filter, 0, sizeof(filter));
    int pos = 0;
    pos += snprintf(filter + pos, 256 - pos, "%s (*.%s)", filter_desc.c_str(), filter_ext.c_str());
    pos++; // null separator
    pos += snprintf(filter + pos, 256 - pos, "*.%s", filter_ext.c_str());
    pos++; // null separator
    pos += snprintf(filter + pos, 256 - pos, "All Files (*.*)");
    pos++;
    pos += snprintf(filter + pos, 256 - pos, "*.*");
    // double null terminator is already there from memset

    // Get desktop/documents as initial dir
    char initial_dir[MAX_PATH] = "";
    SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, 0, initial_dir);

    OPENFILENAMEA ofn;
    std::memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initial_dir;
    ofn.lpstrTitle = "Save Recording As";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = filter_ext.c_str();

    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
    return "";
}

#elif defined(__APPLE__) && !TARGET_OS_IOS
std::string show_save_dialog(const std::string& default_name,
                             const std::string& /*filter_desc*/,
                             const std::string& filter_ext) {
    // Use osascript to show a native NSSavePanel
    std::string cmd = "osascript -e 'set theFile to POSIX path of (choose file name "
                      "with prompt \"Save Recording As\" "
                      "default name \"" + default_name + "\")' 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    char buf[1024];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    pclose(pipe);

    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    if (result.empty()) return "";

    // Ensure it ends with the correct extension
    if (result.size() < filter_ext.size() + 1 ||
        result.substr(result.size() - filter_ext.size() - 1) != "." + filter_ext) {
        result += "." + filter_ext;
    }

    return result;
}

#else // Linux
std::string show_save_dialog(const std::string& default_name,
                             const std::string& filter_desc,
                             const std::string& filter_ext) {
    // Try zenity first, then kdialog
    std::string cmd = "zenity --file-selection --save --confirm-overwrite "
                      "--title='Save Recording As' "
                      "--filename='" + default_name + "' "
                      "--file-filter='" + filter_desc + " (*." + filter_ext + ")|*." + filter_ext + "' "
                      "--file-filter='All Files (*)|*' 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    char buf[1024];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    int status = pclose(pipe);

    // If zenity failed (not installed), try kdialog
    if (status != 0) {
        cmd = "kdialog --getsavefilename ~/ '*." + filter_ext + "|" + filter_desc + "' "
              "--title 'Save Recording As' 2>/dev/null";
        pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        result.clear();
        while (fgets(buf, sizeof(buf), pipe)) {
            result += buf;
        }
        pclose(pipe);
    }

    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    if (result.empty()) return "";

    // Ensure it ends with the correct extension
    if (result.size() < filter_ext.size() + 1 ||
        result.substr(result.size() - filter_ext.size() - 1) != "." + filter_ext) {
        result += "." + filter_ext;
    }

    return result;
}
#endif

} // namespace Amplitron
