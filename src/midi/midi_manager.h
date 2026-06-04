#pragma once
#include "audio/utils/spsc_queue.h"
#include "midi/i_midi_manager.h"

namespace Amplitron {

#ifdef AMPLITRON_NO_MIDI

// Stub implementation for non-desktop platforms (web, mobile)
class MidiManager : public IMidiManager {
    friend class TestAccessor;
public:
    MidiManager() = default;
    ~MidiManager() = default;

    bool initialize() override { return false; }
    void shutdown() override {}

    std::vector<std::string> get_available_ports() const override { return {}; }
    bool open_port(int) override { return false; }
    void close_port() override {}
    int current_port() const override { return -1; }
    std::string current_port_name() const override { return ""; }
    bool is_port_open() const override { return false; }

    void add_mapping(const MidiMapping&) override {}
    void remove_mapping(int) override {}
    void remove_mapping_for_param(const std::string&, const std::string&) override {}
    void clear_mappings() override {}
    const std::vector<MidiMapping>& mappings() const override {
        static std::vector<MidiMapping> empty;
        return empty;
    }

    void install_default_mappings() override {}

    void start_learn(MidiTargetType, const std::string&, const std::string&) override {}
    void cancel_learn() override {}
    bool is_learning() const override { return false; }
    std::string learn_status() const override { return ""; }
    const std::string& learn_effect_name() const override { static std::string empty; return empty; }
    const std::string& learn_param_name() const override { static std::string empty; return empty; }

    void poll(IAudioEngine&) override {}
    void save_config() const override {}
    void load_config() override {}

    void inject_event(const MidiEvent&) override {}
};

#else

/**
 * @brief MIDI input manager with CC-to-parameter mapping and MIDI learn.
 *
 * Runs a lock-free SPSC queue between RtMidi's callback thread and the
 * GUI thread. The GUI thread calls poll() each frame to drain events and
 * route CC values through the existing engine.push_param_change() path.
 */
class MidiManager : public IMidiManager {
    friend class TestAccessor;
public:
    MidiManager();
    ~MidiManager();

    /** @brief Open the first available MIDI input port. @return true on success. */
    bool initialize() override;

    /** @brief Close the MIDI port and release resources. */
    void shutdown() override;

    // --- Port management ---

    /** @brief List available MIDI input port names. */
    std::vector<std::string> get_available_ports() const override;

    /** @brief Open a specific MIDI input port by index. @return true on success. */
    bool open_port(int port_index) override;

    /** @brief Close the currently open port. */
    void close_port() override;

    /** @brief Return the index of the currently open port, or -1 if none. */
    int current_port() const override { return current_port_; }

    /** @brief Return the name of the currently open port, or empty string. */
    std::string current_port_name() const override { return current_port_name_; }

    /** @brief Return true if a MIDI port is currently open. */
    bool is_port_open() const override { return current_port_ >= 0; }

    // --- Mapping management ---

    void add_mapping(const MidiMapping& mapping) override;
    void remove_mapping(int index) override;
    void remove_mapping_for_param(const std::string& effect_name, const std::string& param_name) override;
    void clear_mappings() override;
    const std::vector<MidiMapping>& mappings() const override { return mappings_; }

    /** @brief Install default CC mappings (CC7, CC11, CC64, CC74). */
    void install_default_mappings() override;

    // --- MIDI Learn ---

    /**
     * @brief Enter learn mode: the next CC event received will be bound to the given target.
     */
    void start_learn(MidiTargetType type, const std::string& effect_name, const std::string& param_name) override;

    /** @brief Cancel learn mode without creating a mapping. */
    void cancel_learn() override;

    /** @brief Return true if learn mode is active. */
    bool is_learning() const override { return learn_active_; }

    /** @brief Human-readable status for the learn indicator, or empty. */
    std::string learn_status() const override;

    const std::string& learn_effect_name() const override { return learn_effect_name_; }
    const std::string& learn_param_name() const override { return learn_param_name_; }

    // --- Poll (called from GUI thread each frame) ---

    /**
     * @brief Drain the MIDI event queue and apply CC mappings.
     *
     * For each CC event:
     * - If learn mode is active, captures the CC and creates a mapping.
     * - Otherwise, resolves the mapping target and pushes the value to the engine.
     */
    void poll(IAudioEngine& engine) override;

    // --- Persistence ---

    /** @brief Save mappings and port preference to midi_config.json. */
    void save_config() const override;

    /** @brief Load mappings and port preference from midi_config.json. */
    void load_config() override;

    /**
     * @brief Push a MIDI event into the queue from test code.
     *
     * This is public so unit tests can inject events without hardware.
     */
    void inject_event(const MidiEvent& event) override;

private:
    static void midi_callback(double timestamp, std::vector<unsigned char>* message, void* user_data);

    static std::string get_config_path();

    void* midi_in_ = nullptr; // RtMidiIn* (opaque to avoid header dependency)
    int current_port_ = -1;
    std::string current_port_name_;

    SPSCQueue<MidiEvent, 256> midi_queue_;
    std::vector<MidiMapping> mappings_;

    // Learn state
    bool learn_active_ = false;
    MidiTargetType learn_target_type_ = MidiTargetType::EffectParam;
    std::string learn_effect_name_;
    std::string learn_param_name_;

    // Helpers
    void apply_mapping(const MidiMapping& mapping, int cc_value, IAudioEngine& engine);
    std::string mappings_to_json() const;
    bool mappings_from_json(const std::string& json);
};

#endif // AMPLITRON_NO_MIDI

} // namespace Amplitron
