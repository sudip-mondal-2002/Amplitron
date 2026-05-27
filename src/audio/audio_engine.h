#pragma once

#include "common.h"
#include "audio/effect.h"
#include "audio/recorder.h"
#include "audio/spsc_queue.h"
#include <chrono>

#include "audio/audio_graph.h"
#include "audio/audio_graph_executor.h"
#include <memory>

#include <nlohmann/json.hpp>
// FORWARD DECLARATIONS
namespace Amplitron {

struct AudioDeviceInfo {
    int index;
    std::string name;
    int max_input_channels;
    int max_output_channels;
    double default_sample_rate;
    bool is_usb_device;
};

struct AudioBackendState;

/**
 * @brief Core audio processing engine.
 *
 * Manages the audio stream (via a platform backend), the effect chain,
 * master gain controls, CPU load monitoring, and a lock-free SPSC command
 * queue for thread-safe GUI-to-audio parameter updates.
 *
 * All platform-specific code (PortAudio / SDL) lives in separate
 * compilation units; the engine itself is platform-agnostic.
 */
class AudioEngine {
public:
    friend class PortAudioTestSaboteur;
    
    /** @brief Construct the engine with default settings. */
    AudioEngine();

    /** @brief Destructor — shuts down the audio stream if still running. */
    ~AudioEngine();

    void commit_graph_changes();

    /** @brief serialize and deserialize method signatures to AudioEngine class definition */
    
    nlohmann::json serialize();
    void deserialize(const nlohmann::json& j);
    /** @brief Initialize the audio back-end. @return true on success. */
    bool initialize();

    /** @brief Release audio back-end resources. */
    void shutdown();

    /** @brief Open and start the audio stream. @return true on success. */
    bool start();

    /** @brief Stop the audio stream. */
    void stop();

    /** @brief Stop and restart the stream (manual recovery). @return true on success. */
    bool restart();

    /** @brief Return the last error message, or empty string. */
    std::string get_last_error() const { return last_error_; }

    /** @brief Clear the stored error message. */
    void clear_error() { last_error_.clear(); }

#ifdef AMPLITRON_ANDROID_OBOE
    /**
     * @brief Return a human-readable label for the Oboe sharing mode negotiated at runtime.
     * "AAudio exclusive mode" when AAudio exclusive path is active; "OpenSL ES (shared)" otherwise.
     * Used by the Android settings UI to display the actual backend, not a hardcoded string.
     */
    const char* get_oboe_sharing_mode_label() const;
#endif

    /** @brief Enumerate available audio input devices. */
    std::vector<AudioDeviceInfo> get_input_devices() const;

    /** @brief Enumerate available audio output devices. */
    std::vector<AudioDeviceInfo> get_output_devices() const;

    /**
     * @brief Select the input device by index.
     * @return true if the device was set successfully.
     */
    bool set_input_device(int device_index);

    /**
     * @brief Select the output device by index.
     * @return true if the device was set successfully.
     */
    bool set_output_device(int device_index);

    /** @brief Return the current input device index. */
    int get_input_device() const { return input_device_; }

    /** @brief Return the current output device index. */
    int get_output_device() const { return output_device_; }

    /** @brief Return the human-readable input device name. */
    std::string get_input_device_name() const;

    /** @brief Return the human-readable output device name. */
    std::string get_output_device_name() const;



    /** @brief Direct access to the effect chain vector (GUI thread only). */
    AudioGraph& graph() { return main_graph_; }
    const AudioGraph& graph() const { return main_graph_; }

    // =========================================================================
    // TEMPORARY COMPILER BRIDGE 
    // (Keeps the Undo/Redo & Snapshot systems quiet while we build the DAG UI)
    // =========================================================================
    std::vector<std::shared_ptr<Effect>> dummy_effects_;
    std::vector<std::shared_ptr<Effect>>& effects() { return dummy_effects_; }
    // void remove_effect(int index) { 
    //     if (index >= 0 && index < static_cast<int>(dummy_effects_.size())) {
    //         dummy_effects_.erase(dummy_effects_.begin() + index);
    //     }
    // }

