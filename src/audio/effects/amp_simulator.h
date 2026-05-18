#pragma once

// Preamp and tone-stack models for classic guitar amplifier voicings.
// Signal model: y = L * sat(G * H_tone{x}, mix, asymmetry), where H_tone is
// a low-shelf + peaking-mid + high-shelf biquad cascade. The factory models
// cover Clean American / Fender Twin, British Crunch / Marshall JCM800,
// High Gain Modern / Mesa Rectifier, and Jazz Warm / Roland JC-120. Dynamic
// sag follows an envelope e[n] = a*x_abs[n] + (1-a)*e[n-1] and reduces gain
// as the simulated supply is loaded.

#include "audio/effect.h"
#include "audio/dsp/biquad.h"

namespace Amplitron {

/**
 * @brief Describes the tonal character of an amp model.
 *
 * Each model packages a characteristic tone-stack EQ curve, saturation
 * transfer function, and dynamic response into a single struct that the
 * AmpSimulator effect consumes.
 */
struct AmpModel {
    const char* name;          ///< Display name (e.g. "Clean American")
    const char* inspiration;   ///< Real-world amp inspiration
    const char* description;   ///< Short tonal description

    // --- Tone stack (3-band biquad EQ) ---
    float bass_freq;           ///< Low shelf center frequency (Hz)
    float bass_gain_db;        ///< Low shelf gain (dB)
    float bass_q;              ///< Low shelf Q factor
    float mid_freq;            ///< Mid peak center frequency (Hz)
    float mid_gain_db;         ///< Mid peak gain (dB)
    float mid_q;               ///< Mid peak Q factor
    float treble_freq;         ///< High shelf center frequency (Hz)
    float treble_gain_db;      ///< High shelf gain (dB)
    float treble_q;            ///< High shelf Q factor

    // --- Saturation ---
    float preamp_gain;         ///< Pre-saturation drive multiplier
    float saturation_mix;      ///< Blend of soft vs hard clipping [0=soft, 1=hard]
    float asymmetry;           ///< Positive-negative clipping ratio (1.0 = symmetric)
    float output_level;        ///< Post-saturation output scaling

    // --- Dynamic response ---
    float attack_coeff;        ///< Envelope follower attack speed (0-1, higher = faster)
    float release_coeff;       ///< Envelope follower release speed (0-1, higher = faster)
    float sag_amount;          ///< Power-sag simulation depth (0 = none)
};

/**
 * @brief Returns the built-in amp model library.
 * @return Vector of AmpModel structs for all factory amp types.
 */
const std::vector<AmpModel>& get_amp_models();

/**
 * @brief Preamp simulator effect with selectable amp models.
 *
 * Implements a complete preamp stage: input gain -> envelope follower ->
 * tone-stack EQ (3 biquad filters) -> waveshaping saturation -> output level.
 * The tonal character is defined by the selected AmpModel.
 */
class AmpSimulator : public Effect {
public:
    AmpSimulator();
    void process(float* buffer, int num_samples) override;
    void set_sample_rate(int sample_rate) override;
    void reset() override;
    const char* name() const override { return "Amp Sim"; }
    std::vector<EffectParam>& params() override { return params_; }

private:
    std::vector<EffectParam> params_;

    // 3-band tone-stack biquad filters
    Biquad low_shelf_;
    Biquad mid_peak_;
    Biquad high_shelf_;

    // Envelope follower for dynamic response
    float envelope_ = 0.0f;

    // DC blocking high-pass state
    OnePole dc_block_;

    // One-pole smoothing states for trim parameters (avoids coefficient click on UI change)
    float bass_trim_state_ = 0.0f;
    float mid_trim_state_ = 0.0f;
    float treble_trim_state_ = 0.0f;
    float gain_smoothed_ = 0.5f;
    float level_smoothed_ = 0.7f;

    // Cached model index for dirty-check coefficient recomputation
    int cached_model_index_ = -1;
    float cached_bass_ = -999.0f;
    float cached_mid_ = -999.0f;
    float cached_treble_ = -999.0f;
    float cached_gain_ = -999.0f;

    void recompute_coefficients_if_dirty();
};

} // namespace Amplitron
