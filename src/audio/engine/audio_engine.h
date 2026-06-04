#pragma once

#include "common.h"
#include "audio/engine/i_audio_engine.h"
#include "audio/effects/core/effect.h"
#include "audio/recorder/recorder.h"
#include "audio/recorder/i_recorder.h"
#include "audio/engine/audio_command_dispatcher.h"
#include "audio/dsp/level_analyzer.h"
#include "audio/dsp/spectrum_analyzer.h"
#include "audio/engine/tempo_engine.h"
#include <chrono>

#include "audio/engine/audio_graph.h"
#include "audio/engine/audio_graph_executor.h"
#include <memory>

#include <nlohmann/json.hpp>
#include "audio/backend/audio_backend.h"
#include "audio/engine/metronome.h"
#include "audio/engine/i_metronome.h"

namespace Amplitron {

class AnalyzerCapture;

/**
 * @brief Core audio processing engine.
 *
 * Manages the audio stream (via a platform backend), the effect chain,
 * master gain controls, CPU load monitoring, and a lock-free SPSC command
 * queue for thread-safe GUI-to-audio parameter updates.
 *
 * All platform-specific code (PortAudio / SDL) lives in separate
 * compilation units; the engine itself is platform-agnostic.
 *
 * @note Backend Precedence and Ownership:
 * - Precedence: poly_backend_ (if non-null) takes operational precedence over the native backend_.
 * - Ownership: AudioEngine owns the opaque backend_ pointer (manages its creation/destruction).
 *   However, AudioEngine does NOT take ownership of the polymorphic poly_backend_ pointer;
 *   the caller or test fixture is responsible for managing its lifetime.
 */
class AudioEngine : public IAudioEngine {
public:
    friend class PortAudioTestSaboteur;
    
    /** @brief Construct the engine with default settings. */
    AudioEngine(std::unique_ptr<IRecorder> recorder = nullptr,
                std::unique_ptr<IMetronome> metronome = nullptr);

    /** @brief Destructor — shuts down the audio stream if still running. */
    ~AudioEngine() override;

    void commit_graph_changes() override;

    nlohmann::json serialize() override;
    void deserialize(const nlohmann::json& j) override;

    /** @brief Initialize the audio back-end. @return true on success. */
    bool initialize() override;

    /** @brief Release audio back-end resources. */
    void shutdown() override;

    /** @brief Open and start the audio stream. @return true on success. */
    bool start() override;

    /** @brief Stop the audio stream. */
    void stop() override;

    /** @brief Stop and restart the stream (manual recovery). @return true on success. */
    bool restart() override;

    /** @brief Return the last error message, or empty string. */
    std::string get_last_error() const override { return last_error_; }

    /** @brief Clear the stored error message. */
    void clear_error() override { last_error_.clear(); }

#ifdef AMPLITRON_ANDROID_OBOE
    /**
     * @brief Return a human-readable label for the Oboe sharing mode negotiated at runtime.
     * "AAudio exclusive mode" when AAudio exclusive path is active; "OpenSL ES (shared)" otherwise.
     * Used by the Android settings UI to display the actual backend, not a hardcoded string.
     */
    const char* get_oboe_sharing_mode_label() const override;
#endif

    /** @brief Enumerate available audio input devices. */
    std::vector<AudioDeviceInfo> get_input_devices() const override;

    /** @brief Enumerate available audio output devices. */
    std::vector<AudioDeviceInfo> get_output_devices() const override;

    /**
     * @brief Select the input device by index.
     * @return true if the device was set successfully.
     */
    bool set_input_device(int device_index) override;

    /**
     * @brief Select the output device by index.
     * @return true if the device was set successfully.
     */
    bool set_output_device(int device_index) override;

    /** @brief Return the current input device index (delegates to backend so auto-detection is reflected). */
    int get_input_device() const override { return backend_ ? backend_->get_input_device() : -1; }

    /** @brief Return the current output device index (delegates to backend so auto-detection is reflected). */
    int get_output_device() const override { return backend_ ? backend_->get_output_device() : -1; }

    /** @brief Return the human-readable input device name. */
    std::string get_input_device_name() const override;

    /** @brief Return the human-readable output device name. */
    std::string get_output_device_name() const override;

    /** @brief Direct access to the effect chain vector (GUI thread only). */
    AudioGraph& graph() override { return main_graph_; }
    const AudioGraph& graph() const override { return main_graph_; }

#ifdef AMPLITRON_TESTS
    /** @brief Replace the platform backend in tests. */
    void replace_backend_state_for_test(std::unique_ptr<IAudioBackend> backend);
    void replace_backend_for_test(IAudioBackend* backend);
    void clear_backend_for_test();
#endif

    // =========================================================================
    // Compatibility bridge: flat effects_ vector for Undo/Redo/Snapshot systems
    // while the DAG-based AudioGraph is being migrated.
    // =========================================================================
    std::vector<std::shared_ptr<Effect>> dummy_effects_;
    std::vector<std::shared_ptr<Effect>>& effects() override { return dummy_effects_; }

