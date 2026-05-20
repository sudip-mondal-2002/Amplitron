#include "gui/file_dialog.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace Amplitron {

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

// --- Open dialog -----------------------------------------------------------
static bool     g_open_ready = false;
static char     g_open_path_buf[1024];

extern "C" void EMSCRIPTEN_KEEPALIVE amplitron_on_open_file(void) {
    g_open_ready = true;
}

std::string show_open_dialog(const std::string& /*title*/,
                              const std::string& /*filter_desc*/,
                              const std::string& filter_ext) {
    g_open_ready        = false;
    g_open_path_buf[0]  = '\0';

    std::string accept = "." + filter_ext;
    const char* accept_cstr = accept.c_str();
    char*       buf_ptr    = g_open_path_buf;

    MAIN_THREAD_EM_ASM({
        var accept  = UTF8ToString($0);
        var bufPtr  = $1;
        var timeout = setTimeout(function() {
            stringToUTF8('', bufPtr, 1024);
            _amplitron_on_open_file();
        }, 60000);

        var input = document.createElement('input');
        input.type   = 'file';
        input.accept = accept;
        input.onchange = function(e) {
            clearTimeout(timeout);
            var file = e.target.files[0];
            if (!file) {
                stringToUTF8('', bufPtr, 1024);
                _amplitron_on_open_file();
                return;
            }
            var reader = new FileReader();
            reader.onload = function() {
                var data = new Uint8Array(reader.result);
                var path = '/' + file.name;
                try { FS.unlink(path); } catch(e) {}
                FS.writeFile(path, data);
                stringToUTF8(path, bufPtr, 1024);
                _amplitron_on_open_file();
            };
            reader.onerror = function() {
                stringToUTF8('', bufPtr, 1024);
                _amplitron_on_open_file();
            };
            reader.readAsArrayBuffer(file);
        };
        input.click();
    }, accept_cstr, buf_ptr);

    while (!g_open_ready) {
        emscripten_sleep(100);
    }

    if (g_open_path_buf[0] == '\0')
        return "";

    return std::string(g_open_path_buf);
}

// --- Save dialog -----------------------------------------------------------
struct PendingSave {
    char path[256];
    char name[256];
};

EM_JS(void, js_trigger_download, (const char* path, const char* name), {
    var p = UTF8ToString(path);
    var n = UTF8ToString(name);
    try {
        var data = FS.readFile(p);
        var blob = new Blob([data], {type: 'application/octet-stream'});
        var url  = URL.createObjectURL(blob);
        var a    = document.createElement('a');
        a.href   = url;
        a.download = n;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        try { FS.unlink(p); } catch(e) {}
    } catch(e) {
        console.error('Amplitron download failed:', e);
    }
});

extern "C" void EMSCRIPTEN_KEEPALIVE amplitron_flush_save(void* userData) {
    auto* ps = static_cast<PendingSave*>(userData);
    js_trigger_download(ps->path, ps->name);
    delete ps;
}

std::string show_save_dialog(const std::string& default_name,
                              const std::string& /*filter_desc*/,
                              const std::string& filter_ext) {
    static int counter = 0;
    char memfs_path[128];
    snprintf(memfs_path, sizeof(memfs_path), "/amplitron_save_%d.%s",
             ++counter, filter_ext.c_str());

    auto* ps    = new PendingSave();
    std::strncpy(ps->path, memfs_path, sizeof(ps->path) - 1);
    ps->path[sizeof(ps->path) - 1] = '\0';

    std::string save_name = default_name.empty()
                                ? ("download." + filter_ext)
                                : default_name;
    std::strncpy(ps->name, save_name.c_str(), sizeof(ps->name) - 1);
    ps->name[sizeof(ps->name) - 1] = '\0';

    emscripten_async_call(amplitron_flush_save, ps, 0);

    return memfs_path;
}

// --- Folder dialog (not supported on web) ---------------------------------
std::string show_folder_dialog(const std::string& /*title*/) {
    return "";
}

#else // iOS stub (no native file dialogs available)

std::string show_open_dialog(const std::string& /*title*/,
                              const std::string& /*filter_desc*/,
                              const std::string& /*filter_ext*/) {
    return "";
}

std::string show_save_dialog(const std::string& /*default_name*/,
                              const std::string& /*filter_desc*/,
                              const std::string& /*filter_ext*/) {
    return "";
}

std::string show_folder_dialog(const std::string& /*title*/) {
    return "";
}

#endif // __EMSCRIPTEN__

} // namespace Amplitron
