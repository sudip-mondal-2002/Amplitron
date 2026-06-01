#pragma once

#include "audio/engine/audio_engine.h"
#include "audio/effects/tuner.h"
#include "gui/ui_component.h"
#include <functional>
#include <memory>

namespace Amplitron {

struct TunerProps {
    bool has_signal = false;
    int  note_idx   = -1;
    int  octave     = 4;
    float cents     = 0.0f;
    float freq      = 0.0f;
    bool mute_on    = false;
    float a4_ref    = 440.0f;

    std::function<void(bool)>  on_mute_changed;  // arg: new mute state
    std::function<void(float)> on_a4_ref_changed;

    // Needed to get note name string
    std::function<const char*(int)> note_name_fn;
};

/**
 * @brief Reactive chromatic tuner modal component.
 *
 * Receives tuner signal data through TunerProps.
 * Callbacks handle mute toggle and A4 reference changes.
 */
class GuiTuner : public UIComponent<TunerProps> {
public:
    GuiTuner() = default;

    /**
     * @brief Render the tuner modal.
     * Uses the internal show_ flag — call render(show) for external visibility control.
     */
    void render() override { render(show_); }

    /** @brief Render with an external show flag (GuiManager holds authoritative state). */
    void render(bool& show);

private:
    bool show_ = true;
};

} // namespace Amplitron
