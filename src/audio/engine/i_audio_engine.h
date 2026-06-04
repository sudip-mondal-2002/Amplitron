#pragma once

#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "audio/backend/audio_device_info.h"

namespace Amplitron {

class Effect;
class AudioGraph;
class IRecorder;
class TempoEngine;

/**
 * @brief Interface for managing lifecycle operations.
 * @note Threading Contract: Call only from the GUI/Main thread.
 */
class ILifecycle {
public:
    virtual ~ILifecycle() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool restart() = 0;
    virtual std::string get_last_error() const = 0;
    virtual void clear_error() = 0;
};

/**
 * @brief Abstract interface for audio device, latency, and sample rate management.
 * Satisfies the Interface Segregation Principle (ISP) and Dependency Inversion Principle (DIP).
 * @note Threading Contract: Call only from the GUI/Main thread.
 */
class IDeviceManager {
public:
    virtual ~IDeviceManager() = default;

    virtual std::vector<AudioDeviceInfo> get_input_devices() const = 0;
    virtual std::vector<AudioDeviceInfo> get_output_devices() const = 0;

    virtual bool set_input_device(int device_index) = 0;
    virtual bool set_output_device(int device_index) = 0;

    virtual int get_input_device() const = 0;
    virtual int get_output_device() const = 0;

    virtual std::string get_input_device_name() const = 0;
    virtual std::string get_output_device_name() const = 0;

    virtual int get_buffer_size() const = 0;
    virtual int get_sample_rate() const = 0;

    virtual void set_buffer_size(int size) = 0;
    virtual void set_sample_rate(int rate) = 0;

    virtual bool is_running() const = 0;
};

/**
 * @brief Abstract interface for audio level, RMS, clipping, and CPU load metrics.
 * Satisfies the Interface Segregation Principle (ISP) and Dependency Inversion Principle (DIP).
 * @note Threading Contract: Safe to call from the GUI/Main thread. Reads atomic variables updated by the audio thread.
 */
class IAudioMetricsService {
public:
    virtual ~IAudioMetricsService() = default;

    virtual float get_input_level() const = 0;
    virtual float get_output_level() const = 0;

    virtual float get_input_rms() const = 0;
    virtual float get_output_rms() const = 0;

    virtual bool consume_input_clipped() = 0;
    virtual bool consume_output_clipped() = 0;

    virtual float get_cpu_load() const = 0;
};

/**
 * @brief Interface for inspecting and mutating the effect chain.
 * @note Threading Contract: Call only from the GUI/Main thread. Modifies graph structure.
 */
class IEffectChain {
public:
    virtual ~IEffectChain() = default;
    virtual std::vector<std::shared_ptr<Effect>>& effects() = 0;
    virtual void add_effect(std::shared_ptr<Effect> fx) = 0;
    virtual void add_initial_effects(const std::vector<std::shared_ptr<Effect>>& fxs) = 0;
    virtual void insert_effect(int index, std::shared_ptr<Effect> fx) = 0;
    virtual void remove_effect(int index) = 0;
    virtual void clear_effects() = 0;
    virtual void move_effect(int from, int to) = 0;
    virtual void restore_effects_state(std::vector<std::shared_ptr<Effect>> state) = 0;
};

/**
 * @brief Interface for transport settings, master gains, and metronome controls.
 * @note Threading Contract: Call only from the GUI/Main thread.
 */
class ITransportControl {
public:
    virtual ~ITransportControl() = default;
    virtual void set_input_gain(float gain) = 0;
    virtual void set_output_gain(float gain) = 0;
    virtual float get_input_gain() const = 0;
    virtual float get_output_gain() const = 0;

    virtual void toggle_metronome() = 0;
    virtual void set_metronome_bpm(int bpm) = 0;
    virtual void set_metronome_volume(float volume) = 0;
    virtual bool get_metronome_enabled() const = 0;
    virtual int get_metronome_bpm() const = 0;
    virtual float get_metronome_volume() const = 0;

    virtual void set_global_bpm(float bpm) = 0;
    virtual float get_global_bpm() const = 0;
};

/**
 * @brief Interface for driving real-time audio block processing.
 * @note Threading Contract: Called exclusively from the high-priority real-time audio callback thread.
 *       Must be lock-free and memory-allocation-free.
 */
class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;
    virtual void process_audio(const float* input, float* output, int frame_count) = 0;
};

/**
 * @brief Interface for configuring and retrieving FFT analyzer snapshots.
 * @note Threading Contract: Safe to call from the GUI/Main thread. Uses locks to copy shared buffers.
 */
class IAnalyzerProvider {
public:
    static constexpr int ANALYZER_FFT_SIZE = 2048;

    virtual ~IAnalyzerProvider() = default;
    virtual void set_analyzer_enabled(bool enabled) = 0;
    virtual bool is_analyzer_enabled() const = 0;
    virtual uint64_t get_analyzer_sequence() const = 0;
    virtual bool copy_analyzer_snapshot(float* input_dest, float* output_dest, int sample_count) const = 0;
};

/**
 * @brief Interface for pushing thread-safe real-time parameter changes.
 * @note Threading Contract: Safe to call from any thread (non-blocking lock-free SPSC queue).
 */
class IParameterDispatch {
public:
    virtual ~IParameterDispatch() = default;
    virtual void push_param_change(int effect_index, int param_index, float value) = 0;
    virtual void push_mixer_gain_change(int node_id, int pin_index, float gain) = 0;
    virtual void push_effect_enabled(int effect_index, float enabled) = 0;
    virtual void push_effect_mix(int effect_index, float mix) = 0;
};

/**
 * @brief Interface for serialization and deserialization of the session state.
 * @note Threading Contract: Call only from the GUI/Main thread.
 */
class ISessionSerializer {
public:
    virtual ~ISessionSerializer() = default;
    virtual nlohmann::json serialize() = 0;
    virtual void deserialize(const nlohmann::json& j) = 0;
};

/**
 * @brief Interface for accessing and committing topological audio graph changes.
 * @note Threading Contract: Call only from the GUI/Main thread.
 */
class IGraphProvider {
public:
    virtual ~IGraphProvider() = default;
    virtual AudioGraph& graph() = 0;
    virtual const AudioGraph& graph() const = 0;
    virtual void commit_graph_changes() = 0;
};

/**
 * @brief Unified interface for the audio processing engine.
 * Satisfies the Dependency Inversion Principle (DIP).
 */
class IAudioEngine : public ILifecycle,
                     public IDeviceManager,
                     public IAudioMetricsService,
                     public IEffectChain,
                     public ITransportControl,
                     public IAudioProcessor,
                     public IAnalyzerProvider,
                     public IParameterDispatch,
                     public ISessionSerializer,
                     public IGraphProvider {
public:
    virtual ~IAudioEngine() = default;

    virtual int get_suggested_buffer_size() const = 0;
    virtual bool is_auto_buffer_enabled() const = 0;
    virtual void set_auto_buffer_enabled(bool enabled) = 0;

    virtual TempoEngine& tempo_engine() = 0;
    virtual const TempoEngine& tempo_engine() const = 0;
    virtual IRecorder& recorder() = 0;
    virtual void set_tuner_tap(std::shared_ptr<Effect> tap) = 0;
    virtual void clear_tuner_tap() = 0;
    virtual bool has_tuner_tap() const = 0;

#ifdef AMPLITRON_ANDROID_OBOE
    virtual const char* get_oboe_sharing_mode_label() const = 0;
#endif
};

} // namespace Amplitron

