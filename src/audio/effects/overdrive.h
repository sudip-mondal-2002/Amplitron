#pragma once

// Soft-clipping overdrive effect for tube-style saturation.
// The transfer curve uses smooth saturation such as y=tanh(drive*x[n]) or an
// equivalent soft clip, preserving small-signal dynamics while compressing
// peaks before tone filtering and output level scaling.

#include "audio/effect.h"
#include "audio/dsp/biquad.h"

namespace Amplitron {

class Overdrive : public Effect {
public:
    Overdrive();
    void process(float* buffer, int num_samples) override;
    void reset() override;
    const char* name() const override { return "Overdrive"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;
    OnePole tone_lp_;
    OnePole dc_block_;

    // One-pole smoothed parameter states
    float smoothed_drive_ = 1.5f;
    float smoothed_tone_ = 0.7f;
    float smoothed_level_ = 0.7f;
};

} // namespace Amplitron
