#include "audio/engine/audio_engine.h"
#include "audio/engine/analyzer_capture.h"
#include <cstring>
#include <chrono>
#include <algorithm>
#include <cmath>

#include <cassert>

namespace Amplitron {



void AudioEngine::process_audio(const float* input, float* output, int frame_count) {
    auto t_start = std::chrono::steady_clock::now();

    if (frame_count > static_cast<int>(process_buffer_.size())) {
        process_buffer_.resize(frame_count, 0.0f);
        process_buffer_right_.resize(frame_count, 0.0f);
    }

    const bool analyzer_on = analyzer_capture_->is_analyzer_enabled();

    float in_gain = input_gain_.load(std::memory_order_relaxed);
    float peak_in = 0.0f;
    if (analyzer_on) {
        float sum_sq_in = 0.0f;
        bool clipped_in = false;
        for (int i = 0; i < frame_count; ++i) {
            process_buffer_[i] = input[i] * in_gain;
            float abs_val = std::fabs(process_buffer_[i]);
            if (abs_val > peak_in) peak_in = abs_val;
            if (abs_val >= 1.0f) clipped_in = true;
            sum_sq_in += process_buffer_[i] * process_buffer_[i];
        }
        input_rms_.store(std::sqrt(sum_sq_in / std::max(frame_count, 1)), std::memory_order_relaxed);
        if (clipped_in) input_clipped_.store(true, std::memory_order_release);
        analyzer_capture_->capture_input(process_buffer_.data(), frame_count);
    } else {
        for (int i = 0; i < frame_count; ++i) {
            process_buffer_[i] = input[i] * in_gain;
            float abs_val = std::fabs(process_buffer_[i]);
            if (abs_val > peak_in) peak_in = abs_val;
        }
    }
    input_level_.store(peak_in);

    std::memcpy(process_buffer_right_.data(), process_buffer_.data(),
                static_cast<size_t>(frame_count) * sizeof(float));

    command_dispatcher_.drain_gain_commands(input_gain_, output_gain_, audio_shadow_executor_);



    if (effect_mutex_.try_lock()) {
        command_dispatcher_.drain_commands(input_gain_, output_gain_, audio_shadow_executor_, main_graph_, dummy_effects_);
        if (topology_dirty_.exchange(false, std::memory_order_acq_rel)) {
            audio_shadow_executor_ = main_executor_;
            audio_shadow_tuner_   = tuner_tap_;
        }
        effect_mutex_.unlock();
    }

    if (audio_shadow_tuner_ && audio_shadow_tuner_->is_enabled()) {
        audio_shadow_tuner_->process(process_buffer_.data(), frame_count);
        std::memcpy(process_buffer_right_.data(), process_buffer_.data(),
                    static_cast<size_t>(frame_count) * sizeof(float));
    }
    // The executor handles all the looping, routing, and processing internally!
    if (audio_shadow_executor_) {
        // Broadcast tempo/bpm
        audio_shadow_executor_->update_transport_state(static_cast<float>(metronome_->get_bpm()));
        
        // Pass your mono/stereo buffers to the executor we built
        audio_shadow_executor_->process(process_buffer_.data(), process_buffer_right_.data(), frame_count);
        std::memcpy(process_buffer_.data(), process_buffer_right_.data(),
                    static_cast<size_t>(frame_count) * sizeof(float));
    }

    float out_gain = output_gain_.load(std::memory_order_relaxed);
    float peak_out = 0.0f;
    auto next_metronome_sample = [this]() -> float {
        return metronome_->next_sample();
    };
    if (analyzer_on) {
        float sum_sq_out = 0.0f;
        bool clipped_out = false;
        for (int i = 0; i < frame_count; ++i) {
            float click = next_metronome_sample();
            float out_l = clamp(process_buffer_[i]       * out_gain + click, -1.0f, 1.0f);
            float out_r = clamp(process_buffer_right_[i] * out_gain + click, -1.0f, 1.0f);
            if (std::fabs(out_l) >= 1.0f || std::fabs(out_r) >= 1.0f) clipped_out = true;
            output[i * 2]     = out_l;
            output[i * 2 + 1] = out_r;
            process_buffer_[i] = out_l;
            float abs_val = std::fabs(out_l);
            if (abs_val > peak_out) peak_out = abs_val;
            sum_sq_out += out_l * out_l;
        }
        output_rms_.store(std::sqrt(sum_sq_out / std::max(frame_count, 1)), std::memory_order_relaxed);
        if (clipped_out) output_clipped_.store(true, std::memory_order_release);
        analyzer_capture_->capture_output(process_buffer_.data(), frame_count);
    } else {
        for (int i = 0; i < frame_count; ++i) {
            float click = next_metronome_sample();
            float out_l = clamp(process_buffer_[i]       * out_gain + click, -1.0f, 1.0f);
            float out_r = clamp(process_buffer_right_[i] * out_gain + click, -1.0f, 1.0f);
            output[i * 2]     = out_l;
            output[i * 2 + 1] = out_r;
            process_buffer_[i] = out_l;
            float abs_val = std::fabs(out_l);
            if (abs_val > peak_out) peak_out = abs_val;
        }
    }
    output_level_.store(peak_out);

    if (recorder_->is_recording()) {
        recorder_->write_samples_stereo(process_buffer_.data(),
                                       process_buffer_right_.data(),
                                       frame_count);
    }

    auto t_end = std::chrono::steady_clock::now();
    float duration_us = std::chrono::duration<float, std::micro>(t_end - t_start).count();
    callback_duration_us_.store(duration_us, std::memory_order_relaxed);
    float budget_us = (static_cast<float>(frame_count) / sample_rate_) * 1e6f;
    cpu_load_.store(duration_us / budget_us, std::memory_order_relaxed);
}


} // namespace Amplitron
