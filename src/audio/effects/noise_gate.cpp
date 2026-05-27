#include "audio/effects/noise_gate.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<NoiseGate> reg("Noise Gate");

NoiseGate::NoiseGate() {
    params_ = {
        {"Threshold", -55.0f, -80.0f, 0.0f, -55.0f, "dB", "Signal level below which the gate closes and mutes the audio. Set just above background noise level."},
        {"Attack",     0.5f,   0.1f,  10.0f, 0.5f,  "ms", "How quickly the gate opens when the signal exceeds the threshold. Fast attack preserves pick transients."},
        {"Release",   50.0f,   5.0f, 500.0f, 50.0f,  "ms", "How quickly the gate closes after the signal falls below threshold. Longer release preserves sustained notes."},
    };
}

void NoiseGate::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    float threshold = db_to_linear(params_[0].value);
    float attack_coeff = EnvelopeFollower::time_to_coeff(params_[1].value, sample_rate_);
    float release_coeff = EnvelopeFollower::time_to_coeff(params_[2].value, sample_rate_);

    for (int i = 0; i < num_samples; ++i) {
        float envelope = env_.process(buffer[i], attack_coeff, release_coeff);

        float target_gain = (envelope > threshold) ? 1.0f : 0.0f;
        float gain_coeff = (target_gain > gain_) ? attack_coeff : release_coeff;
        gain_ += (target_gain - gain_) * (1.0f - gain_coeff);
        buffer[i] *= gain_;
    }
}

void NoiseGate::reset() {
    env_.reset();
    gain_ = 0.0f;
}

} // namespace Amplitron