    void add_effect(std::shared_ptr<Effect> fx);
    void add_initial_effects(const std::vector<std::shared_ptr<Effect>>& fxs) {
        dummy_effects_.clear();
        for (const auto& fx : fxs) {
            dummy_effects_.push_back(fx);
        }
        sync_graph_with_dummy_effects(true);
    }
    void insert_effect(int index, std::shared_ptr<Effect> fx);
    void remove_effect(int index);
    void clear_effects();
    void move_effect(int from, int to);
    void restore_effects_state(std::vector<std::shared_ptr<Effect>> state);


    /**
     * @brief Set the audio buffer size (takes effect on next stream restart).
     * @param size Buffer size in samples.
     */
    void set_buffer_size(int size);

    /**
     * @brief Set the audio sample rate (takes effect on next stream restart).
     * @param rate Sample rate in Hz.
     */
    void set_sample_rate(int rate);

    /** @brief Return the current buffer size in samples. */
    int get_buffer_size() const { return buffer_size_; }

    /** @brief Return the current sample rate in Hz. */
    int get_sample_rate() const { return sample_rate_; }

    /** @brief Return true if the audio stream is actively running. */
    bool is_running() const { return running_; }

    /** @brief Return the most recent input peak level (0.0–1.0, atomic). */
    float get_input_level() const { return input_level_.load(); }

    /** @brief Return the most recent output peak level (0.0–1.0, atomic). */
    float get_output_level() const { return output_level_.load(); }

    /** @brief Return the most recent input RMS level (0.0–1.0, atomic). */
    float get_input_rms() const { return input_rms_.load(std::memory_order_relaxed); }

    /** @brief Return the most recent output RMS level (0.0–1.0, atomic). */
    float get_output_rms() const { return output_rms_.load(std::memory_order_relaxed); }

    /** @brief Consume one-shot input clipping flag set by audio thread. */
    bool consume_input_clipped() { return input_clipped_.exchange(false, std::memory_order_acq_rel); }

    /** @brief Consume one-shot output clipping flag set by audio thread. */
    bool consume_output_clipped() { return output_clipped_.exchange(false, std::memory_order_acq_rel); }

    /** @brief FFT size used for GUI analyzer snapshots. */
    static constexpr int ANALYZER_FFT_SIZE = 2048;
    static constexpr int ANALYZER_FFT_MASK = ANALYZER_FFT_SIZE - 1;

    /** @brief Enable/disable analyzer capture in the audio callback (GUI thread). */
    void set_analyzer_enabled(bool enabled) { analyzer_enabled_.store(enabled, std::memory_order_release); }

    /** @brief Return true if analyzer capture is active. */
    bool is_analyzer_enabled() const { return analyzer_enabled_.load(std::memory_order_acquire); }

    /** @brief Snapshot sequence counter; increments when new analyzer data is published. */
    uint64_t get_analyzer_sequence() const {
        return analyzer_sequence_.load(std::memory_order_acquire);
    }

    /**
     * @brief Copy latest pre/post-chain analyzer snapshots (GUI thread).
     * @param input_dest  Destination buffer for pre-chain samples.
     * @param output_dest Destination buffer for post-chain samples.
     * @param sample_count Number of samples to copy (clamped to ANALYZER_FFT_SIZE).
     * @return true if at least one snapshot has been published.
     */
    bool copy_analyzer_snapshot(float* input_dest, float* output_dest, int sample_count) const;

    /**
     * @brief Set the master input gain (enqueued to audio thread via SPSC queue).
     * @param gain Linear gain multiplier.
     */
    void set_input_gain(float gain);

    /**
     * @brief Set the master output gain (enqueued to audio thread via SPSC queue).
     * @param gain Linear gain multiplier.
     */
    void set_output_gain(float gain);

    
    /** @brief Return the current input gain (atomic relaxed read). */
    float get_input_gain() const { return input_gain_.load(std::memory_order_relaxed); }

