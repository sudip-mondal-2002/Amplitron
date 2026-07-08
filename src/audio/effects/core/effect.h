#pragma once

#include <atomic>
#include <cstring>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

#include "audio/effects/core/effect_param.h"
#include "audio/effects/core/i_metadata.h"
#include "audio/effects/core/i_parameterizable.h"
#include "audio/effects/core/i_processor.h"
#include "audio/effects/core/i_serializable.h"
#include "common.h"

namespace Amplitron {

// Common interface for all mono/stereo audio effects in the pedal chain.
class Effect : public IProcessor, public IParameterizable, public ISerializable, public IMetadata {
   public:
    virtual ~Effect() = default;

    // Process a mono buffer in place.
    virtual void process(float* buffer, int num_samples) override = 0;

    // Stereo processing. Default fans mono left channel to both outputs.
    // Stereo-capable effects override this to produce true stereo.
    virtual void process_stereo(float* left, float* right, int num_samples) override {
        process(left, num_samples);
        std::memcpy(right, left, static_cast<size_t>(num_samples) * sizeof(float));
    }

    // Update the processing sample rate before audio starts or after device changes.
    virtual void set_sample_rate(int sample_rate) override { sample_rate_ = sample_rate; }

    // Clear delay lines, envelopes, filters, and other effect state.
    virtual void reset() override = 0;

    // Tempo broadcast receiver.
    virtual void set_transport_state(float /*bpm*/) {}

    // Display name used by the pedal board and preset serialization.
    virtual const char* name() const override = 0;

    virtual const char* type_id() const override { return name(); }

    // Mutable parameter list used by controls and automation.
    virtual std::vector<EffectParam>& params() override = 0;
    virtual const std::vector<EffectParam>& params() const override = 0;

    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    void set_mix(float mix) { mix_.store(clamp(mix, 0.0f, 1.0f), std::memory_order_relaxed); }
    float get_mix() const { return mix_.load(std::memory_order_relaxed); }

    virtual std::shared_ptr<Effect> clone() const;

    std::vector<std::string> get_param_names() override {
        std::vector<std::string> names;
        for (const auto& p : params()) {
            names.push_back(p.name);
        }
        return names;
    }

    float get_param_value(const std::string& name) override {
        for (const auto& p : params()) {
            if (p.name == name) {
                return p.value;
            }
        }
        return 0.0f;
    }

    void set_param_by_name(const std::string& name, float value) override {
        for (auto& p : params()) {
            if (p.name == name) {
                p.value = clamp(value, p.min_val, p.max_val);
                return;
            }
        }
    }

    virtual const char* get_display_name() const override { return name(); }

    // --- AUTOMATED SERIALIZATION LOGIC ---
    // These methods automatically handle saving/loading for any effect
    // that uses the EffectParam vector.

    virtual nlohmann::json get_params() const override;
    virtual void set_params(const nlohmann::json& j) override;

   protected:
    int sample_rate_ = DEFAULT_SAMPLE_RATE;
    std::atomic<bool> enabled_{true};
    std::atomic<float> mix_{1.0f};

    // Wet/dry mix helper
    void apply_mix(const float* dry, float* wet, int num_samples) {
        float current_mix = mix_.load(std::memory_order_relaxed);
        if (current_mix >= 1.0f) return;
        for (int i = 0; i < num_samples; ++i) {
            wet[i] = dry[i] * (1.0f - current_mix) + wet[i] * current_mix;
        }
    }
};

}  // namespace Amplitron
