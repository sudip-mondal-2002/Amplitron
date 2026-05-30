// =============================================================================
// Native file dialog implementations (Windows, macOS, Linux)
// Folder dialog implementation
// =============================================================================

#include "gui/dialogs/file_dialog.h"
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#else
// Required for popen, pclose, fgets, FILE on non-Windows builds
#include <cstdio>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace Amplitron {

#ifdef AMPLITRON_HEADLESS
std::string show_folder_dialog(const std::string&) {
    return "";
}
#else

#ifdef _WIN32
std::string show_folder_dialog(const std::string& title) {
    BROWSEINFOA bi;
    std::memset(&bi, 0, sizeof(bi));
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return "";

    char path[MAX_PATH];
    bool ok = SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);
    return ok ? std::string(path) : "";
}

#elif defined(__APPLE__) && !TARGET_OS_IOS
std::string show_folder_dialog(const std::string& title) {
    // Sanitize title for embedding in an AppleScript string literal:
    // escape backslashes first, then double-quotes.
    std::string safe_title;
    for (char c : title) {
        if (c == '\\') { safe_title += "\\\\"; }
        else if (c == '"') { safe_title += "\\\""; }
        else { safe_title += c; }
    }

    std::string script = "POSIX path of (choose folder with prompt \"" + safe_title + "\")";

    // Use fork+exec to invoke /usr/bin/osascript directly, bypassing /bin/sh
    // so the script string is never interpreted by a shell.
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "";
    }

    if (pid == 0) {
        // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execl("/usr/bin/osascript", "osascript", "-e", script.c_str(), nullptr);
        _exit(1);
    }

    // Parent process
    close(pipefd[1]);
    char buf[1024];
    std::string result;
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        result.append(buf, static_cast<size_t>(n));
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    // osascript returns paths with trailing slash; strip it for consistency
    if (!result.empty() && result.back() == '/') result.pop_back();

    return result;
}

#else // Linux
std::string show_folder_dialog(const std::string& title) {
    // Sanitize title for single-quote shell embedding: replace ' with '\''
    std::string safe_title;
    for (char c : title) {
        if (c == '\'') { safe_title += "'\\''"; }
        else { safe_title += c; }
    }

    std::string cmd = "zenity --file-selection --directory "
                      "--title='" + safe_title + "' 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    char buf[1024];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    int wait_status = pclose(pipe);
    int exit_code = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;

    if (exit_code != 0) {
        // Exit status 1 means the user cancelled in zenity — return empty.
        if (exit_code == 1) return "";

        // Any other non-zero status means zenity is unavailable; try kdialog.
        cmd = "kdialog --getexistingdirectory ~/ --title '" + safe_title + "' 2>/dev/null";
        pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        result.clear();
        while (fgets(buf, sizeof(buf), pipe)) {
            result += buf;
        }
        pclose(pipe);
    }

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    return result;
}
#endif

#endif // AMPLITRON_HEADLESS

} // namespace Amplitron
