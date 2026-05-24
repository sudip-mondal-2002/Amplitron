#pragma once

#include "common.h"
#include "audio/effect.h"
#include <imgui.h>

namespace Amplitron {

class CommandHistory;
class AudioEngine;
class GuiMidi;

/**
 * @brief GUI widget for a single audio effect pedal.
 *
 * Draws the pedal body, rotary knobs, footswitch, and LED indicator.
 * All parameter changes are tracked for undo/redo via a CommandHistory
 * pointer injected with set_history().
 */
class PedalWidget {
public:
    /**
     * @brief Construct a PedalWidget.
     * @param engine Reference to the audio engine.
     * @param effect Shared pointer to the effect this widget controls.
     * @param index  Position in the signal chain (used for ImGui IDs).
     */
    PedalWidget(AudioEngine& engine, std::shared_ptr<Effect> effect, int index);

    /**
     * @brief Render the pedal widget for one frame.
     * @return true if the user clicked the remove button and the pedal should be deleted.
     */
    bool render(float zoom = 1.0f);

    /** @brief Return the current chain index. */
    int get_index() const { return index_; }

    /** @brief Update the chain index (e.g. after reordering). */
    void set_index(int idx) { index_ = idx; }

    /** @brief Return the underlying effect. */
    std::shared_ptr<Effect> get_effect() const { return effect_; }

    /**
     * @brief Inject a CommandHistory for recording undo-able parameter changes.
     * @param history Pointer to the shared history (may be nullptr to disable undo).
     */
    void set_history(CommandHistory* history) { history_ = history; }

    /**
     * @brief Inject a GuiMidi pointer for MIDI Learn integration in knob popups.
     * @param gm Pointer to the shared GuiMidi module (may be nullptr to disable).
     */
    void set_gui_midi(GuiMidi* gm) { gui_midi_ = gm; }

private:
    /** @brief Render a single rotary knob (unused legacy helper). */
    void render_knob(const char* label, float* value, float min_val, float max_val,
                     float default_val, const char* format = "%.1f");

    /** @brief Render a toggle switch (unused legacy helper). */
    void render_toggle(const char* label, bool* value);

    // Render helpers for decomposing the huge render() function
    void render_amp_cabinet(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, float pedal_height, float zoom);
    void render_standard_pedal(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, bool enabled, float zoom);
    void render_tuner_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom);
    void render_looper_display(ImVec2 p0, float pedal_width, float zoom);
    void render_ir_cabinet_display(ImVec2 p0, float pedal_width, float zoom);
    void render_multiband_compressor_display(ImDrawList* dl, ImVec2 p0, float pedal_width, float zoom);
    void render_knobs(ImDrawList* dl, ImVec2 p0, float pedal_width, bool is_amp, bool is_tuner, bool is_ir_cab, float zoom);
    void render_footswitch_and_extras(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float pedal_width, float pedal_height, bool is_amp, bool enabled, bool& should_remove, float zoom);

    AudioEngine& engine_;
    std::shared_ptr<Effect> effect_;
    int index_;
    CommandHistory* history_ = nullptr;
    GuiMidi* gui_midi_ = nullptr;

    // Knob drag tracking for undo coalescing
    bool knob_was_active_ = false;
    int active_param_index_ = -1;
    float param_value_before_drag_ = 0.0f;

    // Popup slider tracking for accurate undo
    int popup_active_param_index_ = -1;
    float popup_param_value_before_edit_ = 0.0f;

    ImVec4 pedal_color_;  ///< Pedal body color derived from effect type.
    ImVec4 led_color_;    ///< LED / accent color derived from effect type.

    /** @brief Look up pedal_color_ and led_color_ from the theme table. */
    void assign_colors();

    /**
     * @brief Push a ParameterChangeCommand to the history (value already applied).
     * @param param_index Index of the changed parameter.
     * @param old_val     Value before the change.
     * @param new_val     Value after the change.
     */
    void commit_param_change(int param_index, float old_val, float new_val);
};

} // namespace Amplitron
