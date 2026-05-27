#pragma once

// Tempo-independent ring-buffer delay line with feedback and wet/dry control.
// The circular buffer reads d samples behind the write head and writes
// x[n] + feedback*y_delay[n]; output is y[n]=(1-mix)*x[n]+mix*y_delay[n], with
// a one-pole tone filter shaping repeated echoes.

#include "audio/effects/effect.h"
#include "audio/dsp/biquad.h"

namespace Amplitron {

class Delay : public Effect {
public:
    Delay();
    void process(float* buffer, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void set_transport_state(float bpm) override;
    void reset() override;
    const char* name() const override { return "Delay"; }
    const char* type_id() const override { return "Delay"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;
    std::vector<float> delay_buffer_;
    int write_pos_ = 0;
    int max_delay_samples_ = 0;
    OnePole tone_lp_;

    // One-pole smoothed parameter states
    float smoothed_time_ms_ = 350.0f;
    float smoothed_feedback_ = 0.4f;
    float smoothed_tone_ = 0.7f;
    float smoothed_level_ = 0.5f;

    //shortcut if bpm hasn't changed
    float last_bpm_ = 0.0f;
};

} // namespace Amplitron
