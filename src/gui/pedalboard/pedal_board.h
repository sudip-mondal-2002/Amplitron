#pragma once

#include "common.h"
#include "audio/engine/audio_engine.h"
#include "gui/commands/command_history.h"
#include <set>

namespace Amplitron {

class PedalWidget;
class GuiMidi;

/**
 * @brief Visual representation of the audio effect signal chain.
 *
 * Renders a horizontal strip of PedalWidget instances, an "Add Pedal" menu,
 * and drag-and-drop reordering. All structural changes (add, remove, reorder)
 * are routed through CommandHistory for undo/redo support.
 */
class PedalBoard {
    friend class TestAccessor;
public:
    /**
     * @brief Construct the pedal board.
     * @param engine  Reference to the audio engine that owns the effect chain.
     * @param history Reference to the shared command history for undo/redo.
     * @param gui_midi Optional GuiMidi pointer for MIDI learn on initial widgets.
     */
    PedalBoard(AudioEngine& engine, CommandHistory& history, GuiMidi* gui_midi = nullptr);

    /** @brief Destructor. */
    ~PedalBoard();

    /** @brief Render the toolbar and signal chain each frame. */
    void render();

    /** @brief Recreate PedalWidget instances from the current engine effect list.
     *  Preserves visibility of effects already on the board by tracking effect pointer
     *  identity; new effects (e.g. after preset load or add) are shown only if enabled. */
    void rebuild_widgets();

    /** @brief Whether only enabled pedals are shown (default true). */
    bool show_active_only() const { return show_active_only_; }

    /** @brief Inject a GuiMidi pointer to propagate to PedalWidgets. */
    void set_gui_midi(GuiMidi* gm) { gui_midi_ = gm; }

private:
    /** @brief Render the "+ Add Pedal" button and its popup menu. */
    void render_add_pedal_menu();

    /** @brief Render the amp model selector dropdown. */
    void render_amp_selector();
    
    /** @brief Render the MIDI status and quick actions menu. */
    void render_midi_menu();

    /** @brief Render the signal flow line, pedal widgets, and drag-and-drop targets. */
    void render_signal_chain();

    /** @brief Find the index of the current AmpSimulator in the effect chain (-1 if none). */
    int find_amp_index() const;

    /** @brief Add an effect, rebuild widgets, and mark it visible. */
    void add_effect_and_show(std::shared_ptr<Effect> effect);

    AudioEngine& engine_;
    CommandHistory& history_;
    std::vector<std::unique_ptr<PedalWidget>> widgets_;
    bool show_active_only_ = true;
    std::set<int> visible_indices_; // Indices of pedals that should be visible
    GuiMidi* gui_midi_ = nullptr;
    bool show_confirm_reset_ = false;
    bool show_confirm_clear_ = false;
    bool show_confirm_midi_clear_ = false;
};

} // namespace Amplitron