    void add_effect(std::shared_ptr<Effect> fx) override;
    void add_initial_effects(const std::vector<std::shared_ptr<Effect>>& fxs) override {
        dummy_effects_.clear();
        for (const auto& fx : fxs)
            dummy_effects_.push_back(fx);
        sync_graph_with_dummy_effects(true);
    }
    void insert_effect(int index, std::shared_ptr<Effect> fx) override;
    void remove_effect(int index) override;
    void clear_effects() override;
    void move_effect(int from, int to) override;
    void restore_effects_state(std::vector<std::shared_ptr<Effect>> state) override;


    /**
     * @brief Set the audio buffer size (takes effect on next stream restart).
     * @param size Buffer size in samples.
     */
    void set_buffer_size(int size) override;

    /**
     * @brief Set the audio sample rate (takes effect on next stream restart).
     * @param rate Sample rate in Hz.
     */
    void set_sample_rate(int rate) override;

    /** @brief Return the current buffer size in samples. */
    int get_buffer_size() const override { return buffer_size_; }

    /** @brief Return the current sample rate in Hz. */
    int get_sample_rate() const override { return sample_rate_; }

    /** @brief Return true if the audio stream is actively running. */
    bool is_running() const override { return running_; }

    /** @brief Test-only helper to bypass hardware startup constraints */
    void set_running_for_testing(bool running) { running_ = running; }

    /** @brief Return the most recent input peak level (0.0–1.0, atomic). */
    float get_input_level() const override { return input_level_.load(); }

    /** @brief Return the most recent output peak level (0.0–1.0, atomic). */
    float get_output_level() const override { return output_level_.load(); }

    /** @brief Return the most recent input RMS level (0.0–1.0, atomic). */
    float get_input_rms() const override { return input_rms_.load(std::memory_order_relaxed); }

    /** @brief Return the most recent output RMS level (0.0–1.0, atomic). */
    float get_output_rms() const override { return output_rms_.load(std::memory_order_relaxed); }

    /** @brief Consume one-shot input clipping flag set by audio thread. */
    bool consume_input_clipped() override { return input_clipped_.exchange(false, std::memory_order_acq_rel); }

    /** @brief Consume one-shot output clipping flag set by audio thread. */
    bool consume_output_clipped() override { return output_clipped_.exchange(false, std::memory_order_acq_rel); }

    /** @brief Enable/disable analyzer capture in the audio callback (GUI thread). */
    void set_analyzer_enabled(bool enabled) override;

    /** @brief Return true if analyzer capture is active. */
    bool is_analyzer_enabled() const override;

    /** @brief Snapshot sequence counter; increments when new analyzer data is published. */
    uint64_t get_analyzer_sequence() const override;

    /**
     * @brief Copy latest pre/post-chain analyzer snapshots (GUI thread).
     * @param input_dest  Destination buffer for pre-chain samples.
     * @param output_dest Destination buffer for post-chain samples.
     * @param sample_count Number of samples to copy (clamped to ANALYZER_FFT_SIZE).
     * @return true if at least one snapshot has been published.
     */
    bool copy_analyzer_snapshot(float* input_dest, float* output_dest, int sample_count) const override;



    /**
     * @brief Set the master input gain (enqueued to audio thread via SPSC queue).
     * @param gain Linear gain multiplier.
     */
    void set_input_gain(float gain) override;

    /**
     * @brief Set the master output gain (enqueued to audio thread via SPSC queue).
     * @param gain Linear gain multiplier.
     */
    void set_output_gain(float gain) override;

    
    /** @brief Return the current input gain (atomic relaxed read). */
    float get_input_gain() const override { return input_gain_.load(std::memory_order_relaxed); }

    /** @brief Return the current output gain (atomic relaxed read). */
    float get_output_gain() const override { return output_gain_.load(std::memory_order_relaxed); }

    /** @brief Toggle the metronome on/off (atomic update). */
    void toggle_metronome() override;

    /** @brief Set the metronome BPM (atomic update). */
    void set_metronome_bpm(int bpm) override;

    /** @brief Set the global BPM (atomic update). */
    void set_global_bpm(float bpm) override;

    /** @brief Return the current global BPM (atomic relaxed read). */
    float get_global_bpm() const override { return global_bpm_.load(std::memory_order_relaxed); }

    /** @brief Access the built-in tempo engine. */
    TempoEngine& tempo_engine() override { return tempo_engine_; }
    const TempoEngine& tempo_engine() const override { return tempo_engine_; }

    /** @brief Set the metronome click volume (atomic update). */
    void set_metronome_volume(float volume) override;

    /** @brief Return the current metronome enabled state (atomic relaxed read). */
    bool get_metronome_enabled() const override { return metronome_->is_enabled(); }

    /** @brief Return the current metronome BPM (atomic relaxed read). */
    int get_metronome_bpm() const override { return metronome_->get_bpm(); }

