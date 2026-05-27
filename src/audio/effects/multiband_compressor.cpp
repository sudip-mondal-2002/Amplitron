#include "audio/effects/multiband_compressor.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<MultiBandCompressor> reg("MultiBand Compressor");

MultiBandCompressor::MultiBandCompressor() {
    params_ = {
        // Crossovers
        {"Low XOver",   200.0f,   50.0f,  1000.0f,   200.0f, "Hz", "Crossover frequency separating the Low and Mid frequency bands."},
        {"High XOver", 4000.0f, 1000.0f, 15000.0f,  4000.0f, "Hz", "Crossover frequency separating the Mid and High frequency bands."},

        // Low band compressor
        {"Low Thresh",  -20.0f,  -60.0f,     0.0f,   -20.0f, "dB", "Low band threshold: volume level above which compression occurs."},
        {"Low Ratio",     4.0f,    1.0f,    20.0f,     4.0f, ":1", "Low band ratio: compression strength for the Low band."},
        {"Low Attack",    5.0f,    0.1f,    50.0f,     5.0f, "ms", "Low band attack: how quickly the compressor acts on Low transients."},
        {"Low Release", 100.0f,   10.0f,   500.0f,   100.0f, "ms", "Low band release: recovery speed of the Low band compressor."},
        {"Low Makeup",    0.0f,    0.0f,    30.0f,     0.0f, "dB", "Low band makeup gain: volume boost applied to the compressed Low band."},

        // Mid band compressor
        {"Mid Thresh",  -20.0f,  -60.0f,     0.0f,   -20.0f, "dB", "Mid band threshold: volume level above which compression occurs."},
        {"Mid Ratio",     4.0f,    1.0f,    20.0f,     4.0f, ":1", "Mid band ratio: compression strength for the Mid band."},
        {"Mid Attack",    5.0f,    0.1f,    50.0f,     5.0f, "ms", "Mid band attack: how quickly the compressor acts on Mid transients."},
        {"Mid Release", 100.0f,   10.0f,   500.0f,   100.0f, "ms", "Mid band release: recovery speed of the Mid band compressor."},
        {"Mid Makeup",    0.0f,    0.0f,    30.0f,     0.0f, "dB", "Mid band makeup gain: volume boost applied to the compressed Mid band."},

        // High band compressor
        {"High Thresh", -20.0f,  -60.0f,     0.0f,   -20.0f, "dB", "High band threshold: volume level above which compression occurs."},
        {"High Ratio",     4.0f,    1.0f,    20.0f,     4.0f, ":1", "High band ratio: compression strength for the High band."},
        {"High Attack",    5.0f,    0.1f,    50.0f,     5.0f, "ms", "High band attack: how quickly the compressor acts on High transients."},
        {"High Release", 100.0f,   10.0f,   500.0f,   100.0f, "ms", "High band release: recovery speed of the High band compressor."},
        {"High Makeup",   0.0f,    0.0f,    30.0f,     0.0f, "dB", "High band makeup gain: volume boost applied to the compressed High band."},

        // Global Output
        {"Out Gain",      0.0f,  -20.0f,    20.0f,     0.0f, "dB", "Global output gain applied to the final summed signal."}
    };

    gain_reduction_db_[0].store(0.0f);
    gain_reduction_db_[1].store(0.0f);
    gain_reduction_db_[2].store(0.0f);

    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void MultiBandCompressor::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    recompute_coefficients();
}

void MultiBandCompressor::recompute_coefficients() {
    float f_low = params_[0].value;
    float f_high = params_[1].value;

    // Enforce that f_low < f_high logically
    if (f_low >= f_high) {
        f_low = f_high - 10.0f;
        if (f_low < params_[0].min_val) {
            f_low = params_[0].min_val;
            f_high = f_low + 10.0f;
        }
    }

    low_pass_filter_.calculate_coefficients(f_low, sample_rate_);
    low_high_pass_filter_.calculate_coefficients(f_low, sample_rate_);

    mid_pass_filter_.calculate_coefficients(f_high, sample_rate_);
    mid_high_pass_filter_.calculate_coefficients(f_high, sample_rate_);
}

