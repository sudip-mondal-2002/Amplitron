#pragma once

// Dynamic range compressor for smoothing guitar input levels.
// An envelope follower estimates level e[n]; above threshold T, gain follows
// g_db = T + (level_db - T)/ratio - level_db, with attack/release smoothing
// applied before multiplying y[n] = x[n] * 10^(g_db/20).

#include "audio/effect.h"
#include "audio/dsp/envelope_follower.h"

namespace Amplitron {

class Compressor : public Effect {
public:
    Compressor();
    void process(float* buffer, int num_samples) override;
    void reset() override;
    const char* name() const override { return "Compressor"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;
    EnvelopeFollower env_;
    float smoothed_attack_ms_ = 5.0f;    // matches default param
    float smoothed_release_ms_ = 100.0f; // matches default param
};

} // namespace Amplitron
