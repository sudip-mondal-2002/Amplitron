#include "audio/effects/chorus.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<Chorus> reg("Chorus");

Chorus::Chorus() {
    params_ = {
        {"Rate",   1.5f, 0.1f, 10.0f, 1.5f, "Hz", "Speed of the modulation sweep. Higher values create faster wobbling effects."},
        {"Depth",  5.0f, 0.5f, 20.0f, 5.0f, "ms", "Intensity of the modulation. Higher values create a deeper, more pronounced pitch shift."},
        {"Level",  0.5f, 0.0f,  1.0f, 0.5f, "", "Mix volume of the chorus effect. 0 is dry, 1 is fully wet."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void Chorus::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    max_delay_samples_ = static_cast<int>(sample_rate * 0.05f); // 50ms max
    delay_buffer_.assign(max_delay_samples_, 0.0f);
    write_pos_ = 0;
}

void Chorus::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.020f));
    smoothed_rate_ += alpha * (params_[0].value - smoothed_rate_);
    float rate = smoothed_rate_;
    float depth_ms = params_[1].value;
    float level = params_[2].value;

    float depth_samples = depth_ms * 0.001f * sample_rate_;
    float lfo_inc = rate / sample_rate_;

    for (int i = 0; i < num_samples; ++i) {
        float dry = buffer[i];

        // Write current sample to delay buffer
        delay_buffer_[write_pos_] = buffer[i];

        // LFO modulated delay
        float lfo = 0.5f * (1.0f + std::sin(TWO_PI * lfo_phase_));
        float delay = 1.0f + lfo * depth_samples;

        // Linear interpolation for fractional delay
        float read_pos_f = static_cast<float>(write_pos_) - delay;
        if (read_pos_f < 0.0f) read_pos_f += max_delay_samples_;

        int read_pos_i = static_cast<int>(read_pos_f);
        float frac = read_pos_f - read_pos_i;

        int pos0 = read_pos_i % max_delay_samples_;
        int pos1 = (read_pos_i + 1) % max_delay_samples_;

        float delayed = delay_buffer_[pos0] * (1.0f - frac) + delay_buffer_[pos1] * frac;

        buffer[i] = dry * (1.0f - level * 0.5f) + delayed * level;

        write_pos_ = (write_pos_ + 1) % max_delay_samples_;
        lfo_phase_ += lfo_inc;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
    }
}

void Chorus::process_stereo(float* left, float* right, int num_samples) {
    if (!enabled_) {
        return;
    }
    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.020f));
    smoothed_rate_ += alpha * (params_[0].value - smoothed_rate_);
    float rate = smoothed_rate_;
    float depth_ms = params_[1].value;
    float level = params_[2].value;
    
    const float depth_samp = depth_ms * 0.001f * sample_rate_;
    const float lfo_inc    = rate / sample_rate_;

    for (int i = 0; i < num_samples; ++i) {
        const float dry = left[i];

        delay_buffer_[write_pos_] = dry;

        // L: LFO at current phase
        const float lfo_l  = 0.5f * (1.0f + std::sin(TWO_PI * lfo_phase_));
        const float dly_l  = 1.0f + lfo_l * depth_samp;
        // R: LFO at +90° (0.25 of normalised cycle) for quadrature width
        const float lfo_r  = 0.5f * (1.0f + std::sin(TWO_PI * (lfo_phase_ + 0.25f)));
        const float dly_r  = 1.0f + lfo_r * depth_samp;

        // Fractional read helper (capture by reference into the loop)
        auto read_tap = [&](float delay) -> float {
            float rp = static_cast<float>(write_pos_) - delay;
            if (rp < 0.0f) rp += static_cast<float>(max_delay_samples_);
            const int ri  = static_cast<int>(rp);
            const float f = rp - static_cast<float>(ri);
            const int p0  = ri % max_delay_samples_;
            const int p1  = (ri + 1) % max_delay_samples_;
            return delay_buffer_[p0] * (1.0f - f) + delay_buffer_[p1] * f;
        };

        const float dry_gain = 1.0f - level * 0.5f;
        left[i]  = dry * dry_gain + read_tap(dly_l) * level;
        right[i] = dry * dry_gain + read_tap(dly_r) * level;

        write_pos_ = (write_pos_ + 1) % max_delay_samples_;
        lfo_phase_ += lfo_inc;
        if (lfo_phase_ >= 1.0f) lfo_phase_ -= 1.0f;
    }
}

void Chorus::set_transport_state(float bpm){
    if(!std::isfinite(bpm) || bpm <= 0.0f)return;
    if(bpm == last_bpm_) return;
    last_bpm_ = bpm;
    //BPM to Hz
    float target_rate_hz = bpm / 60.0f;
    //set knob
    params_[0].value = clamp(target_rate_hz, params_[0].min_val, params_[0].max_val);
}

void Chorus::reset() {
    std::fill(delay_buffer_.begin(), delay_buffer_.end(), 0.0f);
    write_pos_ = 0;
    lfo_phase_ = 0.0f;
}

} // namespace Amplitron
