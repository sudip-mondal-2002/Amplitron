#include "audio/effects/pitch_shifter.h"
#include "audio/effects/effect_factory.h"
#include <cmath>

namespace Amplitron {

static EffectRegistrar<PitchShifter> reg("Pitch Shifter");

// Param indices
static constexpr int P_SHIFT = 0;
static constexpr int P_FINE  = 1;
static constexpr int P_MIX   = 2;

// Grain window size in seconds (~23 ms at 48kHz = 1024 samples)
static constexpr float GRAIN_WINDOW_SEC = 0.023f;

static float wrap_phase(float phase, int buf_size) {
    phase = std::fmod(phase, static_cast<float>(buf_size));
    if (phase < 0.0f) phase += static_cast<float>(buf_size);
    return phase;
}

PitchShifter::PitchShifter() {
    params_ = {
        {"Shift", 0.0f, -12.0f, 12.0f, 0.0f, "st",
         "Pitch shift in semitones. Negative shifts down, positive shifts up. 12 = one octave."},
        {"Fine",  0.0f, -50.0f, 50.0f, 0.0f, "ct",
         "Fine-tune adjustment in cents (hundredths of a semitone) for precise detuning."},
        {"Mix",   0.0f,   0.0f,  1.0f, 0.0f, "",
         "Dry/wet blend. 0 = fully dry, 1 = fully pitch-shifted."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void PitchShifter::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    buf_size_ = static_cast<int>(sample_rate * GRAIN_WINDOW_SEC * 2.0f);
    if (buf_size_ < 256) buf_size_ = 256;
    grain_buf_.assign(buf_size_, 0.0f);
    reset();
}

float PitchShifter::read_linear(float phase) const {
    phase = wrap_phase(phase, buf_size_);

    int pos0 = static_cast<int>(phase);
    int pos1 = (pos0 + 1) % buf_size_;
    float frac = phase - static_cast<float>(pos0);
    return grain_buf_[pos0] * (1.0f - frac) + grain_buf_[pos1] * frac;
}

void PitchShifter::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.010f));

    for (int i = 0; i < num_samples; ++i) {
        const float dry = buffer[i];

        // Write input into circular grain buffer
        grain_buf_[write_pos_] = dry;

        // Smooth parameters
        shift_smooth_ += alpha * (params_[P_SHIFT].value - shift_smooth_);
        fine_smooth_  += alpha * (params_[P_FINE].value  - fine_smooth_);
        mix_smooth_   += alpha * (params_[P_MIX].value   - mix_smooth_);

        // Total shift in semitones (coarse + fine)
        float total_semitones = shift_smooth_ + fine_smooth_ / 100.0f;

        // Pitch ratio: 2^(semitones/12)
        float ratio = std::pow(2.0f, total_semitones / 12.0f);

        // Read pointer increment: how much faster/slower we read vs write
        // ratio > 1 means pitch up -> read faster -> increment > 1
        // We want the *offset* from write to change, so increment = 1 - ratio
        // gives us the drift rate of the read pointer relative to write.
        float drift = 1.0f - ratio;

        // Advance read phases (they drift relative to write position)
        read_phase_a_ += drift;
        read_phase_b_ += drift;

        // Wrap phases into [0, buf_size_) in constant time.
        read_phase_a_ = wrap_phase(read_phase_a_, buf_size_);
        read_phase_b_ = wrap_phase(read_phase_b_, buf_size_);

        // Compute absolute read positions in the buffer
        float pos_a = static_cast<float>(write_pos_) - read_phase_a_;
        float pos_b = static_cast<float>(write_pos_) - read_phase_b_;

        // Read from both taps with linear interpolation
        float tap_a = read_linear(pos_a);
        float tap_b = read_linear(pos_b);

        // Raised-cosine crossfade based on distance between taps
        // Tap A and B are offset by half_buf; crossfade so the one closer
        // to write_pos_ is louder (its grain is fresher).
        // Use read_phase_a_ as the crossfade driver: as it sweeps 0..buf_size_,
        // we fade A in for the first half and B in for the second half.
        float fade_pos = read_phase_a_ / static_cast<float>(buf_size_);
        // Hann window for smooth crossfade
        float gain_a = 0.5f * (1.0f - std::cos(TWO_PI * fade_pos));
        float gain_b = 1.0f - gain_a;

        float wet = tap_a * gain_a + tap_b * gain_b;

        // Dry/wet mix
        buffer[i] = dry * (1.0f - mix_smooth_) + wet * mix_smooth_;

        // Advance write position
        write_pos_ = (write_pos_ + 1) % buf_size_;
    }
}

void PitchShifter::reset() {
    std::fill(grain_buf_.begin(), grain_buf_.end(), 0.0f);
    write_pos_ = 0;
    // Start tap B offset by half the buffer from tap A
    read_phase_a_ = 0.0f;
    read_phase_b_ = static_cast<float>(buf_size_) * 0.5f;
    shift_smooth_ = params_[P_SHIFT].value;
    fine_smooth_ = params_[P_FINE].value;
    mix_smooth_ = params_[P_MIX].value;
}

} // namespace Amplitron
