#pragma once

// Algorithmic reverb for adding room and tail reflections.
// Uses a Schroeder-style network: parallel feedback combs y_i[n]=x[n]+g_i*y_i[n-d_i]
// build the decay tail, followed by all-pass filters that diffuse echoes while
// keeping magnitude roughly flat.

#include "audio/effects/effect.h"

namespace Amplitron {

class Reverb : public Effect {
public:
    Reverb();
    void process(float* buffer, int num_samples) override;
    void process_stereo(float* left, float* right, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "Reverb"; }
    const char* type_id() const override { return "Reverb"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;

    // Schroeder reverb: 4 comb filters + 2 allpass filters
    static constexpr int NUM_COMBS = 4;
    static constexpr int NUM_ALLPASS = 2;

    struct CombFilter {
        std::vector<float> buffer;
        int write_pos = 0;
        float feedback = 0.0f;
        float lp_state = 0.0f;
        float damp = 0.5f;
    };

    struct AllpassFilter {
        std::vector<float> buffer;
        int write_pos = 0;
        float feedback = 0.5f;
    };

    std::array<CombFilter, NUM_COMBS> combs_;
    std::array<AllpassFilter, NUM_ALLPASS> allpasses_;
    // Right-channel filter banks — slightly longer delays for decorrelation
    std::array<CombFilter, NUM_COMBS> combs_r_;
    std::array<AllpassFilter, NUM_ALLPASS> allpasses_r_;

    void init_filters();
};

} // namespace Amplitron
