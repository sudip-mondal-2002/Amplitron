#pragma once
#include <cstdio>
#include <string>
#include <vector>

namespace Amplitron {
namespace TestMocks {
void reset();
void set_mock_result(const std::string& path);
void set_popen_fail(bool fail);
void set_exit_status(int status);
const std::vector<std::string>& get_executed_commands();
}  // namespace TestMocks
}  // namespace Amplitron

extern "C" {
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
// clang-format on
// Use standard calling convention declarations matching Win32 APIs
BOOL WINAPI MOCK_GetOpenFileNameA(LPOPENFILENAMEA ofn);
BOOL WINAPI MOCK_GetSaveFileNameA(LPOPENFILENAMEA ofn);
LPITEMIDLIST WINAPI MOCK_SHBrowseForFolderA(LPBROWSEINFOA lpbi);
BOOL WINAPI MOCK_SHGetPathFromIDListA(PCIDLIST_ABSOLUTE pidl, LPSTR pszPath);
HRESULT WINAPI MOCK_SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags,
                                     LPSTR pszPath);
void WINAPI MOCK_CoTaskMemFree(LPVOID pv);
#else
FILE* MOCK_popen(const char* command, const char* type);
int MOCK_pclose(FILE* stream);
#ifdef __APPLE__
int MOCK_execl(const char* path, const char* arg0, ...);
#endif
#endif
}
