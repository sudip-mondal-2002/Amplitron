#include "audio/engine/audio_engine.h"
#include "audio/engine/analyzer_capture.h"
#include <cstring>
#include <algorithm>

namespace Amplitron {

void AudioEngine::set_input_gain(float gain) {
    command_dispatcher_.push_input_gain(gain);
    input_gain_.store(gain, std::memory_order_relaxed);
}

void AudioEngine::set_output_gain(float gain) {
    command_dispatcher_.push_output_gain(gain);
    output_gain_.store(gain, std::memory_order_relaxed);
}

void AudioEngine::toggle_metronome() {
    metronome_->toggle();
}

void AudioEngine::set_metronome_bpm(int bpm) {
    metronome_->set_bpm(bpm);
}

void AudioEngine::set_metronome_volume(float volume) {
    metronome_->set_volume(volume);
}

void AudioEngine::push_param_change(int effect_index, int param_index, float value) {
    command_dispatcher_.push_param_change(effect_index, param_index, value);
}

void AudioEngine::push_effect_enabled(int effect_index, float enabled) {
    command_dispatcher_.push_effect_enabled(effect_index, enabled);
}

void AudioEngine::push_effect_mix(int effect_index, float mix) {
    command_dispatcher_.push_effect_mix(effect_index, mix);
}

void AudioEngine::push_mixer_gain_change(int node_id, int pin_index, float gain) {
    command_dispatcher_.push_mixer_gain_change(node_id, pin_index, gain);
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

void AudioEngine::set_analyzer_enabled(bool enabled) {
    analyzer_capture_->set_analyzer_enabled(enabled);
}

bool AudioEngine::is_analyzer_enabled() const {
    return analyzer_capture_->is_analyzer_enabled();
}

uint64_t AudioEngine::get_analyzer_sequence() const {
    return analyzer_capture_->get_analyzer_sequence();
}

bool AudioEngine::copy_analyzer_snapshot(float* input_dest,
                                         float* output_dest,
                                         int sample_count) const {
    return analyzer_capture_->copy_analyzer_snapshot(input_dest, output_dest, sample_count);
}


} // namespace Amplitron
