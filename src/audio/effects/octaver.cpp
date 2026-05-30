#include "audio/effects/octaver.h"
#include "audio/effects/effect_factory.h"
#include <cmath>

namespace Amplitron {

static EffectRegistrar<Octaver> reg("Octaver");

// Param indices
static constexpr int P_OCT_DOWN = 0;
static constexpr int P_OCT_UP   = 1;
static constexpr int P_DRY      = 2;

// Hysteresis threshold for the flip-flop zero-crossing detector.
// The divider only flips when prev_sample_ < -FLIP_HYSTERESIS and the current
// sample > +FLIP_HYSTERESIS, preventing chatter caused by near-zero noise.
//
// Value choice: 0.002 ≈ -54 dBFS — comfortably above the noise floor of a
// typical USB audio interface while remaining well below the smallest musically
// significant guitar signal.  At this threshold the per-sample slope of the
// slowest guitar fundamental (E2 ≈ 82 Hz at 48 kHz) still crosses the ±H band
// reliably, so the divider tracks pitch without false negatives.  Tune upward
// only if the target hardware has an unusually noisy input stage.
static constexpr float FLIP_HYSTERESIS = 0.002f;

Octaver::Octaver() {
    params_ = {
        {"Oct -1", 0.5f, 0.0f, 1.0f, 0.5f, "",
         "Level of the sub-octave (one octave below). Produces a thick, organ-like low tone."},
        {"Oct +1", 0.0f, 0.0f, 1.0f, 0.0f, "",
         "Level of the upper octave (one octave above). Adds a bright, shimmery harmonic."},
        {"Dry",    0.7f, 0.0f, 1.0f, 0.7f, "",
         "Level of the original dry signal blended with the octave voices."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void Octaver::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    reset();
}

void Octaver::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    // One-pole smoothing coefficient (~10 ms time constant)
    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.010f));

    // Envelope follower coefficients
    const float env_attack  = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.002f));  // 2 ms
    const float env_release = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.020f));  // 20 ms

    // DC blocker coefficient (high-pass at ~20 Hz)
    const float dc_coeff = 1.0f - (TWO_PI * 20.0f / sample_rate_);

    for (int i = 0; i < num_samples; ++i) {
        const float dry = buffer[i];

        // Smooth parameters
        oct_down_smooth_ += alpha * (params_[P_OCT_DOWN].value - oct_down_smooth_);
        oct_up_smooth_   += alpha * (params_[P_OCT_UP].value   - oct_up_smooth_);
        dry_smooth_      += alpha * (params_[P_DRY].value      - dry_smooth_);

        // --- Envelope follower (tracks input amplitude) ---
        const float abs_in = std::fabs(dry);
        if (abs_in > envelope_) {
            envelope_ += env_attack * (abs_in - envelope_);
        } else {
            envelope_ += env_release * (abs_in - envelope_);
        }

        // --- Oct-1: Flip-flop divider ---
        // Detect positive-going zero crossing with hysteresis: require the
        // previous sample to have been clearly negative and the current sample
        // to be clearly positive before toggling.  This suppresses chatter from
        // near-zero noise that would otherwise cause false flips.
        if (prev_sample_ < -FLIP_HYSTERESIS && dry > FLIP_HYSTERESIS) {
            flipflop_ = -flipflop_;
        }
        prev_sample_ = dry;

        // Square wave at half frequency, shaped by envelope
        float oct_down = flipflop_ * envelope_;

        // --- Oct+1: Full-wave rectification + DC removal ---
        float rectified = std::fabs(dry);

        // DC blocker (1st-order high-pass): removes the DC offset from rectification
        float dc_out = rectified - dc_x1_ + dc_coeff * dc_y1_;
        dc_x1_ = rectified;
        dc_y1_ = dc_out;

        float oct_up = dc_out;

        // --- Mix ---
        buffer[i] = dry * dry_smooth_
                   + oct_down * oct_down_smooth_
                   + oct_up * oct_up_smooth_;

        // Safety clamp
        buffer[i] = clamp(buffer[i], -1.0f, 1.0f);
    }
}

void Octaver::reset() {
    prev_sample_ = 0.0f;
    flipflop_ = 1.0f;
    dc_x1_ = 0.0f;
    dc_y1_ = 0.0f;
    envelope_ = 0.0f;
    oct_down_smooth_ = params_[P_OCT_DOWN].value;
    oct_up_smooth_ = params_[P_OCT_UP].value;
    dry_smooth_ = params_[P_DRY].value;
}

} // namespace Amplitron