void MultiBandCompressor::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    // Crossover frequencies
    float f_low = params_[0].value;
    float f_high = params_[1].value;

    // Check if crossover filters need to recompute coefficients (thread-safe local recompute)
    if (f_low != cached_low_xover_ || f_high != cached_high_xover_ || sample_rate_ != cached_sample_rate_) {
        recompute_coefficients();
        cached_low_xover_ = f_low;
        cached_high_xover_ = f_high;
        cached_sample_rate_ = sample_rate_;
    }

    // Extract per-band compressor parameters
    float threshold_db[3] = { params_[2].value, params_[7].value, params_[12].value };
    float ratio[3]        = { params_[3].value, params_[8].value, params_[13].value };
    float attack_ms[3]    = { params_[4].value, params_[9].value, params_[14].value };
    float release_ms[3]   = { params_[5].value, params_[10].value, params_[15].value };
    float makeup[3]       = { db_to_linear(params_[6].value), db_to_linear(params_[11].value), db_to_linear(params_[16].value) };
    float output_gain     = db_to_linear(params_[17].value);

    // Apply attack/release parameter smoothing to avoid pops/clicks during UI drags
    // Keep smoothing short to reduce zipper noise while minimizing parameter-lag audibility.
    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.002f)); // 2 ms smoothing time constant
    for (int b = 0; b < 3; ++b) {
        smoothed_attack_ms_[b]  += alpha * (attack_ms[b]  - smoothed_attack_ms_[b]);
        smoothed_release_ms_[b] += alpha * (release_ms[b] - smoothed_release_ms_[b]);
    }

    // Calculate filter coefficients for envelope followers
    float attack_coeff[3], release_coeff[3];
    for (int b = 0; b < 3; ++b) {
        attack_coeff[b]  = EnvelopeFollower::time_to_coeff(smoothed_attack_ms_[b], sample_rate_);
        release_coeff[b] = EnvelopeFollower::time_to_coeff(smoothed_release_ms_[b], sample_rate_);
    }

    // Crossover processing and dynamic range compression
    for (int i = 0; i < num_samples; ++i) {
        float x = buffer[i];

        // 1. Split band via subtractive crossovers
        float x_low = low_pass_filter_.process(x);
        float x_high = mid_high_pass_filter_.process(x);
        float x_mid = x - x_low - x_high;

        // 2. Compute envelope followers per band (using process_additive matching compressor)
        float env_val[3];
        env_val[0] = env_[0].process_additive(x_low, attack_coeff[0], release_coeff[0]);
        env_val[1] = env_[1].process_additive(x_mid, attack_coeff[1], release_coeff[1]);
        env_val[2] = env_[2].process_additive(x_high, attack_coeff[2], release_coeff[2]);

        // 3. Gain reduction computation
        float gain_db[3] = { 0.0f, 0.0f, 0.0f };
        for (int b = 0; b < 3; ++b) {
            float env_db = linear_to_db(env_val[b]);
            if (env_db > threshold_db[b]) {
                gain_db[b] = (threshold_db[b] - env_db) * (1.0f - 1.0f / ratio[b]);
            }
        }

        // 4. Update responsive gain reduction meter envelopes for UI
        for (int b = 0; b < 3; ++b) {
            float gr_instant = -gain_db[b]; // non-negative value
            if (gr_instant > gr_peak_[b]) {
                gr_peak_[b] = gr_instant; // instant attack
            } else {
                // Smooth decay visual (decay time constant: ~150 ms)
                float decay_coeff = std::exp(-1.0f / (sample_rate_ * 0.150f));
                gr_peak_[b] = decay_coeff * gr_peak_[b] + (1.0f - decay_coeff) * gr_instant;
            }
            gain_reduction_db_[b].store(gr_peak_[b], std::memory_order_relaxed);
        }

        // 5. Apply compression gain and makeup gain per band
        float y_low  = x_low  * db_to_linear(gain_db[0]) * makeup[0];
        float y_mid  = x_mid  * db_to_linear(gain_db[1]) * makeup[1];
        float y_high = x_high * db_to_linear(gain_db[2]) * makeup[2];

        // 6. Recombine signal and apply global output gain
        buffer[i] = (y_low + y_mid + y_high) * output_gain;
    }
}

void MultiBandCompressor::reset() {
    low_pass_filter_.reset();
    low_high_pass_filter_.reset();
    mid_pass_filter_.reset();
    mid_high_pass_filter_.reset();

    for (int b = 0; b < 3; ++b) {
        env_[b].reset();
        gr_peak_[b] = 0.0f;
        gain_reduction_db_[b].store(0.0f);
    }
}

} // namespace Amplitron
