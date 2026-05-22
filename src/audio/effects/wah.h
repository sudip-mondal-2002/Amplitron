#pragma once

// Wah filter with manual and envelope-following sweep modes.
// A resonant state-variable band-pass filter sweeps center frequency f_c; in
// auto mode f_c follows an envelope e[n], while manual mode maps pedal position
// directly. Resonance/Q controls bandwidth around f_c.

#include "audio/effect.h"
#include "audio/dsp/envelope_follower.h"

namespace Amplitron {

class WahPedal : public Effect {
public:
    WahPedal();
    void process(float* buffer, int num_samples) override;
    void reset() override;
    const char* name() const override { return "Wah"; }
    const char* type_id() const override { return "Wah"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;

    // State-variable filter (Chamberlin topology) state
    float svf_lp_ = 0.0f;
    float svf_bp_ = 0.0f;

    // Envelope follower state (auto-wah)
    EnvelopeFollower env_;

    // Smoothed sweep position (avoids zipper noise)
    float sweep_smooth_ = 0.5f;

    // Smoothed Q / resonance (avoids zipper noise on knob moves)
    float q_smooth_ = 3.5f;
};

} // namespace Amplitron