    /** @brief Return the current output gain (atomic relaxed read). */
    float get_output_gain() const { return output_gain_.load(std::memory_order_relaxed); }

    /** @brief Toggle the metronome on/off (atomic update). */
    void toggle_metronome();

    /** @brief Set the metronome BPM (atomic update). */
    void set_metronome_bpm(int bpm);

    /** @brief Set the metronome click volume (atomic update). */
    void set_metronome_volume(float volume);

    /** @brief Return the current metronome enabled state (atomic relaxed read). */
    bool get_metronome_enabled() const { return metronome_enabled_state_.load(std::memory_order_relaxed); }

    /** @brief Return the current metronome BPM (atomic relaxed read). */
    int get_metronome_bpm() const { return metronome_bpm_state_.load(std::memory_order_relaxed); }

    /** @brief Return the current metronome volume (atomic relaxed read). */
    float get_metronome_volume() const { return metronome_volume_state_.load(std::memory_order_relaxed); }

    /**
     * @brief Enqueue a parameter value change from the GUI thread (lock-free).
     * @param effect_index Index of the effect in the chain.
     * @param param_index  Index of the parameter within the effect.
     * @param value        New parameter value.
     */
    void push_param_change(int effect_index, int param_index, float value);

    /**
     * @brief Enqueue an effect enabled/disabled change from the GUI thread.
     * @param effect_index Index of the effect in the chain.
     * @param enabled      >0.5 means enabled.
     */
    void push_effect_enabled(int effect_index, float enabled);

    /**
     * @brief Enqueue a dry/wet mix change from the GUI thread.
     * @param effect_index Index of the effect in the chain.
     * @param mix          New mix value (0.0–1.0).
     */
    void push_effect_mix(int effect_index, float mix);

    /** @brief Return the current CPU load fraction (0.0–1.0, atomic). */
    float get_cpu_load() const { return cpu_load_.load(std::memory_order_relaxed); }

    /** @brief Suggest a new buffer size based on current CPU load. */
    int get_suggested_buffer_size() const;

    /** @brief Return true if automatic buffer-size tuning is enabled. */
    bool is_auto_buffer_enabled() const { return auto_buffer_enabled_; }

    /** @brief Enable or disable automatic buffer-size tuning. */
    void set_auto_buffer_enabled(bool enabled) { auto_buffer_enabled_ = enabled; }

    /** @brief Access the built-in audio recorder. */
    Recorder& recorder() { return recorder_; }

    /**
     * @brief Set a tuner tap that receives pre-chain audio each callback.
     *
     * The tap is processed before the effect chain. If its mute param is
     * active it will zero the buffer, silencing the downstream chain.
     * Protected by effect_mutex_.
     */
    void set_tuner_tap(std::shared_ptr<Effect> tap);

    /** @brief Remove the tuner tap. */
    void clear_tuner_tap();

    /** @brief Return true if a tuner tap is currently installed. */
    bool has_tuner_tap() const;

    /**
     * @brief Run the DSP pipeline on a block of audio samples.
     *
     * Called by the platform backend's audio callback. Public so that
     * backend compilation units (which are not class members) can invoke it.
     */
    void process_audio(const float* input, float* output, int frame_count);

    // MIDI instance is managed by the GUI thread's MidiManager.

private:
    // Platform backend state (defined in the backend .cpp that is compiled)
    AudioBackendState* backend_ = nullptr;

    bool initialized_ = false;
    bool running_ = false;

    int input_device_ = -1;
    int output_device_ = -1;
    int sample_rate_ = DEFAULT_SAMPLE_RATE;
    int buffer_size_ = DEFAULT_BUFFER_SIZE;
    //global transport
    std::atomic<float> input_gain_{1.0f};
    std::atomic<float> output_gain_{0.8f};
    std::atomic<bool> metronome_enabled_state_{false};
    std::atomic<int> metronome_bpm_state_{120};
    std::atomic<float> metronome_volume_state_{0.5f};

