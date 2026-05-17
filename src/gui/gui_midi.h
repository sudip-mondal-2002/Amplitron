#pragma once

#include <string>
#include <vector>

namespace Amplitron {

class MidiManager;

#ifdef AMPLITRON_NO_MIDI

// Stub implementation for non-desktop platforms
class GuiMidi {
public:
    explicit GuiMidi(MidiManager&) {}
    void render(bool&) {}
    bool render_learn_menu_item(const std::string&, const std::string&) { return false; }
    bool render_learn_bypass_item(const std::string&) { return false; }
};

#else

/**
 * @brief GUI module for MIDI settings window and MIDI Learn integration.
 *
 * Renders a floating settings window (port selection, mapping table,
 * learn status indicator) and provides menu items for knob right-click
 * popups to enable MIDI Learn.
 */
class GuiMidi {
public:
    explicit GuiMidi(MidiManager& midi);

    /** @brief Render the MIDI settings floating window. */
    void render(bool& show);

    /**
     * @brief Render a "MIDI Learn" item inside a knob's right-click popup.
     * @return true if learn was activated (caller should close the popup).
     */
    bool render_learn_menu_item(const std::string& effect_name,
                                const std::string& param_name);

    /**
     * @brief Render a "MIDI Learn (Bypass)" item for effect bypass toggle.
     * @return true if learn was activated.
     */
    bool render_learn_bypass_item(const std::string& effect_name);

    const MidiManager& midi() const { return midi_; }

private:
    MidiManager& midi_;
    std::vector<std::string> cached_ports_;
};

#endif // AMPLITRON_NO_MIDI

} // namespace Amplitron
