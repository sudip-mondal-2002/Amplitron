#include "audio/effects/phaser.h"
#include "audio/effects/effect_factory.h"
#include <cmath>

namespace Amplitron {

static EffectRegistrar<Phaser> reg("Phaser");

// Param indices
static constexpr int P_RATE     = 0;
static constexpr int P_DEPTH    = 1;
static constexpr int P_STAGES   = 2;
static constexpr int P_FEEDBACK = 3;
static constexpr int P_MIX      = 4;

// Stage count table: Stages param 0..3 -> 4/6/8/12 stages
static constexpr int STAGE_COUNTS[4] = {4, 6, 8, 12};

Phaser::Phaser() {
    params_ = {
        {"Rate",     0.5f, 0.05f, 10.0f, 0.5f, "Hz",
         "LFO speed. Low values create a slow, hypnotic sweep; high values create a fast vibrato-like effect."},
        {"Depth",    0.7f,  0.0f,  1.0f, 0.7f, "",
         "Modulation depth. Controls how wide the all-pass frequency sweeps across the spectrum."},
        {"Stages",   0.0f,  0.0f,  3.0f, 0.0f, "",
         "Number of all-pass stages: 0=4 (Phase 90), 1=6, 2=8, 3=12. More stages add more notches."},
        {"Feedback", 0.5f,  0.0f, 0.95f, 0.5f, "",
         "Feeds the chain output back to the input, adding resonance and intensity to the phasing notches."},
        {"Mix",      0.5f,  0.0f,  1.0f, 0.5f, "",
         "Dry/wet blend. At 0.5 the notch effect is most pronounced (classic phaser mix point)."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void Phaser::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    reset();
}

void Phaser::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    const float rate     = params_[P_RATE].value;
    const float depth    = params_[P_DEPTH].value;
    const int   nstages  = STAGE_COUNTS[(int)clamp(params_[P_STAGES].value + 0.5f, 0.0f, 3.0f)];
    const float feedback = params_[P_FEEDBACK].value;
    const float mix      = params_[P_MIX].value;

    const float lfo_inc = rate / static_cast<float>(sample_rate_);
    // Logarithmic sweep: base_freq * exp(lfo * depth * ln(ratio))
    // fc range: ~200 Hz (lfo=0) to ~4000 Hz (lfo=1, depth=1)
    const float log_ratio = std::log(20.0f);  // ln(4000/200)

    for (int i = 0; i < num_samples; ++i) {
        const float dry = buffer[i];

        // LFO in [0, 1]
        const float lfo = 0.5f * (1.0f + std::sin(TWO_PI * lfo_phase_));

        // Modulated all-pass corner frequency (log sweep)
        const float fc = clamp(200.0f * std::exp(lfo * depth * log_ratio),
                               80.0f, static_cast<float>(sample_rate_) * 0.40f);

        // 1st-order all-pass coefficient: c = (tan(π*fc/fs) - 1) / (tan(π*fc/fs) + 1)
        const float t   = std::tan(3.14159265f * fc / static_cast<float>(sample_rate_));
        const float apc = (t - 1.0f) / (t + 1.0f);

        // Feed input + feedback into the all-pass cascade
        float x = dry + feedback * feedback_state_;

        for (int s = 0; s < nstages; ++s) {
            // y[n] = c * (x[n] - y[n-1]) + x[n-1]
            const float y = apc * (x - apf_yprev_[s]) + apf_xprev_[s];
            apf_xprev_[s] = x;
            apf_yprev_[s] = y;
            x = y;
        }

        feedback_state_ = x;

        buffer[i] = dry * (1.0f - mix) + x * mix;

        lfo_phase_ += lfo_inc;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
    }
}

void Phaser::process_stereo(float* left, float* right, int num_samples) {
    if (!enabled_) {
        return;
    }

    const float rate     = params_[P_RATE].value;
    const float depth    = params_[P_DEPTH].value;
    const int   nstages  = STAGE_COUNTS[(int)clamp(params_[P_STAGES].value + 0.5f, 0.0f, 3.0f)];
    const float feedback = params_[P_FEEDBACK].value;
    const float mix      = params_[P_MIX].value;

    const float lfo_inc   = rate / static_cast<float>(sample_rate_);
    const float log_ratio = std::log(20.0f);

    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];

        // Left LFO
        const float lfo_l = 0.5f * (1.0f + std::sin(TWO_PI * lfo_phase_));
        const float fc_l  = clamp(200.0f * std::exp(lfo_l * depth * log_ratio),
                                  80.0f, static_cast<float>(sample_rate_) * 0.40f);
        const float t_l   = std::tan(3.14159265f * fc_l / static_cast<float>(sample_rate_));
        const float apc_l = (t_l - 1.0f) / (t_l + 1.0f);

        // Right LFO — 180° offset (0.5 of normalised cycle)
        const float lfo_r = 0.5f * (1.0f + std::sin(TWO_PI * (lfo_phase_ + 0.5f)));
        const float fc_r  = clamp(200.0f * std::exp(lfo_r * depth * log_ratio),
                                  80.0f, static_cast<float>(sample_rate_) * 0.40f);
        const float t_r   = std::tan(3.14159265f * fc_r / static_cast<float>(sample_rate_));
        const float apc_r = (t_r - 1.0f) / (t_r + 1.0f);

        // Left APF cascade
        float x_l = dry_l + feedback * feedback_state_;
        for (int s = 0; s < nstages; ++s) {
            const float y = apc_l * (x_l - apf_yprev_[s]) + apf_xprev_[s];
            apf_xprev_[s] = x_l;
            apf_yprev_[s] = y;
            x_l = y;
        }
        feedback_state_ = x_l;

        // Right APF cascade
        float x_r = dry_r + feedback * feedback_state_r_;
        for (int s = 0; s < nstages; ++s) {
            const float y = apc_r * (x_r - apf_yprev_r_[s]) + apf_xprev_r_[s];
            apf_xprev_r_[s] = x_r;
            apf_yprev_r_[s] = y;
            x_r = y;
        }
        feedback_state_r_ = x_r;

        left[i]  = dry_l * (1.0f - mix) + x_l * mix;
        right[i] = dry_r * (1.0f - mix) + x_r * mix;

        lfo_phase_ += lfo_inc;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
    }
}

void Phaser::reset() {
    lfo_phase_        = 0.0f;
    feedback_state_   = 0.0f;
    feedback_state_r_ = 0.0f;
    apf_xprev_.fill(0.0f);
    apf_yprev_.fill(0.0f);
    apf_xprev_r_.fill(0.0f);
    apf_yprev_r_.fill(0.0f);
}

} // namespace Amplitron
