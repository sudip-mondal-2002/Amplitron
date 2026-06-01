#pragma once

// Cascaded all-pass phaser with LFO modulation.
// Each all-pass stage has near-unity magnitude and phase shift A(z); mixing
// dry + wet creates notches where phase cancellation occurs. The LFO modulates
// the all-pass coefficient so notch frequencies sweep over time.

#include "audio/effects/effect.h"
#include <array>

namespace Amplitron {

/**
 * Phaser effect — cascaded 1st-order all-pass filters with LFO modulation.
 * Supports 4, 6, 8, or 12 stages (classic MXR Phase 90 to studio phasers).
 */
class Phaser : public Effect {
public:
    Phaser();
    void process(float* buffer, int num_samples) override;
    void process_stereo(float* left, float* right, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "Phaser"; }
    const char* type_id() const override { return "Phaser"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;

    float lfo_phase_ = 0.0f;
    float feedback_state_   = 0.0f;
    float feedback_state_r_ = 0.0f;  // right-channel APF feedback

    static constexpr int MAX_STAGES = 12;
    std::array<float, MAX_STAGES> apf_xprev_{};
    std::array<float, MAX_STAGES> apf_yprev_{};
    // Right-channel APF state — LFO is 180° out of phase for stereo sweep
    std::array<float, MAX_STAGES> apf_xprev_r_{};
    std::array<float, MAX_STAGES> apf_yprev_r_{};
};

} // namespace Amplitron
