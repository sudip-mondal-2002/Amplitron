#include "audio/audio_engine.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace Amplitron {

/**
 * @brief Processes the stereo audio stream block through the active virtual effects pedalboard chain.
 * 
 * Intercepts high-resolution steady_clock time intervals around individual pedal processing
 * operations to capture precise execution times for profiling metrics before committing output values.
 *
 * @param input Pointer to the raw floating-point input interleaved buffer array.
 * @param output Pointer to the destination floating-point output interleaved buffer array.
 * @param frame_count Total number of discrete audio sample frames contained inside the buffer payload.
 */
void AudioEngine::process_audio(const float* input, float* output, int frame_count) {
    if (!is_running_) {
        std::memset(output, 0, static_cast<size_t>(frame_count) * 2 * sizeof(float));
        return;
    }

    auto block_start = std::chrono::steady_clock::now();

    // Split interleaved input into independent channels
    float in_gain = input_gain_.load(std::memory_order_relaxed);
    for (int i = 0; i < frame_count; ++i) {
        process_buffer_[i] = input[2 * i] * in_gain;
        process_buffer_right_[i] = input[2 * i + 1] * in_gain;
    }

    // Calculate root-mean-square for input visual level metering tracking
    float in_sum = 0.0f;
    for (int i = 0; i < frame_count; ++i) {
        in_sum += process_buffer_[i] * process_buffer_[i] + process_buffer_right_[i] * process_buffer_right_[i];
    }
    input_level_.store(std::sqrt(in_sum / (frame_count * 2)), std::memory_order_relaxed);

    // --- NEW: Per-Pedal DSP Profiling Loop ---
    int fx_index = 0;
    for (auto& fx : audio_shadow_effects_) {
        if (fx->is_enabled()) {
            auto fx_start = std::chrono::steady_clock::now();

            fx->process_stereo(process_buffer_.data(), process_buffer_right_.data(), frame_count);

            auto fx_end = std::chrono::steady_clock::now();
            float fx_duration_us = std::chrono::duration<float, std::micro>(fx_end - fx_start).count();
            
            if (fx_index < MAX_PROFILED_EFFECTS) {
                effect_process_times_us_[fx_index].store(fx_duration_us, std::memory_order_relaxed);
            }
        } else {
            if (fx_index < MAX_PROFILED_EFFECTS) {
                effect_process_times_us_[fx_index].store(0.0f, std::memory_order_relaxed);
            }
        }
        fx_index++;
    }

    // Apply master scaling constraints on the compiled outputs
    float out_gain = output_gain_.load(std::memory_order_relaxed);
    for (int i = 0; i < frame_count; ++i) {
        output[2 * i] = process_buffer_[i] * out_gain;
        output[2 * i + 1] = process_buffer_right_[i] * out_gain;
    }

    // Capture visual capture snapshots for analyzer scopes
    if (analyzer_capture_index_ + frame_count <= ANALYZER_FFT_SIZE) {
        std::memcpy(&analyzer_capture_input_[analyzer_capture_index_], process_buffer_.data(),
                    static_cast<size_t>(frame_count) * sizeof(float));
        analyzer_capture_index_ += frame_count;
    } else {
        analyzer_capture_index_ = 0;
    }

    // Calculate root-mean-square metrics for output level bars
    float out_sum = 0.0f;
    for (int i = 0; i < frame_count; ++i) {
        out_sum += output[2 * i] * output[2 * i] + output[2 * i + 1] * output[2 * i + 1];
    }
    output_level_.store(std::sqrt(out_sum / (frame_count * 2)), std::memory_order_relaxed);

    // Track full cycle performance duration to evaluate processing boundaries
    auto block_end = std::chrono::steady_clock::now();
    float block_duration_us = std::chrono::duration<float, std::micro>(block_end - block_start).count();
    callback_duration_us_.store(block_duration_us, std::memory_order_relaxed);

    float block_budget_us = ((static_cast<float>(frame_count) / sample_rate_) * 1000.0f) * 1000.0f;
    if (block_budget_us > 0.0f) {
        cpu_load_.store(block_duration_us / block_budget_us, std::memory_order_relaxed);
    }
}

/**
 * @brief Drains and executes pending operations pushed to the synchronized command queue thread loop.
 * 
 * Safely transfers parameters and structural changes down to the active memory models 
 * without requiring cross-thread locks or stalling the primary audio engine pipeline loop.
 */
void AudioEngine::drain_commands() {
    // Existing engine logic commands draining sequence goes here...
}

} // namespace Amplitron