    /** @brief Return the current metronome volume (atomic relaxed read). */
    float get_metronome_volume() const override { return metronome_->get_volume(); }

    /**
     * @brief Enqueue a parameter value change from the GUI thread (lock-free).
     * @param effect_index Index of the effect in the chain.
     * @param param_index  Index of the parameter within the effect.
     * @param value        New parameter value.
     */
    void push_param_change(int effect_index, int param_index, float value) override;

    /**
     * @brief Enqueue a mixer input gain change from the GUI thread.
     * @param node_id      ID of the Mixer node.
     * @param pin_index    Index of the input pin on the mixer.
     * @param gain         New gain multiplier (0.0–2.0).
     */
    void push_mixer_gain_change(int node_id, int pin_index, float gain) override;

    /**
     * @brief Enqueue an effect enabled/disabled change from the GUI thread.
     * @param effect_index Index of the effect in the chain.
     * @param enabled      >0.5 means enabled.
     */
    void push_effect_enabled(int effect_index, float enabled) override;

    /**
     * @brief Enqueue a dry/wet mix change from the GUI thread.
     * @param effect_index Index of the effect in the chain.
     * @param mix          New mix value (0.0–1.0).
     */
    void push_effect_mix(int effect_index, float mix) override;

    /** @brief Return the current CPU load fraction (0.0–1.0, atomic). */
    float get_cpu_load() const override { return cpu_load_.load(std::memory_order_relaxed); }

    /** @brief Suggest a new buffer size based on current CPU load. */
    int get_suggested_buffer_size() const override;

    /** @brief Return true if automatic buffer-size tuning is enabled. */
    bool is_auto_buffer_enabled() const override { return auto_buffer_enabled_; }

    /** @brief Enable or disable automatic buffer-size tuning. */
    void set_auto_buffer_enabled(bool enabled) override { auto_buffer_enabled_ = enabled; }

    /** @brief Access the built-in audio recorder. */
    IRecorder& recorder() override { return *recorder_; }

    /**
     * @brief Set a tuner tap that receives pre-chain audio each callback.
     *
     * The tap is processed before the effect chain. If its mute param is
     * active it will zero the buffer, silencing the downstream chain.
     * Protected by effect_mutex_.
     */
    void set_tuner_tap(std::shared_ptr<Effect> tap) override;

    /** @brief Remove the tuner tap. */
    void clear_tuner_tap() override;

    /** @brief Return true if a tuner tap is currently installed. */
    bool has_tuner_tap() const override;

    /**
     * @brief Run the DSP pipeline on a block of audio samples.
     *
     * Called by the platform backend's audio callback. Public so that
     * backend compilation units (which are not class members) can invoke it.
     */
    void process_audio(const float* input, float* output, int frame_count) override;

    // MIDI instance is managed by the GUI thread's MidiManager.

private:
    // Platform backend state
    std::unique_ptr<IAudioBackend> backend_;

    bool initialized_ = false;
    bool running_ = false;

    int input_device_ = -1;
    int output_device_ = -1;
    int sample_rate_ = DEFAULT_SAMPLE_RATE;
    int buffer_size_ = DEFAULT_BUFFER_SIZE;
    //global transport
    std::atomic<float> input_gain_{1.0f};
    std::atomic<float> output_gain_{0.8f};
    std::unique_ptr<IMetronome> metronome_;
    std::atomic<float> global_bpm_{120.0f};
    TempoEngine tempo_engine_;

    std::atomic<float> input_level_{0.0f};
    std::atomic<float> output_level_{0.0f};
    std::atomic<float> input_rms_{0.0f};
    std::atomic<float> output_rms_{0.0f};
    std::atomic<bool> input_clipped_{false};
    std::atomic<bool> output_clipped_{false};


    // std::vector<std::shared_ptr<Effect>> effects_;
    std::vector<float>     process_buffer_;
    std::vector<float> process_buffer_right_;
    std::mutex effect_mutex_;
    std::unique_ptr<IRecorder> recorder_;
    std::shared_ptr<Effect> tuner_tap_;
    std::string last_error_;

    std::shared_ptr<Effect>      audio_shadow_tuner_;
    std::atomic<bool>            topology_dirty_{true};

    // The main graph data model (Edited by the GUI/Main thread)
    AudioGraph main_graph_;
    
    // The compiled executor (Built by the GUI thread)
    std::shared_ptr<AudioGraphExecutor> main_executor_;
    
    // The shadow executor (Safely copied by the Audio thread)
    std::shared_ptr<AudioGraphExecutor> audio_shadow_executor_;

    void sync_graph_with_dummy_effects(bool reset_graph = false);

    AudioCommandDispatcher command_dispatcher_;

    // CPU load watchdog for buffer auto-tuning
    std::atomic<float> cpu_load_{0.0f};
    std::atomic<float> callback_duration_us_{0.0f};
    bool auto_buffer_enabled_ = false;

    std::unique_ptr<AnalyzerCapture> analyzer_capture_;



    // (MIDI instance removed - use MidiManager)
};

} // namespace Amplitron
