#pragma once

// 3-band parametric equalizer for tone correction and pedal-chain shaping.
// The Equalizer cascades active biquads H(z)=H_lowShelf(z)*H_peakMid(z)*H_highShelf(z),
// giving independent low-shelf, peaking-mid, and high-shelf gain control.

#include "audio/effects/effect.h"
#include "audio/dsp/biquad.h"

namespace Amplitron {

class Equalizer : public Effect {
public:
    Equalizer();
    void process(float* buffer, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "Equalizer"; }
    const char* type_id() const override { return "Equalizer"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;

    Biquad low_shelf_;
    Biquad mid_peak_;
    Biquad high_shelf_;

    // One-pole smoothing states (avoids zipper noise on UI parameter jumps)
    float bass_state_ = 0.0f;
    float mid_state_ = 0.0f;
    float treble_state_ = 0.0f;
    float presence_state_ = 0.0f;

    // Cached parameter values for dirty-check
    float cached_bass_ = -999.0f;
    float cached_mid_ = -999.0f;
    float cached_treble_ = -999.0f;
    float cached_presence_ = -999.0f;

    void recompute_coefficients_if_dirty();
};

} // namespace Amplitron
