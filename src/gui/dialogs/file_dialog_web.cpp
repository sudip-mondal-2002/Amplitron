// =============================================================================
// Web / Emscripten file dialog
//
// Native file pickers are unavailable in browsers. Instead we inject a hidden
// <input type="file"> element via EM_ASM, read the selected file into
// Emscripten's in-memory FS under /tmp/amplitron_upload/, and surface the
// virtual path to the C++ caller. The file picker is triggered on the frame
// of the button click; the path becomes available the following frame.
// =============================================================================
#include "gui/dialogs/file_dialog.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <sys/stat.h>

#include <string>

namespace Amplitron {

// ---------------------------------------------------------------------------
// Shared pending-path state (main-thread only — ImGui render loop)
// ---------------------------------------------------------------------------
static std::string s_pending_open_path;

// Called from JavaScript (see shell.html) once the user's file has been
// written into Emscripten's FS.  The path argument is a null-terminated C
// string like "/tmp/amplitron_upload/model.nam".
extern "C" EMSCRIPTEN_KEEPALIVE void amplitron_on_file_uploaded(const char* path) {
    if (path) {
        s_pending_open_path = path;
    }
}

std::string show_save_dialog(const std::string& /*default_name*/,
                             const std::string& /*filter_desc*/,
                             const std::string& /*filter_ext*/) {
    // No native save dialog on web — saving is handled via amplitronSaveFile
    // in shell.html (triggered from the recorder).
    return "";
}

std::string show_open_dialog(const std::string& /*title*/,
                             const std::string& /*filter_desc*/,
                             const std::string& filter_ext) {
    // If a previously-uploaded file is waiting, consume and return it.
    if (!s_pending_open_path.empty()) {
        std::string path = s_pending_open_path;
        s_pending_open_path.clear();
        return path;
    }

    // Otherwise trigger the browser file picker for the requested extension.
    // The JS callback (amplitron_on_file_uploaded) will fire asynchronously
    // and populate s_pending_open_path for the next frame.
    //
    // Note: EM_ASM_({ ... }, arg) passes a C++ value into JS as $0.
    EM_ASM({
        var ext = UTF8ToString($0);
        var accept = ext ? ('.' + ext) : '*';

        // Reuse an existing hidden input if one is already in the DOM.
        var inp = document.getElementById('_amplitron_file_input');
        if (!inp) {
            inp = document.createElement('input');
            inp.id  = '_amplitron_file_input';
            inp.type = 'file';
            inp.style.display = 'none';
            document.body.appendChild(inp);
        }
        inp.accept = accept;
        // Remove any previous listener to avoid stacking callbacks.
        inp.onchange = null;
        inp.value = '';

        inp.onchange = function(ev) {
            var file = ev.target.files[0];
            if (!file) return;

            var reader = new FileReader();
            reader.onload = function(re) {
                var data = new Uint8Array(re.target.result);
                var dir  = '/tmp/amplitron_upload';
                try { FS.mkdir(dir); } catch(e) {}
                var path = dir + '/' + file.name;
                FS.writeFile(path, data);
                // Notify C++ that the file is ready.
                Module.ccall(
                    'amplitron_on_file_uploaded',
                    null,
                    ['string'],
                    [path]
                );
            };
            reader.readAsArrayBuffer(file);
        };

        inp.click();
    }, filter_ext.c_str());

    return "";  // Path will be returned on the next call (next frame).
}

std::string show_folder_dialog(const std::string& /*title*/) {
    // Folder picking is not required for any current web workflow.
    return "";
}

// Non-blocking poll — drains whatever was set by amplitron_on_file_uploaded().
std::string poll_uploaded_file_path() {
    if (s_pending_open_path.empty()) return "";
    std::string path = s_pending_open_path;
    s_pending_open_path.clear();
    return path;
}

}  // namespace Amplitron

#else
// Non-Emscripten fallback (should never be compiled — the CMake config
// selects file_dialog_native.cpp for all non-web targets).
#include <string>
namespace Amplitron {
std::string show_save_dialog(const std::string&, const std::string&, const std::string&) { return ""; }
std::string show_open_dialog(const std::string&, const std::string&, const std::string&) { return ""; }
std::string show_folder_dialog(const std::string&) { return ""; }
std::string poll_uploaded_file_path() { return ""; }
}  // namespace Amplitron
#endif
