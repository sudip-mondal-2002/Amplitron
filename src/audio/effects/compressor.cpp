#include "audio/effects/compressor.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<Compressor> reg("Compressor");

Compressor::Compressor() {
    params_ = {
        {"Threshold", -20.0f, -60.0f,  0.0f, -20.0f, "dB", "Signal level at which compression begins. Signals above this level will be reduced in volume."},
        {"Ratio",       4.0f,   1.0f, 20.0f,   4.0f, ":1", "How strongly to reduce peaks that exceed the threshold. Higher values create a more squashed sound."},
        {"Attack",      5.0f,   0.1f, 50.0f,   5.0f, "ms", "How quickly the compressor reacts to peaks. Fast attack clamps down immediately, slow allows pick transients through."},
        {"Release",   100.0f,  10.0f, 500.0f, 100.0f, "ms", "How quickly the compressor stops reducing volume after the signal drops below threshold."},
        {"Makeup",      0.0f,   0.0f, 30.0f,   0.0f, "dB", "Gain boost applied after compression to compensate for the volume lost during peak reduction."},
    };
}

void Compressor::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    float threshold_db = params_[0].value;
    float ratio = params_[1].value;
    float attack_ms = params_[2].value;
    float release_ms = params_[3].value;
    float makeup = db_to_linear(params_[4].value);

    // Parameter smoothing (anti-zipper) — keep short enough to reduce zipper noise
    // while limiting lag in fast attack/release settings.
    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.002f)); // 2 ms
    smoothed_attack_ms_  += alpha * (attack_ms  - smoothed_attack_ms_);
    smoothed_release_ms_ += alpha * (release_ms - smoothed_release_ms_);

    float attack_coeff = EnvelopeFollower::time_to_coeff(smoothed_attack_ms_, sample_rate_);
    float release_coeff = EnvelopeFollower::time_to_coeff(smoothed_release_ms_, sample_rate_);

    for (int i = 0; i < num_samples; ++i) {
        float envelope = env_.process_additive(buffer[i], attack_coeff, release_coeff);

        float env_db = linear_to_db(envelope);
        float gain_db = 0.0f;
        if (env_db > threshold_db) {
            gain_db = (threshold_db - env_db) * (1.0f - 1.0f / ratio);
        }

        float gain = db_to_linear(gain_db) * makeup;
        buffer[i] *= gain;
    }
}

void Compressor::reset() {
    env_.reset();
}

} // namespace Amplitron
