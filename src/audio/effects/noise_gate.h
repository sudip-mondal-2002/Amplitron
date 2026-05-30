#pragma once

// Noise gate that attenuates low-level input between notes.
// An envelope follower tracks amplitude e[n]; when e[n] falls below threshold
// T, gain approaches 0 with release smoothing, and when e[n]>=T it approaches
// 1 with attack smoothing, preventing abrupt chopping.

#include "audio/effects/effect.h"
#include "audio/dsp/envelope_follower.h"

namespace Amplitron {

class NoiseGate : public Effect {
public:
    NoiseGate();
    void process(float* buffer, int num_samples) override;
    void reset() override;
    const char* name() const override { return "Noise Gate"; }
    const char* type_id() const override { return "Noise Gate"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;
    EnvelopeFollower env_;
    float gain_ = 0.0f;
};

} // namespace Amplitron
