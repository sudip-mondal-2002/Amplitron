#pragma once

#include "audio/effect.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Amplitron {

/**
 * A simple in-chain looper (record / play / overdub / clear).
 *
 * Design goals:
 * - No allocations inside process/process_stereo (buffer is preallocated).
 * - Thread-safe UI visibility for state and loop position via atomics.
 * - Minimal, predictable state machine for practice workflows.
 */
class Looper : public Effect {
public:
    enum class State : uint32_t {
        Empty = 0, // no loop in memory
        Idle,      // loop exists but not playing
        Recording,
        Playing,
        Overdubbing,
    };

    Looper();

    void process(float* buffer, int num_samples) override;
    void process_stereo(float* left, float* right, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "Looper"; }
    std::vector<EffectParam>& params() override { return params_; }

    // --- UI control (thread-safe) ---
    void request_record_toggle();
    void request_play_toggle();
    void request_overdub_toggle();
    void request_clear();

    // --- UI status snapshot (thread-safe) ---
    State state() const { return static_cast<State>(ui_state_.load(std::memory_order_relaxed)); }
    bool has_loop() const { return ui_has_loop_.load(std::memory_order_relaxed) != 0; }
    int loop_length_samples() const { return ui_loop_length_samples_.load(std::memory_order_relaxed); }
    int playhead_samples() const { return ui_playhead_samples_.load(std::memory_order_relaxed); }

private:
    static constexpr int kMaxSeconds = 60;
    static constexpr float kMinLoopSeconds = 0.10f;
    static constexpr float kLoopLevelSmoothingSeconds = 0.02f;

    enum CommandBits : uint32_t {
        CmdRecordToggle  = 1u << 0,
        CmdPlayToggle    = 1u << 1,
        CmdOverdubToggle = 1u << 2,
        CmdClear         = 1u << 3,
    };

    // Params (saved in presets): loop playback level + crossfade length.
    std::vector<EffectParam> params_;

    // Preallocated buffers (full capacity, mono or stereo).
    std::vector<float> buffer_l_;
    std::vector<float> buffer_r_;
    int max_samples_ = 0;

    // Audio-thread state (not atomic; only touched in process/process_stereo).
    State state_rt_ = State::Empty;
    bool has_loop_rt_ = false;
    int record_pos_ = 0;
    int playhead_ = 0;
    int loop_length_ = 0;

    float loop_level_smoothed_ = 0.80f;
    float loop_level_alpha_ = 0.0f;

    // UI-visible atomics (written from audio thread, read by GUI thread).
    std::atomic<uint32_t> ui_state_{static_cast<uint32_t>(State::Empty)};
    std::atomic<int> ui_has_loop_{0};
    std::atomic<int> ui_loop_length_samples_{0};
    std::atomic<int> ui_playhead_samples_{0};

    // UI -> audio thread command mailbox (bitmask).
    std::atomic<uint32_t> pending_commands_{0};

    // Helpers
    void ensure_capacity();
    void apply_pending_commands();
    void publish_ui_snapshot();
    void clear_loop_rt();
    void start_recording_rt();
    void stop_recording_rt_and_play_rt();
    void toggle_play_rt();
    void toggle_overdub_rt();

    static inline float soft_clip(float x) {
        const float ax = std::fabs(x);
        return x / (1.0f + ax);
    }

    inline int crossfade_samples_rt() const;
    inline void process_core(float* left, float* right, int num_samples, bool stereo);
};

} // namespace Amplitron
