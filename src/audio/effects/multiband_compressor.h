#pragma once

#include "audio/effect.h"
#include "audio/dsp/envelope_follower.h"
#include "audio/dsp/biquad.h"
#include <atomic>

namespace Amplitron {

/**
 * @brief 2nd-order Butterworth Low-Pass filter stage.
 * Two cascaded stages form a Linkwitz-Riley 4th-order (LR4) filter.
 */
struct ButterworthLP {
    Biquad b1, b2;

    void calculate_coefficients(float cutoff, float sample_rate) {
        float w0 = TWO_PI * cutoff / sample_rate;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * 0.70710678f); // Q = 1/sqrt(2)
        float a0 = 1.0f + alpha;

        float b0 = (1.0f - cos_w0) / 2.0f;
        float b1_val = 1.0f - cos_w0;
        float b2_val = (1.0f - cos_w0) / 2.0f;
        float a1 = -2.0f * cos_w0;
        float a2 = 1.0f - alpha;

        b1.b0 = b2.b0 = b0 / a0;
        b1.b1 = b2.b1 = b1_val / a0;
        b1.b2 = b2.b2 = b2_val / a0;
        b1.a1 = b2.a1 = a1 / a0;
        b1.a2 = b2.a2 = a2 / a0;
    }

    float process(float x) {
        return b2.process(b1.process(x));
    }

    void reset() {
        b1.reset();
        b2.reset();
    }
};

/**
 * @brief 2nd-order Butterworth High-Pass filter stage.
 * Two cascaded stages form a Linkwitz-Riley 4th-order (LR4) filter.
 */
struct ButterworthHP {
    Biquad b1, b2;

    void calculate_coefficients(float cutoff, float sample_rate) {
        float w0 = TWO_PI * cutoff / sample_rate;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * 0.70710678f); // Q = 1/sqrt(2)
        float a0 = 1.0f + alpha;

        float b0 = (1.0f + cos_w0) / 2.0f;
        float b1_val = -(1.0f + cos_w0);
        float b2_val = (1.0f + cos_w0) / 2.0f;
        float a1 = -2.0f * cos_w0;
        float a2 = 1.0f - alpha;

        b1.b0 = b2.b0 = b0 / a0;
        b1.b1 = b2.b1 = b1_val / a0;
        b1.b2 = b2.b2 = b2_val / a0;
        b1.a1 = b2.a1 = a1 / a0;
        b1.a2 = b2.a2 = a2 / a0;
    }

    float process(float x) {
        return b2.process(b1.process(x));
    }

    void reset() {
        b1.reset();
        b2.reset();
    }
};

/**
 * @brief A professional 3-band dynamics compressor.
 * Crossover filters split the signal into Low, Mid, and High bands.
 * Each band is compressed independently and summed back to the output.
 */
class MultiBandCompressor : public Effect {
public:
    MultiBandCompressor();
    void process(float* buffer, int num_samples) override;
    void reset() override;
    const char* name() const override { return "MultiBand Compressor"; }
    std::vector<EffectParam>& params() override { return params_; }
    void set_sample_rate(int sample_rate) override;

    // Thread-safe gain reduction query for the GUI meters
    float get_gain_reduction_db(int band) const {
        if (band < 0 || band >= 3) return 0.0f;
        return gain_reduction_db_[band].load(std::memory_order_relaxed);
    }

private:
    std::vector<EffectParam> params_;

    // Crossover filters (LR4 crossovers)
    ButterworthLP low_pass_filter_;
    ButterworthHP low_high_pass_filter_;
    ButterworthLP mid_pass_filter_;
    ButterworthHP mid_high_pass_filter_;

    // Per-band compressors
    EnvelopeFollower env_[3];
    float smoothed_attack_ms_[3] = {5.0f, 5.0f, 5.0f};
    float smoothed_release_ms_[3] = {100.0f, 100.0f, 100.0f};

    // Thread-safe gain reduction tracking (instantly updated)
    std::atomic<float> gain_reduction_db_[3];

    // Peak-decay envelope for smoother GUI meter visual response
    float gr_peak_[3] = {0.0f, 0.0f, 0.0f};

    // Cached states to determine when to recompute crossover coefficients
    float cached_low_xover_ = -1.0f;
    float cached_high_xover_ = -1.0f;
    int cached_sample_rate_ = -1;

    void recompute_coefficients();
};

} // namespace Amplitron
