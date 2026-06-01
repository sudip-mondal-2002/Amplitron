#include "audio/effects/overdrive.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<Overdrive> reg("Overdrive");

Overdrive::Overdrive() {
    params_ = {
        {"Drive",  1.5f,  1.0f, 10.0f, 1.5f, "x", "Amount of input gain pushing into soft-clipping. Increases harmonic saturation and sustain."},
        {"Tone",   0.7f,  0.0f,  1.0f, 0.7f, "", "Adjusts high-frequency content. Lower values are darker and smoother, higher values are brighter and more biting."},
        {"Level",  0.7f,  0.0f,  1.0f, 0.7f, "", "Master output volume of the pedal to compensate for the gain added by the Drive control."},
    };
}

void Overdrive::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.010f)); // 10 ms
    smoothed_drive_ += alpha * (params_[0].value - smoothed_drive_);
    smoothed_tone_  += alpha * (params_[1].value - smoothed_tone_);
    smoothed_level_ += alpha * (params_[2].value - smoothed_level_);

    float drive = smoothed_drive_;
    float tone = smoothed_tone_;
    float level = smoothed_level_;

    float lp_coeff = 0.05f + tone * 0.9f;

    for (int i = 0; i < num_samples; ++i) {
        float dry = buffer[i];

        // Asymmetric soft clipping (tube-like)
        float x = buffer[i] * drive;
        if (x > 0.0f) {
            x = 1.0f - std::exp(-x);
        } else {
            x = -1.0f + std::exp(x);
            x *= 0.8f; // asymmetry
        }

        // Tone: LP filter
        x = tone_lp_.lp(x, lp_coeff);

        // DC blocking HP filter
        x = dc_block_.hp(x, 0.001f);

        x *= level;
        buffer[i] = dry * (1.0f - mix_) + x * mix_;
    }
}

void Overdrive::reset() {
    tone_lp_.reset();
    dc_block_.reset();
}

} // namespace Amplitron
