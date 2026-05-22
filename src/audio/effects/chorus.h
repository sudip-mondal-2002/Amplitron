#pragma once

// Modulated delay effect that thickens the signal with chorus movement.
// The wet tap is y_w[n] = x[n - D(n)] with D(n)=D0 + A*sin(2*pi*f_lfo*n/Fs);
// fractional delay reads use interpolation, then y[n]=(1-mix)*x[n]+mix*y_w[n].

#include "audio/effect.h"

namespace Amplitron {

class Chorus : public Effect {
public:
    Chorus();
    void process(float* buffer, int num_samples) override;
    void process_stereo(float* left, float* right, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void set_transport_state(float bpm) override;
    void reset() override;
    const char* name() const override { return "Chorus"; }
    const char* type_id() const override { return "Chorus"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;
    std::vector<float> delay_buffer_;
    int write_pos_ = 0;
    float lfo_phase_ = 0.0f;
    int max_delay_samples_ = 0;
    float last_bpm_=0.0f; //shortcut if bpm not changed.
    float smoothed_rate_ = 1.5f;
};

} // namespace Amplitron
