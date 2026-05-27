#include "audio/effects/equalizer.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<Equalizer> reg("Equalizer");

Equalizer::Equalizer() {
    params_ = {
        {"Bass",    0.0f, -12.0f, 12.0f, 0.0f, "dB", "Low shelf filter (200Hz). Boosts or cuts the heavy low-end thud and body of the tone."},
        {"Mid",     0.0f, -12.0f, 12.0f, 0.0f, "dB", "Bell filter (800Hz). Boosts for leads that cut through a mix, cuts for a hollow metal rhythm tone."},
        {"Treble",  0.0f, -12.0f, 12.0f, 0.0f, "dB", "High shelf filter (3kHz). Adjusts the bite, pick attack, and overall brightness."},
        {"Presence", 0.0f, -12.0f, 12.0f, 0.0f, "dB", "Ultra-high frequency contour. Adds 'air' and glassiness to the very top end."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void Equalizer::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    // Snap smoothing states and force recomputation
    bass_state_     = params_[0].value;
    mid_state_      = params_[1].value;
    treble_state_   = params_[2].value;
    presence_state_ = params_.size() > 3 ? params_[3].value : 0.0f;
    cached_bass_ = -999.0f;
    cached_mid_ = -999.0f;
    cached_treble_ = -999.0f;
    cached_presence_ = -999.0f;
    recompute_coefficients_if_dirty();
}

void Equalizer::recompute_coefficients_if_dirty() {
    float bass     = bass_state_;
    float mid      = mid_state_;
    float treble   = treble_state_;
    float presence = presence_state_;

    if (bass != cached_bass_ || mid != cached_mid_ ||
        treble != cached_treble_ || presence != cached_presence_) {
        low_shelf_.set_low_shelf(200.0f, bass, 0.7f, sample_rate_);
        mid_peak_.set_peaking(800.0f, mid, 1.0f, sample_rate_);
        high_shelf_.set_high_shelf(3000.0f, treble + presence, 0.7f, sample_rate_);
        cached_bass_     = bass;
        cached_mid_      = mid;
        cached_treble_   = treble;
        cached_presence_ = presence;
    }
}

void Equalizer::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    // One-pole smoothing: advance states toward raw param targets each block
    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.010f)); // 10 ms
    bass_state_     += alpha * (params_[0].value - bass_state_);
    mid_state_      += alpha * (params_[1].value - mid_state_);
    treble_state_   += alpha * (params_[2].value - treble_state_);
    presence_state_ += alpha * ((params_.size() > 3 ? params_[3].value : 0.0f) - presence_state_);

    recompute_coefficients_if_dirty();

    for (int i = 0; i < num_samples; ++i) {
        float x = buffer[i];
        x = low_shelf_.process(x);
        x = mid_peak_.process(x);
        x = high_shelf_.process(x);
        buffer[i] = x;
    }
}

void Equalizer::reset() {
    low_shelf_.reset();
    mid_peak_.reset();
    high_shelf_.reset();
}

} // namespace Amplitron
