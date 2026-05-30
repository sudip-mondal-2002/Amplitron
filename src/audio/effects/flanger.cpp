#include "audio/effects/flanger.h"
#include "audio/effects/effect_factory.h"
#include <cmath>

namespace Amplitron {

static EffectRegistrar<Flanger> reg("Flanger");

// Param indices
static constexpr int P_RATE     = 0;
static constexpr int P_DEPTH    = 1;
static constexpr int P_DELAY    = 2;
static constexpr int P_FEEDBACK = 3;
static constexpr int P_MIX      = 4;

// Max total delay: 15ms base + 7ms depth = 22ms. Size to 30ms for safety.
static constexpr float MAX_DELAY_MS = 30.0f;

Flanger::Flanger() {
    params_ = {
        {"Rate",     0.3f, 0.05f,  5.0f, 0.3f, "Hz",
         "LFO speed. Slow rates create a dreamy sweep; fast rates push toward vibrato territory."},
        {"Depth",    2.0f,  0.1f,  7.0f, 2.0f, "ms",
         "Modulation depth in milliseconds. Controls the width of the delay time sweep around the base delay."},
        {"Delay",    1.0f,  0.1f, 10.0f, 1.0f, "ms",
         "Base delay time. Short values (< 2ms) create tight comb filtering; longer values soften the effect."},
        {"Feedback", 0.7f, -0.95f, 0.95f, 0.7f, "",
         "Feedback amount. Positive values add metallic resonance; negative values create a hollow, phasey character."},
        {"Mix",      0.5f,  0.0f,  1.0f, 0.5f, "",
         "Dry/wet blend. At 0.5 the comb filter notches are deepest for the classic flanger sound."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void Flanger::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    max_delay_samples_ = static_cast<int>(sample_rate * MAX_DELAY_MS * 0.001f) + 2;
    delay_buffer_.assign(max_delay_samples_, 0.0f);
    delay_buffer_r_.assign(max_delay_samples_, 0.0f);
    reset();
}

void Flanger::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    const float rate     = params_[P_RATE].value;
    const float depth_ms = params_[P_DEPTH].value;
    const float delay_ms = params_[P_DELAY].value;
    const float feedback = params_[P_FEEDBACK].value;
    const float mix      = params_[P_MIX].value;

    const float lfo_inc = rate / static_cast<float>(sample_rate_);

    for (int i = 0; i < num_samples; ++i) {
        const float dry = buffer[i];

        // LFO in [0, 1]
        const float lfo = 0.5f * (1.0f + std::sin(TWO_PI * lfo_phase_));

        // Modulated delay in samples
        const float delay_samples = clamp(
            (delay_ms + lfo * depth_ms) * 0.001f * static_cast<float>(sample_rate_),
            1.0f, static_cast<float>(max_delay_samples_ - 2));

        // Fractional read position
        float read_pos_f = static_cast<float>(write_pos_) - delay_samples;
        if (read_pos_f < 0.0f) read_pos_f += static_cast<float>(max_delay_samples_);

        const int ipos = static_cast<int>(read_pos_f);
        const float frac = read_pos_f - static_cast<float>(ipos);
        const int pos0 = ipos % max_delay_samples_;
        const int pos1 = (ipos + 1) % max_delay_samples_;

        const float delayed = delay_buffer_[pos0] * (1.0f - frac) + delay_buffer_[pos1] * frac;

        // Write dry + feedback into the delay line; clamp to prevent runaway
        delay_buffer_[write_pos_] = clamp(dry + feedback * delayed, -2.0f, 2.0f);

        buffer[i] = dry * (1.0f - mix) + delayed * mix;

        write_pos_ = (write_pos_ + 1) % max_delay_samples_;
        lfo_phase_ += lfo_inc;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
    }
}

void Flanger::process_stereo(float* left, float* right, int num_samples) {
    if (!enabled_) {
        return;
    }

    const float rate     = params_[P_RATE].value;
    const float depth_ms = params_[P_DEPTH].value;
    const float delay_ms = params_[P_DELAY].value;
    const float feedback = params_[P_FEEDBACK].value;
    const float mix      = params_[P_MIX].value;
    const float lfo_inc  = rate / static_cast<float>(sample_rate_);

    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];

        // Left LFO
        const float lfo_l      = 0.5f * (1.0f + std::sin(TWO_PI * lfo_phase_));
        const float delay_samp_l = clamp(
            (delay_ms + lfo_l * depth_ms) * 0.001f * static_cast<float>(sample_rate_),
            1.0f, static_cast<float>(max_delay_samples_ - 2));

        float rp_l = static_cast<float>(write_pos_) - delay_samp_l;
        if (rp_l < 0.0f) rp_l += static_cast<float>(max_delay_samples_);
        const int ip_l  = static_cast<int>(rp_l);
        const float f_l = rp_l - static_cast<float>(ip_l);
        const float delayed_l = delay_buffer_[ip_l % max_delay_samples_] * (1.0f - f_l) +
                                delay_buffer_[(ip_l + 1) % max_delay_samples_] * f_l;

        delay_buffer_[write_pos_] = clamp(dry_l + feedback * delayed_l, -2.0f, 2.0f);
        left[i] = dry_l * (1.0f - mix) + delayed_l * mix;

        // Right LFO — 180° offset (0.5 of normalised cycle)
        const float lfo_r      = 0.5f * (1.0f + std::sin(TWO_PI * (lfo_phase_ + 0.5f)));
        const float delay_samp_r = clamp(
            (delay_ms + lfo_r * depth_ms) * 0.001f * static_cast<float>(sample_rate_),
            1.0f, static_cast<float>(max_delay_samples_ - 2));

        float rp_r = static_cast<float>(write_pos_r_) - delay_samp_r;
        if (rp_r < 0.0f) rp_r += static_cast<float>(max_delay_samples_);
        const int ip_r  = static_cast<int>(rp_r);
        const float f_r = rp_r - static_cast<float>(ip_r);
        const float delayed_r = delay_buffer_r_[ip_r % max_delay_samples_] * (1.0f - f_r) +
                                delay_buffer_r_[(ip_r + 1) % max_delay_samples_] * f_r;

        delay_buffer_r_[write_pos_r_] = clamp(dry_r + feedback * delayed_r, -2.0f, 2.0f);
        right[i] = dry_r * (1.0f - mix) + delayed_r * mix;

        write_pos_   = (write_pos_   + 1) % max_delay_samples_;
        write_pos_r_ = (write_pos_r_ + 1) % max_delay_samples_;
        lfo_phase_ += lfo_inc;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
    }
}

void Flanger::reset() {
    std::fill(delay_buffer_.begin(),   delay_buffer_.end(),   0.0f);
    std::fill(delay_buffer_r_.begin(), delay_buffer_r_.end(), 0.0f);
    write_pos_   = 0;
    write_pos_r_ = 0;
    lfo_phase_   = 0.0f;
}

} // namespace Amplitron
