#pragma once

// Lightweight speaker cabinet filtering for guitar amp output shaping.
// Approximates speaker response as H(z)=H_hp(z) * H_peak(z) * H_lp(z): a low
// cut removes rumble, a resonant biquad models cabinet/body emphasis, and a
// low-pass rolloff attenuates harsh high-frequency content.

#include "audio/effect.h"
#include "audio/dsp/biquad.h"

namespace Amplitron {

class CabinetSim : public Effect {
public:
    CabinetSim();
    void process(float* buffer, int num_samples) override;
    void reset() override;
    const char* name() const override { return "Cabinet"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;

    Biquad lp_;   // speaker rolloff
    Biquad hp_;   // low cut
    Biquad peak_; // resonance bump
};

} // namespace Amplitron
