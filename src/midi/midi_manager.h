#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifndef AMPLITRON_NO_MIDI
#include "audio/spsc_queue.h"
#endif

namespace Amplitron {

class AudioEngine;

// Raw MIDI event pushed from the RtMidi callback thread.
// Must be trivially copyable for SPSCQueue.
struct MidiEvent {
    uint8_t status;  // e.g. 0xB0 = CC on channel 0
    uint8_t data1;   // CC number (0-127)
    uint8_t data2;   // CC value (0-127)
    uint8_t pad = 0; // Pad to 4 bytes
};

enum class MidiMappingMode : uint8_t {
    Continuous, // CC 0-127 maps linearly to param [min..max]
    Toggle,     // CC >= 64 -> on, CC < 64 -> off
};

enum class MidiTargetType : uint8_t {
    EffectParam,  // Maps to a specific effect parameter
    EffectBypass, // Maps to effect enabled/disabled
    InputGain,    // Maps to master input gain
    OutputGain,   // Maps to master output gain
};

struct MidiMapping {
    int cc_number = 0;    // 0-127
    int midi_channel = -1; // 0-15, or -1 for "any channel"
    MidiTargetType target_type = MidiTargetType::EffectParam;
    MidiMappingMode mode = MidiMappingMode::Continuous;
    std::string effect_name; // For EffectParam/EffectBypass targets
    std::string param_name;  // For EffectParam targets only
};

#ifdef AMPLITRON_NO_MIDI

// Stub implementation for non-desktop platforms (web, mobile)
class MidiManager {
public:
    MidiManager() = default;
    ~MidiManager() = default;

    bool initialize() { return false; }
    void shutdown() {}

    std::vector<std::string> get_available_ports() const { return {}; }
    bool open_port(int) { return false; }
    void close_port() {}
    int current_port() const { return -1; }
    std::string current_port_name() const { return ""; }
    bool is_port_open() const { return false; }

    void add_mapping(const MidiMapping&) {}
    void remove_mapping(int) {}
    void remove_mapping_for_param(const std::string&, const std::string&) {}
    void clear_mappings() {}
    const std::vector<MidiMapping>& mappings() const {
        static std::vector<MidiMapping> empty;
        return empty;
    }

    void install_default_mappings() {}

    void start_learn(MidiTargetType, const std::string&, const std::string&) {}
    void cancel_learn() {}
    bool is_learning() const { return false; }
    std::string learn_status() const { return ""; }
    const std::string& learn_effect_name() const { static std::string empty; return empty; }
    const std::string& learn_param_name() const { static std::string empty; return empty; }

    void poll(AudioEngine&) {}
    void save_config() const {}
    void load_config() {}

    void inject_event(const MidiEvent&) {}
};

#else

/**
 * @brief MIDI input manager with CC-to-parameter mapping and MIDI learn.
 *
 * Runs a lock-free SPSC queue between RtMidi's callback thread and the
 * GUI thread. The GUI thread calls poll() each frame to drain events and
 * route CC values through the existing engine.push_param_change() path.
 */
class MidiManager {
public:
    MidiManager();
    ~MidiManager();

    /** @brief Open the first available MIDI input port. @return true on success. */
    bool initialize();

    /** @brief Close the MIDI port and release resources. */
    void shutdown();

    // --- Port management ---

    /** @brief List available MIDI input port names. */
    std::vector<std::string> get_available_ports() const;

    /** @brief Open a specific MIDI input port by index. @return true on success. */
    bool open_port(int port_index);

    /** @brief Close the currently open port. */
    void close_port();

    /** @brief Return the index of the currently open port, or -1 if none. */
    int current_port() const { return current_port_; }

    /** @brief Return the name of the currently open port, or empty string. */
    std::string current_port_name() const { return current_port_name_; }

    /** @brief Return true if a MIDI port is currently open. */
    bool is_port_open() const { return current_port_ >= 0; }

    // --- Mapping management ---

    void add_mapping(const MidiMapping& mapping);
    void remove_mapping(int index);
    void remove_mapping_for_param(const std::string& effect_name, const std::string& param_name);
    void clear_mappings();
    const std::vector<MidiMapping>& mappings() const { return mappings_; }

    /** @brief Install default CC mappings (CC7, CC11, CC64, CC74). */
    void install_default_mappings();

    // --- MIDI Learn ---

    /**
     * @brief Enter learn mode: the next CC event received will be bound to the given target.
     */
    void start_learn(MidiTargetType type, const std::string& effect_name, const std::string& param_name);

    /** @brief Cancel learn mode without creating a mapping. */
    void cancel_learn();

    /** @brief Return true if learn mode is active. */
    bool is_learning() const { return learn_active_; }

    /** @brief Human-readable status for the learn indicator, or empty. */
    std::string learn_status() const;

    const std::string& learn_effect_name() const { return learn_effect_name_; }
    const std::string& learn_param_name() const { return learn_param_name_; }

    // --- Poll (called from GUI thread each frame) ---

    /**
     * @brief Drain the MIDI event queue and apply CC mappings.
     *
     * For each CC event:
     * - If learn mode is active, captures the CC and creates a mapping.
     * - Otherwise, resolves the mapping target and pushes the value to the engine.
     */
    void poll(AudioEngine& engine);

    // --- Persistence ---

    /** @brief Save mappings and port preference to midi_config.json. */
    void save_config() const;

    /** @brief Load mappings and port preference from midi_config.json. */
    void load_config();

    /**
     * @brief Push a MIDI event into the queue from test code.
     *
     * This is public so unit tests can inject events without hardware.
     */
    void inject_event(const MidiEvent& event);

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
    void apply_mapping(const MidiMapping& mapping, int cc_value, AudioEngine& engine);
    std::string mappings_to_json() const;
    bool mappings_from_json(const std::string& json);
};

#endif // AMPLITRON_NO_MIDI

} // namespace Amplitron
