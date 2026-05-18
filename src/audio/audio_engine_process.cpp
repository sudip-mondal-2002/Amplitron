#include "audio/audio_engine.h"
#include <cstring>
#include <chrono>
#include <algorithm>

namespace Amplitron {

void AudioEngine::process_audio(const float* input, float* output, int frame_count) {
    auto t_start = std::chrono::steady_clock::now();

    if (frame_count > static_cast<int>(process_buffer_.size())) {
        process_buffer_.resize(frame_count, 0.0f);
        process_buffer_right_.resize(frame_count, 0.0f);
    }

    const bool analyzer_on = analyzer_enabled_.load(std::memory_order_relaxed);

    float in_gain = input_gain_.load(std::memory_order_relaxed);
    float peak_in = 0.0f;
    if (analyzer_on) {
        float sum_sq_in = 0.0f;
        bool clipped_in = false;
        int cap = analyzer_capture_index_;
        for (int i = 0; i < frame_count; ++i) {
            process_buffer_[i] = input[i] * in_gain;
            float abs_val = std::fabs(process_buffer_[i]);
            if (abs_val > peak_in) peak_in = abs_val;
            if (abs_val >= 1.0f) clipped_in = true;
            sum_sq_in += process_buffer_[i] * process_buffer_[i];
            analyzer_capture_input_[cap] = process_buffer_[i];
            cap = (cap + 1) & ANALYZER_FFT_MASK;
        }
        input_rms_.store(std::sqrt(sum_sq_in / std::max(frame_count, 1)), std::memory_order_relaxed);
        if (clipped_in) input_clipped_.store(true, std::memory_order_release);
        analyzer_capture_index_ = cap;
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

    drain_gain_commands();

    if (effect_mutex_.try_lock()) {
        drain_commands();
        if (topology_dirty_.exchange(false, std::memory_order_acq_rel)) {
            audio_shadow_effects_ = effects_;
            audio_shadow_tuner_   = tuner_tap_;
        }
        effect_mutex_.unlock();
    }

    if (audio_shadow_tuner_ && audio_shadow_tuner_->is_enabled()) {
        audio_shadow_tuner_->process(process_buffer_.data(), frame_count);
        std::memcpy(process_buffer_right_.data(), process_buffer_.data(),
                    static_cast<size_t>(frame_count) * sizeof(float));
    }

    // --- NEW: Per-Pedal DSP Profiling Loop ---
    int fx_index = 0;
    for (auto& fx : audio_shadow_effects_) {
        if (fx->is_enabled()) {
            auto fx_start = std::chrono::steady_clock::now();

            fx->process_stereo(process_buffer_.data(),
                               process_buffer_right_.data(), frame_count);

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

    float out_gain = output_gain_.load(std::memory_order_relaxed);
    float peak_out = 0.0f;
    if (analyzer_on) {
        float sum_sq_out = 0.0f;
        bool clipped_out = false;
        int cap = (analyzer_capture_index_ - frame_count) & ANALYZER_FFT_MASK;
        for (int i = 0; i < frame_count; ++i) {
            float out_l = clamp(process_buffer_[i]       * out_gain, -1.0f, 1.0f);
            float out_r = clamp(process_buffer_right_[i] * out_gain, -1.0f, 1.0f);
            if (std::fabs(out_l) >= 1.0f || std::fabs(out_r) >= 1.0f) clipped_out = true;
            output[i * 2]     = out_l;
            output[i * 2 + 1] = out_r;
            process_buffer_[i] = out_l;
            float abs_val = std::fabs(out_l);
            if (abs_val > peak_out) peak_out = abs_val;
            sum_sq_out += out_l * out_l;
            analyzer_capture_output_[cap] = out_l;
            cap = (cap + 1) & ANALYZER_FFT_MASK;
        }
        output_rms_.store(std::sqrt(sum_sq_out / std::max(frame_count, 1)), std::memory_order_relaxed);
        if (clipped_out) output_clipped_.store(true, std::memory_order_release);

        analyzer_samples_since_publish_ += frame_count;
        if (analyzer_samples_since_publish_ >= ANALYZER_HOP_SIZE) {
            if (analyzer_mutex_.try_lock()) {
                const int start = analyzer_capture_index_;
                const int first_chunk = ANALYZER_FFT_SIZE - start;
                std::memcpy(analyzer_snapshot_input_.data(),
                            analyzer_capture_input_.data() + start,
                            static_cast<size_t>(first_chunk) * sizeof(float));
                std::memcpy(analyzer_snapshot_input_.data() + first_chunk,
                            analyzer_capture_input_.data(),
                            static_cast<size_t>(start) * sizeof(float));
                std::memcpy(analyzer_snapshot_output_.data(),
                            analyzer_capture_output_.data() + start,
                            static_cast<size_t>(first_chunk) * sizeof(float));
                std::memcpy(analyzer_snapshot_output_.data() + first_chunk,
                            analyzer_capture_output_.data(),
                            static_cast<size_t>(start) * sizeof(float));
                analyzer_sequence_.fetch_add(1, std::memory_order_release);
                analyzer_samples_since_publish_ = 0;
                analyzer_mutex_.unlock();
            }
        }
    } else {
        for (int i = 0; i < frame_count; ++i) {
            float out_l = clamp(process_buffer_[i]       * out_gain, -1.0f, 1.0f);
            float out_r = clamp(process_buffer_right_[i] * out_gain, -1.0f, 1.0f);
            output[i * 2]     = out_l;
            output[i * 2 + 1] = out_r;
            process_buffer_[i] = out_l;
            float abs_val = std::fabs(out_l);
            if (abs_val > peak_out) peak_out = abs_val;
        }
    }
    output_level_.store(peak_out);

    if (recorder_.is_recording()) {
        recorder_.write_samples(process_buffer_.data(), frame_count);
    }

    auto t_end = std::chrono::steady_clock::now();
    float duration_us = std::chrono::duration<float, std::micro>(t_end - t_start).count();
    callback_duration_us_.store(duration_us, std::memory_order_relaxed);
    float budget_us = (static_cast<float>(frame_count) / sample_rate_) * 1e6f;
    cpu_load_.store(duration_us / budget_us, std::memory_order_relaxed);
}

void AudioEngine::drain_gain_commands() {
    AudioCommand cmd;
    while (command_queue_.try_peek(cmd)) {
        if (cmd.type == AudioCommand::SetInputGain) {
            command_queue_.try_pop(cmd);
            input_gain_.store(cmd.value, std::memory_order_relaxed);
        } else if (cmd.type == AudioCommand::SetOutputGain) {
            command_queue_.try_pop(cmd);
            output_gain_.store(cmd.value, std::memory_order_relaxed);
        } else {
            break;
        }
    }
}

void AudioEngine::drain_commands() {
    AudioCommand cmd;
    while (command_queue_.try_pop(cmd)) {
        switch (cmd.type) {
            case AudioCommand::SetEffectParam:
                if (cmd.effect_index >= 0 &&
                    cmd.effect_index < static_cast<int>(effects_.size())) {
                    auto& params = effects_[cmd.effect_index]->params();
                    if (cmd.param_index >= 0 &&
                        cmd.param_index < static_cast<int>(params.size())) {
                        params[cmd.param_index].value = cmd.value;
                    }
                }
                break;
            case AudioCommand::SetEffectEnabled:
                if (cmd.effect_index >= 0 &&
                    cmd.effect_index < static_cast<int>(effects_.size())) {
                    effects_[cmd.effect_index]->set_enabled(cmd.value > 0.5f);
                }
                break;
            case AudioCommand::SetEffectMix:
                if (cmd.effect_index >= 0 &&
                    cmd.effect_index < static_cast<int>(effects_.size())) {
                    effects_[cmd.effect_index]->set_mix(cmd.value);
                }
                break;
            case AudioCommand::SetInputGain:
                input_gain_.store(cmd.value, std::memory_order_relaxed);
                break;
            case AudioCommand::SetOutputGain:
                output_gain_.store(cmd.value, std::memory_order_relaxed);
                break;
            default:
                break;
        }
    }
}

} // namespace Amplitron