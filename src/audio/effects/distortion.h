#pragma once

// Hard-clipping distortion effect with tone shaping.
// The driven sample u=drive*x[n] is clipped with y=clamp(u, -clip, clip), then
// filtered by the tone stage and level-scaled; this creates odd harmonics as
// the transfer function flattens near the clipping threshold.

#include "audio/effects/effect.h"
#include "audio/dsp/biquad.h"

namespace Amplitron {

class Distortion : public Effect {
public:
    // Create a distortion effect with drive, tone, and output gain controls.
    Distortion();
    // Apply the distortion curve and filtering to a mono audio buffer.
    void process(float* buffer, int num_samples) override;
    // Clear any filter state held by the effect.
    void reset() override;
    // Return the display name for this effect.
    const char* name() const override { return "Distortion"; }
    const char* type_id() const override { return "Distortion"; }
    // Return editable parameters exposed by this effect.
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;
    OnePole tone_lp_;

    // One-pole smoothing states (avoids zipper noise on parameter changes)
    float drive_smoothed_ = 2.0f;
    float tone_smoothed_  = 0.6f;
    float level_smoothed_ = 0.5f;
};

} // namespace Amplitron