    std::atomic<float> input_level_{0.0f};
    std::atomic<float> output_level_{0.0f};
    std::atomic<float> input_rms_{0.0f};
    std::atomic<float> output_rms_{0.0f};
    std::atomic<bool> input_clipped_{false};
    std::atomic<bool> output_clipped_{false};
    std::atomic<bool> analyzer_enabled_{false};

    // std::vector<std::shared_ptr<Effect>> effects_;
    std::vector<float> process_buffer_;
    std::vector<float> process_buffer_right_;
    std::mutex effect_mutex_;
    Recorder recorder_;
    std::shared_ptr<Effect> tuner_tap_;
    std::string last_error_;

    // Audio-thread-private shadow of the effect chain.
    // Copied from effects_ / tuner_tap_ whenever effect_mutex_ is acquired
    // and topology_dirty_ is set, avoiding unnecessary shared_ptr churn on
    // every callback when the chain is stable.
    // std::vector<std::shared_ptr<Effect>> audio_shadow_effects_;
    std::shared_ptr<Effect> audio_shadow_tuner_;
    std::atomic<bool> topology_dirty_{true};

    // The main graph data model (Edited by the GUI/Main thread)
    AudioGraph main_graph_;
    
    // The compiled executor (Built by the GUI thread)
    std::shared_ptr<AudioGraphExecutor> main_executor_;
    
    // The shadow executor (Safely copied by the Audio thread)
    std::shared_ptr<AudioGraphExecutor> audio_shadow_executor_;

    void sync_graph_with_dummy_effects(bool reset_graph = false);

    // Lock-free GUI -> Audio command queue (256 slots)
    SPSCQueue<AudioCommand, 256> command_queue_;
    void drain_commands();        // Must be called while holding effect_mutex_
    void drain_gain_commands();   // Safe to call without effect_mutex_
    void update_metronome_timing();

    // CPU load watchdog for buffer auto-tuning
    std::atomic<float> cpu_load_{0.0f};
    std::atomic<float> callback_duration_us_{0.0f};
    bool auto_buffer_enabled_ = false;

    // Audio-thread capture for GUI analyzer snapshots.
    static constexpr int ANALYZER_HOP_SIZE = 1024;
    std::array<float, ANALYZER_FFT_SIZE> analyzer_capture_input_{};
    std::array<float, ANALYZER_FFT_SIZE> analyzer_capture_output_{};
    int analyzer_capture_index_ = 0;
    int analyzer_samples_since_publish_ = 0;

    // Shared snapshot buffers (audio thread writes with try_lock, GUI reads with lock).
    mutable std::mutex analyzer_mutex_;
    std::array<float, ANALYZER_FFT_SIZE> analyzer_snapshot_input_{};
    std::array<float, ANALYZER_FFT_SIZE> analyzer_snapshot_output_{};
    std::atomic<uint64_t> analyzer_sequence_{0};

    // Metronome state (audio thread only)
    bool metronome_enabled_ = false;
    int metronome_bpm_ = 120;
    float metronome_volume_ = 0.5f;

    float metronome_volume_smoothed_ = 0.0f;
    float metronome_volume_smooth_alpha_ = 0.05f;
    float metronome_bpm_smoothed_ = 120.0f;
    float metronome_bpm_smooth_alpha_ = 0.05f;

    int metronome_sample_rate_ = 0;
    double metronome_samples_per_beat_ = 0.0;
    double metronome_sample_counter_ = 0.0;
    int metronome_click_samples_total_ = 0;
    int metronome_click_samples_remaining_ = 0;
    float metronome_click_phase_ = 0.0f;
    float metronome_click_phase_inc_ = 0.0f;
    float metronome_click_env_ = 0.0f;
    float metronome_click_decay_ = 0.0f;
    // (MIDI instance removed - use MidiManager)
};

} // namespace Amplitron
