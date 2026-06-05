#include "audio/engine/metronome.h"

#include <algorithm>
#include <cmath>

namespace Amplitron {

static constexpr float kTwoPi = 6.28318530718f;

Metronome::Metronome() { update_timing(); }

void Metronome::set_enabled(bool enabled) { enabled_.store(enabled, std::memory_order_relaxed); }

bool Metronome::is_enabled() const { return enabled_.load(std::memory_order_relaxed); }

void Metronome::toggle() {
    bool current = enabled_.load(std::memory_order_relaxed);
    while (!enabled_.compare_exchange_weak(current, !current, std::memory_order_relaxed,
                                           std::memory_order_relaxed)) {
    }
}

void Metronome::set_bpm(int bpm) {
    bpm_.store(std::max(40, std::min(bpm, 240)), std::memory_order_relaxed);
}

int Metronome::get_bpm() const { return bpm_.load(std::memory_order_relaxed); }

void Metronome::set_volume(float volume) {
    volume_.store(clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
}

float Metronome::get_volume() const { return volume_.load(std::memory_order_relaxed); }

void Metronome::set_sample_rate(int sample_rate) {
    sample_rate_.store(sample_rate, std::memory_order_relaxed);
    update_timing();
}

void Metronome::reset() {
    metronome_sample_counter_ = 0.0;
    metronome_click_samples_remaining_ = 0;
    metronome_click_env_ = 0.0f;
    metronome_click_phase_ = 0.0f;
    update_timing();
}

void Metronome::update_timing() {
    const int bpm = bpm_.load(std::memory_order_relaxed);
    const int sample_rate = sample_rate_.load(std::memory_order_relaxed);
    if (sample_rate <= 0) {
        metronome_samples_per_beat_ = 0.0;
        metronome_click_phase_inc_ = 0.0f;
        metronome_click_samples_total_ = 0;
        metronome_click_decay_ = 0.0f;
        return;
    }

    metronome_samples_per_beat_ =
        (static_cast<double>(sample_rate) * 60.0) / static_cast<double>(bpm);
    if (metronome_samples_per_beat_ < 1.0) {
        metronome_samples_per_beat_ = 1.0;
    }

    constexpr float kClickLengthSec = 0.01f;
    const int click_samples = std::max(1, static_cast<int>(sample_rate * kClickLengthSec + 0.5f));
    metronome_click_samples_total_ = click_samples;

    constexpr float kClickFreq = 1000.0f;
    metronome_click_phase_inc_ = (kTwoPi * kClickFreq) / static_cast<float>(sample_rate);

    const float target = 0.001f;
    metronome_click_decay_ = std::exp(std::log(target) / static_cast<float>(click_samples));
}

float Metronome::next_sample() {
    const bool metronome_target = enabled_.load(std::memory_order_relaxed);
    if (metronome_target != metronome_enabled_) {
        metronome_enabled_ = metronome_target;
        metronome_sample_counter_ = 0.0;
        metronome_click_samples_remaining_ = 0;
        metronome_click_env_ = 0.0f;
        metronome_click_phase_ = 0.0f;
    }

    const int bpm_state = bpm_.load(std::memory_order_relaxed);
    const bool bpm_changed = (bpm_state != metronome_bpm_);
    if (bpm_changed) {
        metronome_bpm_ = bpm_state;
    }

    const float volume_state = volume_.load(std::memory_order_relaxed);
    if (volume_state != metronome_volume_) {
        metronome_volume_ = volume_state;
    }

    const bool timing_dirty = bpm_changed;
    if (timing_dirty) {
        update_timing();
        if (metronome_enabled_) {
            if (metronome_sample_counter_ <= 0.0 ||
                metronome_sample_counter_ > metronome_samples_per_beat_) {
                metronome_sample_counter_ = metronome_samples_per_beat_;
            }
        }
    }

    metronome_bpm_smoothed_ +=
        metronome_bpm_smooth_alpha_ * (metronome_bpm_ - metronome_bpm_smoothed_);
    metronome_volume_smoothed_ +=
        metronome_volume_smooth_alpha_ * (metronome_volume_ - metronome_volume_smoothed_);

    const int sample_rate = sample_rate_.load(std::memory_order_relaxed);
    if (metronome_bpm_smoothed_ > 0.0f) {
        metronome_samples_per_beat_ =
            (static_cast<double>(sample_rate) * 60.0) / metronome_bpm_smoothed_;
    }

    if (!metronome_enabled_ || metronome_samples_per_beat_ <= 0.0) {
        return 0.0f;
    }

    metronome_sample_counter_ -= 1.0;
    if (metronome_sample_counter_ <= 0.0) {
        metronome_sample_counter_ += metronome_samples_per_beat_;
        metronome_click_samples_remaining_ = metronome_click_samples_total_;
        metronome_click_env_ = 1.0f;
        metronome_click_phase_ = 0.0f;
    }

    if (metronome_click_samples_remaining_ <= 0) {
        return 0.0f;
    }

    float click =
        std::sin(metronome_click_phase_) * metronome_click_env_ * metronome_volume_smoothed_;
    metronome_click_phase_ += metronome_click_phase_inc_;
    if (metronome_click_phase_ >= kTwoPi) {
        metronome_click_phase_ -= kTwoPi;
    }
    metronome_click_env_ *= metronome_click_decay_;
    --metronome_click_samples_remaining_;
    return click;
}

}  // namespace Amplitron
