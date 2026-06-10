#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Amplitron {

class IAudioEngine;

// Raw MIDI event pushed from the RtMidi callback thread.
struct MidiEvent {
    uint8_t status;   // e.g. 0xB0 = CC on channel 0
    uint8_t data1;    // CC number (0-127)
    uint8_t data2;    // CC value (0-127)
    uint8_t pad = 0;  // Pad to 4 bytes
};

enum class MidiMappingMode : uint8_t {
    Continuous,  // CC 0-127 maps linearly to param [min..max]
    Toggle,      // CC >= 64 -> on, CC < 64 -> off
};

enum class MidiTargetType : uint8_t {
    EffectParam,   // Maps to a specific effect parameter
    EffectBypass,  // Maps to effect enabled/disabled
    InputGain,     // Maps to master input gain
    OutputGain,    // Maps to master output gain
};

struct MidiMapping {
    int cc_number = 0;      // 0-127
    int midi_channel = -1;  // 0-15, or -1 for "any channel"
    MidiTargetType target_type = MidiTargetType::EffectParam;
    MidiMappingMode mode = MidiMappingMode::Continuous;

    std::string effect_name;          // For EffectParam/EffectBypass targets
    std::string param_name;           // For EffectParam targets only
    mutable bool last_state = false;  // Tracks pedal state for Toggle mode
};

/**
 * @brief Abstract interface for MIDI port management.
 * Satisfies the Interface Segregation Principle (ISP).
 */
class IMidiPortManager {
   public:
    virtual ~IMidiPortManager() = default;

    virtual std::vector<std::string> get_available_ports() const = 0;
    virtual bool open_port(int port_index) = 0;
    virtual void close_port() = 0;
    virtual int current_port() const = 0;
    virtual std::string current_port_name() const = 0;
    virtual bool is_port_open() const = 0;
};

/**
 * @brief Abstract interface for managing MIDI CC mapping.
 * Satisfies the Interface Segregation Principle (ISP).
 */
class IMidiMappingService {
   public:
    virtual ~IMidiMappingService() = default;

    virtual void add_mapping(const MidiMapping& mapping) = 0;
    virtual void remove_mapping(int index) = 0;
    virtual void remove_mapping_for_param(const std::string& effect_name,
                                          const std::string& param_name) = 0;
    virtual void clear_mappings() = 0;
    virtual const std::vector<MidiMapping>& mappings() const = 0;
    virtual void install_default_mappings() = 0;
};

/**
 * @brief Abstract interface for managing interactive MIDI learning.
 * Satisfies the Interface Segregation Principle (ISP).
 */
class IMidiLearnSession {
   public:
    virtual ~IMidiLearnSession() = default;

    virtual void start_learn(MidiTargetType type, const std::string& effect_name,
                             const std::string& param_name) = 0;
    virtual void cancel_learn() = 0;
    virtual bool is_learning() const = 0;
    virtual std::string learn_status() const = 0;
    virtual const std::string& learn_effect_name() const = 0;
    virtual const std::string& learn_param_name() const = 0;
};

/**
 * @brief Abstract interface for persistence of MIDI mappings.
 * Satisfies the Interface Segregation Principle (ISP).
 */
class IMidiConfigStore {
   public:
    virtual ~IMidiConfigStore() = default;

    virtual void save_config() const = 0;
    virtual void load_config() = 0;
};

/**
 * @brief Abstract interface for the MIDI input manager.
 * Satisfies the Dependency Inversion Principle (DIP) and Interface Segregation Principle (ISP).
 */
class IMidiManager : public IMidiPortManager,
                     public IMidiMappingService,
                     public IMidiLearnSession,
                     public IMidiConfigStore {
   public:
    virtual ~IMidiManager() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual void poll(IAudioEngine& engine) = 0;
    virtual void inject_event(const MidiEvent& event) = 0;
};

}  // namespace Amplitron
