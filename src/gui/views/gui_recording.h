#pragma once

#include "gui/ui_component.h"
#include <functional>
#include <string>
#include <cstdint>

namespace Amplitron {

struct RecordingProps {
    bool    is_recording   = false;
    bool    is_paused      = false;
    bool    has_unsaved    = false;
    float   duration       = 0.0f;
    float   current_peak   = 0.0f;
    int64_t samples_written = 0;
    int     channels       = 1;
    int     sample_rate    = 44100;

    // Waveform preview (valid only while is_recording == true)
    const float* waveform_buf  = nullptr;
    int          waveform_size = 0;

    std::function<void()> on_resume;
    std::function<void()> on_pause;
    std::function<void()> on_stop;    // stops recording; GuiManager then calls render_save_dialog
    std::function<void()> on_start;
    std::function<void()> on_discard;
};

struct RecordingState {
    bool        needs_save = false;   // true after stop/save-as is requested
    std::string status_msg;
};

/**
 * @brief Reactive recording controls component.
 *
 * Receives all recording state via RecordingProps from GuiManager each frame.
 * Communicates recording lifecycle events back through callbacks in Props;
 * never touches the engine or recorder directly.
 *
 * Save-dialog flow:
 *   1. User clicks STOP or "Save As..." → state_.needs_save set true.
 *   2. GuiManager polls needs_save_dialog() after render().
 *   3. GuiManager calls render_save_dialog(callback) which runs the native
 *      file picker and invokes the callback with the destination path.
 */
class GuiRecording : public UIComponent<RecordingProps, RecordingState> {
public:
    GuiRecording() = default;

    /** @brief Render recording controls (start/stop/pause/waveform). */
    void render() override;

    /**
     * @brief Show the native "Save Recording As…" file dialog.
     *
     * Must be called by GuiManager when needs_save_dialog() returns true.
     * Resets the internal flag after running (so it fires exactly once).
     * @param on_save_done Called with the chosen path; caller writes the file.
     */
    void render_save_dialog(std::function<void(const std::string& dest)> on_save_done);

    /** @brief Returns true if the save dialog should be opened this frame. */
    bool needs_save_dialog() const { return state_.needs_save; }
};

} // namespace Amplitron
