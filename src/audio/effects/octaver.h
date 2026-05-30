#pragma once

// Octave generator for sub-octave and upper-octave guitar tones.
// Sub-octave generation derives a period-doubled component from zero-crossing
// state, while the upper octave emphasizes rectified/nonlinear content; output
// blends dry, sub, and upper components by their mix gains.

#include "audio/effects/effect.h"

namespace Amplitron {

/**
 * Monophonic Octaver — generates sub-octave (Oct-1) and upper-octave (Oct+1)
 * signals blended with the dry input.
 *
 * Oct-1: Zero-crossing flip-flop divider produces a square wave at half the
 *        input frequency, shaped by the input envelope for a warm, organ-like tone.
 * Oct+1: Full-wave rectification (|x|) doubles the fundamental frequency,
 *        followed by DC removal and envelope shaping.
 *
 * Classic references: Boss OC-2, EHX Octave Multiplexer.
 */
class Octaver : public Effect {
public:
    Octaver();
    void process(float* buffer, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "Octaver"; }
    const char* type_id() const override { return "Octaver"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;

    // Oct-1 state: flip-flop divider
    float prev_sample_ = 0.0f;       // previous sample for zero-crossing detection
    float flipflop_ = 1.0f;          // +1 or -1, toggles on hysteresis-gated positive zero crossings

    // Oct+1 state: DC blocker for full-wave rectified signal
    float dc_x1_ = 0.0f;             // previous input to DC blocker
    float dc_y1_ = 0.0f;             // previous output of DC blocker

    // Envelope follower for shaping synthesized octave signals
    float envelope_ = 0.0f;

    // Parameter smoothing
    float oct_down_smooth_ = 0.0f;
    float oct_up_smooth_ = 0.0f;
    float dry_smooth_ = 0.0f;
};

} // namespace Amplitron
