#include "audio/engine/audio_engine.h"
#include <cstring>
#include <algorithm>

namespace Amplitron {

void AudioEngine::set_input_gain(float gain) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetInputGain;
    cmd.value = gain;
    command_queue_.try_push(cmd);
    input_gain_.store(gain, std::memory_order_relaxed);
}

void AudioEngine::set_output_gain(float gain) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetOutputGain;
    cmd.value = gain;
    command_queue_.try_push(cmd);
    output_gain_.store(gain, std::memory_order_relaxed);
}

void AudioEngine::toggle_metronome() {
    bool enabled = !metronome_enabled_state_.load(std::memory_order_relaxed);
    metronome_enabled_state_.store(enabled, std::memory_order_relaxed);
}

void AudioEngine::set_metronome_bpm(int bpm) {
    const int clamped = std::max(40, std::min(bpm, 240));
    metronome_bpm_state_.store(clamped, std::memory_order_relaxed);
}

void AudioEngine::set_metronome_volume(float volume) {
    const float clamped = std::max(0.0f, std::min(volume, 1.0f));
    metronome_volume_state_.store(clamped, std::memory_order_relaxed);
}

void AudioEngine::push_param_change(int effect_index, int param_index, float value) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetEffectParam;
    cmd.effect_index = effect_index;
    cmd.param_index = param_index;
    cmd.value = value;
    command_queue_.try_push(cmd);
}

void AudioEngine::push_effect_enabled(int effect_index, float enabled) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetEffectEnabled;
    cmd.effect_index = effect_index;
    cmd.value = enabled;
    command_queue_.try_push(cmd);
}

void AudioEngine::push_effect_mix(int effect_index, float mix) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetEffectMix;
    cmd.effect_index = effect_index;
    cmd.value = mix;
    command_queue_.try_push(cmd);
}

void AudioEngine::push_mixer_gain_change(int node_id, int pin_index, float gain) {
    AudioCommand cmd{};
    cmd.type = AudioCommand::SetMixerGain;
    cmd.effect_index = node_id; // Overload effect_index to mean node_id
    cmd.param_index = pin_index; // Overload param_index to mean pin_index
    cmd.value = gain;
    command_queue_.try_push(cmd);
}

int AudioEngine::get_suggested_buffer_size() const {
    float load = cpu_load_.load(std::memory_order_relaxed);
    int current = buffer_size_;

    if (load > 0.80f) {
        if (current < MAX_BUFFER_SIZE) {
            return std::min(current * 2, MAX_BUFFER_SIZE);
        }
    }
    if (load < 0.30f) {
        if (current > MIN_BUFFER_SIZE) {
            return std::max(current / 2, MIN_BUFFER_SIZE);
        }
    }
    return current;
}

bool AudioEngine::copy_analyzer_snapshot(float* input_dest,
                                         float* output_dest,
                                         int sample_count) const {
    if (!input_dest || !output_dest || sample_count <= 0) {
        return false;
    }

    const int count = std::min(sample_count, ANALYZER_FFT_SIZE);
    std::lock_guard<std::mutex> lock(analyzer_mutex_);
    const uint64_t seq = analyzer_sequence_.load(std::memory_order_relaxed);
    if (seq == 0) {
        return false;
    }

    std::memcpy(input_dest, analyzer_snapshot_input_.data(), static_cast<size_t>(count) * sizeof(float));
    std::memcpy(output_dest, analyzer_snapshot_output_.data(), static_cast<size_t>(count) * sizeof(float));
    return true;
}

void AudioEngine::update_level_analyzer(float dt) {
    const float input_rms       = get_input_rms();
    const float output_rms      = get_output_rms();
    const bool  input_clipped   = consume_input_clipped();
    const bool  output_clipped  = consume_output_clipped();
    level_analyzer_.update(input_rms, output_rms, input_clipped, output_clipped, dt);
}

void AudioEngine::update_spectrum_analyzer(float dt) {
    const uint64_t seq = get_analyzer_sequence();
    if (seq == analyzer_last_sequence_) {
        spectrum_analyzer_.update(analyzer_input_buf_.data(),
                                  analyzer_output_buf_.data(),
                                  get_sample_rate(),
                                  dt);
        return;
    }

    if (copy_analyzer_snapshot(analyzer_input_buf_.data(),
                               analyzer_output_buf_.data(),
                               ANALYZER_FFT_SIZE)) {
        spectrum_analyzer_.update(analyzer_input_buf_.data(),
                                  analyzer_output_buf_.data(),
                                  get_sample_rate(),
                                  dt);
        analyzer_last_sequence_ = seq;
    }
}

} // namespace Amplitron
