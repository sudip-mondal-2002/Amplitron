#pragma once

#include "audio/audio_engine.h"
#include "gui/command_history.h"
#include "preset_manager.h"
#include <string>
#include <vector>

namespace Amplitron {

class PedalBoard;

/**
 * @brief Handles preset save/load/delete UI and logic.
 * Extracted from GuiManager for single-responsibility.
 */
class GuiPresets {
public:
    /**
     * @brief Construct a GuiPresets helper for the given audio engine and undo history.
     * @param engine Reference to the engine used for preset state capture.
     * @param history Shared command history for undo/redo integration.
     */
    GuiPresets(AudioEngine& engine, CommandHistory& history);

    /** @brief Render save preset popup. Only call when show is true. */
    void render_save_popup(bool& show);

    /** @brief Render load preset popup. Only call when show is true. */
    void render_load_popup(bool& show);

    /** @brief Initialize dialog for "New Preset" (clears fields). */
    void begin_new_preset();

    /** @brief Initialize dialog for "Save Preset" (preserves current name). */
    void begin_save_preset();

    /** @brief Set the pedal board pointer (for rebuild_widgets calls). */
    void set_pedal_board(PedalBoard* pb) { pedal_board_ = pb; }

    /** @brief Set the MidiManager pointer (for saving/loading midi mappings). */
    void set_midi_manager(MidiManager* m) { midi_manager_ = m; }

    // Preset management methods
    void refresh_presets(bool preserve_selection = true);
    bool save_named_preset(const std::string& preset_name,
                           const std::string& description);
    bool load_preset_by_index(int index);
    bool delete_preset_by_index(int index);
    void ensure_factory_presets();

    // Accessors for menu bar integration
    int selected_preset_index() const { return selected_preset_index_; }
    int preset_count() const { return static_cast<int>(preset_files_.size()); }
    const std::string& status_message() const { return preset_status_msg_; }
    void set_status_message(const std::string& msg) { preset_status_msg_ = msg; }

    /** @brief Return true if the current engine state differs from the last saved preset. */
    bool is_dirty() const;

    /** @brief Return the current preset name or "Untitled" if unset. */
    std::string current_preset_name() const;

    /** @brief Serialise the current engine state to a JSON string for clipboard export. */
    std::string serialise_current_preset_to_json() const;

    /** @brief Record the current engine state as the clean saved preset state. */
    void mark_clean();

private:
    std::string preset_name_from_path(const std::string& filepath) const;
    std::string preset_path_from_name(const std::string& preset_name) const;

    AudioEngine& engine_;
    CommandHistory& history_;
    PresetData saved_state_;
    bool saved_state_valid_ = false;
    PedalBoard* pedal_board_ = nullptr;
    MidiManager* midi_manager_ = nullptr;

    // Preset UI state
    char preset_name_buf_[128] = "My Preset";
    char preset_desc_buf_[256] = "";
    std::vector<std::string> preset_files_;
    int selected_preset_index_ = -1;
    bool factory_presets_initialized_ = false;
    bool preset_dialog_is_new_ = false;
    std::string preset_status_msg_;
};

} // namespace Amplitron
