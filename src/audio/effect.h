#pragma once

#include "common.h"
#include <cstring>

namespace Amplitron {

// Runtime-editable effect parameter exposed to the UI and preset system.
struct EffectParam {
    std::string name;
    float value;
    float min_val;
    float max_val;
    float default_val;
    std::string unit;
    std::string tooltip;
};

// Common interface for all mono/stereo audio effects in the pedal chain.
class Effect {
public:
    virtual ~Effect() = default;

    // Process a mono buffer in place.
    virtual void process(float* buffer, int num_samples) = 0;

    // Stereo processing. Default fans mono left channel to both outputs.
    // Stereo-capable effects override this to produce true stereo.
    virtual void process_stereo(float* left, float* right, int num_samples) {
        process(left, num_samples);
        std::memcpy(right, left, static_cast<size_t>(num_samples) * sizeof(float));
    }

    // Update the processing sample rate before audio starts or after device changes.
    virtual void set_sample_rate(int sample_rate) { sample_rate_ = sample_rate; }

    // Clear delay lines, envelopes, filters, and other effect state.
    virtual void reset() = 0;

    // Display name used by the pedal board and preset serialization.
    virtual const char* name() const = 0;

    // Mutable parameter list used by controls and automation.
    virtual std::vector<EffectParam>& params() = 0;

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    void set_mix(float mix) { mix_ = clamp(mix, 0.0f, 1.0f); }
    float get_mix() const { return mix_; }

protected:
    int sample_rate_ = DEFAULT_SAMPLE_RATE;
    bool enabled_ = true;
    float mix_ = 1.0f;

    // Wet/dry mix helper
    void apply_mix(const float* dry, float* wet, int num_samples) {
        if (mix_ >= 1.0f) return;
        for (int i = 0; i < num_samples; ++i) {
            wet[i] = dry[i] * (1.0f - mix_) + wet[i] * mix_;
        }
    }
};

} // namespace Amplitron
