#include "fixtures/file_dialog_mock.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace Amplitron {
namespace TestMocks {

static std::string g_mock_result = "";
static bool g_popen_fail = false;
static int g_exit_status = 0;
static std::vector<std::string> g_executed_commands;

void reset() {
    g_mock_result = "";
    g_popen_fail = false;
    g_exit_status = 0;
    g_executed_commands.clear();
#ifndef _WIN32
    unlink("dialog_mock_temp.txt");
    unlink("dialog_mock_script.txt");
#endif
}

void set_mock_result(const std::string& path) { g_mock_result = path; }

void set_popen_fail(bool fail) { g_popen_fail = fail; }

void set_exit_status(int status) { g_exit_status = status; }

const std::vector<std::string>& get_executed_commands() { return g_executed_commands; }

}  // namespace TestMocks
}  // namespace Amplitron

using namespace Amplitron::TestMocks;

extern "C" {

#ifdef _WIN32
BOOL WINAPI MOCK_GetOpenFileNameA(LPOPENFILENAMEA ofn) {
    if (g_popen_fail) return FALSE;
    if (ofn && ofn->lpstrFile && ofn->nMaxFile > 0) {
        std::strncpy(ofn->lpstrFile, g_mock_result.c_str(), ofn->nMaxFile - 1);
        ofn->lpstrFile[ofn->nMaxFile - 1] = '\0';
        return TRUE;
    }
    return FALSE;
}

BOOL WINAPI MOCK_GetSaveFileNameA(LPOPENFILENAMEA ofn) {
    if (g_popen_fail) return FALSE;
    if (ofn && ofn->lpstrFile && ofn->nMaxFile > 0) {
        std::strncpy(ofn->lpstrFile, g_mock_result.c_str(), ofn->nMaxFile - 1);
        ofn->lpstrFile[ofn->nMaxFile - 1] = '\0';
        return TRUE;
    }
    return FALSE;
}

LPITEMIDLIST WINAPI MOCK_SHBrowseForFolderA(LPBROWSEINFOA lpbi) {
    if (g_popen_fail) return NULL;
    g_executed_commands.push_back(lpbi && lpbi->lpszTitle ? lpbi->lpszTitle : "");
    return (LPITEMIDLIST)0x12345678;
}

BOOL WINAPI MOCK_SHGetPathFromIDListA(PCIDLIST_ABSOLUTE pidl, LPSTR pszPath) {
    if (pidl == (PCIDLIST_ABSOLUTE)0x12345678 && pszPath) {
        std::strncpy(pszPath, g_mock_result.c_str(), MAX_PATH - 1);
        pszPath[MAX_PATH - 1] = '\0';
        return TRUE;
    }
    return FALSE;
}

HRESULT WINAPI MOCK_SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags,
                                     LPSTR pszPath) {
    (void)hwnd;
    (void)csidl;
    (void)hToken;
    (void)dwFlags;
    if (pszPath) {
        std::strncpy(pszPath, "C:\\Users\\MockUser\\Desktop", MAX_PATH - 1);
        pszPath[MAX_PATH - 1] = '\0';
        return S_OK;
    }
    return E_FAIL;
}

void WINAPI MOCK_CoTaskMemFree(LPVOID pv) {
    (void)pv;
    // No-op
}

#else

FILE* MOCK_popen(const char* command, const char* type) {
    (void)type;
    g_executed_commands.push_back(command);
    if (g_popen_fail) {
        return nullptr;
    }
    FILE* tmp = fopen("dialog_mock_temp.txt", "w+");
    if (!tmp) return nullptr;
    fprintf(tmp, "%s\n", g_mock_result.c_str());
    rewind(tmp);
    return tmp;
}

int MOCK_pclose(FILE* stream) {
    FILE* volatile s = stream;
    if (s) {
        fclose(s);
        unlink("dialog_mock_temp.txt");
    }
    return (g_exit_status & 0xff) << 8;
}

#ifdef __APPLE__
int MOCK_execl(const char* path, const char* arg0, ...) {
    (void)path;
    va_list args;
    va_start(args, arg0);
    const char* flag = va_arg(args, const char*);
    (void)flag;
    const char* script = va_arg(args, const char*);
    if (script) {
        FILE* f = fopen("dialog_mock_script.txt", "w");
        if (f) {
            fprintf(f, "%s", script);
            fclose(f);
        }
    }
    va_end(args);

    printf("%s\n", g_mock_result.c_str());
    fflush(stdout);

    _exit(g_exit_status);
    return 0;
}
#endif
#endif
}
