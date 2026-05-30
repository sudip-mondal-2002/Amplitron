#include "audio/effects/amp_simulator.h"
#include "audio/effects/effect_factory.h"

namespace Amplitron {

static EffectRegistrar<AmpSimulator> reg("Amp Sim");

// ============================================================
// Factory amp model library
// ============================================================

const std::vector<AmpModel>& get_amp_models() {
    static const std::vector<AmpModel> models = {
        // ---------------------------------------------------------
        // Clean American -- Fender Twin/Deluxe
        // ---------------------------------------------------------
        {
            "Clean American",
            "Fender Twin / Deluxe",
            "Sparkling clean, scooped mids, glassy highs",
            200.0f, 3.0f, 0.7f,
            800.0f, -2.0f, 0.8f,
            3500.0f, 2.5f, 0.7f,
            1.2f, 0.0f, 1.0f, 0.85f,
            0.01f, 0.005f, 0.0f,
        },
        // ---------------------------------------------------------
        // British Crunch -- Marshall JCM800
        // ---------------------------------------------------------
        {
            "British Crunch",
            "Marshall JCM800",
            "Warm breakup, mid-forward, classic rock crunch",
            180.0f, -1.0f, 0.8f,
            650.0f, 4.0f, 1.2f,
            3000.0f, 1.5f, 0.7f,
            3.5f, 0.15f, 0.8f, 0.7f,
            0.05f, 0.008f, 0.15f,
        },
        // ---------------------------------------------------------
        // High Gain Modern -- Mesa Boogie Rectifier
        // ---------------------------------------------------------
        {
            "High Gain Modern",
            "Mesa Boogie Rectifier",
            "Tight low-end, scooped mids, aggressive distortion",
            150.0f, 1.5f, 1.0f,
            500.0f, -4.5f, 0.9f,
            4000.0f, 2.0f, 0.8f,
            8.0f, 0.7f, 0.9f, 0.55f,
            0.15f, 0.003f, 0.25f,
        },
        // ---------------------------------------------------------
        // Jazz Warm -- Roland JC-120
        // ---------------------------------------------------------
        {
            "Jazz Warm",
            "Roland JC-120",
            "Flat clean, warm highs rolloff, round tone",
            250.0f, 2.0f, 0.6f,
            700.0f, 0.5f, 0.7f,
            2800.0f, -3.5f, 0.6f,
            1.0f, 0.0f, 1.0f, 0.9f,
            0.005f, 0.003f, 0.0f,
        },
    };
    return models;
}

// ============================================================
// AmpSimulator implementation
// ============================================================

AmpSimulator::AmpSimulator() {
    float max_model = static_cast<float>(get_amp_models().size() - 1);
    params_ = {
        {"Model", 0.0f, 0.0f, max_model, 0.0f, "", "Selects the amplifier model. Each model has unique EQ curves, gain staging, and clipping characteristics."},
        {"Gain",  0.5f, 0.0f, 1.0f, 0.5f, "", "Preamp gain control. Drives the virtual tubes harder for more compression and distortion."},
        {"Bass",  0.0f, -6.0f, 6.0f, 0.0f, "dB", "Pre-distortion low-frequency trim. Adjusts the fatness and punch of the amplifier."},
        {"Mid",   0.0f, -6.0f, 6.0f, 0.0f, "dB", "Pre-distortion mid-frequency trim. Controls the core voice and 'bark' of the amplifier."},
        {"Treble", 0.0f, -6.0f, 6.0f, 0.0f, "dB", "Pre-distortion high-frequency trim. Adjusts the brightness and bite before clipping."},
        {"Level", 0.7f, 0.0f, 1.0f, 0.7f, "", "Master output volume of the amplifier. Does not affect the amount of distortion."},
    };
    set_sample_rate(DEFAULT_SAMPLE_RATE);
}

void AmpSimulator::set_sample_rate(int sample_rate) {
    Effect::set_sample_rate(sample_rate);
    // Snap smoothing states to current params to avoid transient on rate change
    bass_trim_state_ = params_[2].value;
    mid_trim_state_ = params_[3].value;
    treble_trim_state_ = params_[4].value;
    gain_smoothed_   = params_[1].value;
    level_smoothed_  = params_[5].value;
    cached_model_index_ = -1; // force recompute
    recompute_coefficients_if_dirty();
}

void AmpSimulator::recompute_coefficients_if_dirty() {
    int model_idx = clamp(static_cast<int>(params_[0].value + 0.5f),
                          0, static_cast<int>(get_amp_models().size()) - 1);
    float bass_trim = bass_trim_state_;
    float mid_trim = mid_trim_state_;
    float treble_trim = treble_trim_state_;
    float gain_knob = params_[1].value;

    if (model_idx != cached_model_index_ ||
        bass_trim != cached_bass_ ||
        mid_trim != cached_mid_ ||
        treble_trim != cached_treble_ ||
        gain_knob != cached_gain_) {

        const AmpModel& model = get_amp_models()[model_idx];
        low_shelf_.set_low_shelf(model.bass_freq, model.bass_gain_db + bass_trim, model.bass_q, sample_rate_);
        mid_peak_.set_peaking(model.mid_freq, model.mid_gain_db + mid_trim, model.mid_q, sample_rate_);
        high_shelf_.set_high_shelf(model.treble_freq, model.treble_gain_db + treble_trim, model.treble_q, sample_rate_);

        cached_model_index_ = model_idx;
        cached_bass_ = bass_trim;
        cached_mid_ = mid_trim;
        cached_treble_ = treble_trim;
        cached_gain_ = gain_knob;
    }
}

void AmpSimulator::process(float* buffer, int num_samples) {
    if (!enabled_) return;

    // One-pole smoothing: advance trim states toward raw param targets each block
    const float alpha = 1.0f - std::exp(-1.0f / (sample_rate_ * 0.010f)); // 10 ms
    bass_trim_state_   += alpha * (params_[2].value - bass_trim_state_);
    mid_trim_state_    += alpha * (params_[3].value - mid_trim_state_);
    treble_trim_state_ += alpha * (params_[4].value - treble_trim_state_);
    gain_smoothed_     += alpha * (params_[1].value - gain_smoothed_);
    level_smoothed_    += alpha * (params_[5].value - level_smoothed_);

    recompute_coefficients_if_dirty();

    int model_idx = clamp(static_cast<int>(params_[0].value + 0.5f),
                          0, static_cast<int>(get_amp_models().size()) - 1);
    const AmpModel& model = get_amp_models()[model_idx];

    float gain_knob = gain_smoothed_;
    float level = level_smoothed_;

    // Effective preamp gain: model base * user gain control (0-2x range)
    float effective_gain = model.preamp_gain * (0.2f + gain_knob * 1.8f);
    float sat_mix = model.saturation_mix;
    float asym = model.asymmetry;
    float model_output = model.output_level;
    float attack = model.attack_coeff;
    float release = model.release_coeff;
    float sag = model.sag_amount;

    for (int i = 0; i < num_samples; ++i) {
        float dry = buffer[i];
        float x = buffer[i];

        // --- Envelope follower for dynamic response ---
        float abs_x = std::fabs(x);
        if (abs_x > envelope_) {
            envelope_ += attack * (abs_x - envelope_);
        } else {
            envelope_ += release * (abs_x - envelope_);
        }

        // Power sag: reduce gain when envelope is high (tube amp compression)
        float sag_factor = 1.0f - sag * clamp(envelope_, 0.0f, 1.0f);

        // --- Input gain with sag ---
        x *= effective_gain * sag_factor;

        // --- Tone stack (pre-saturation EQ) ---
        x = low_shelf_.process(x);
        x = mid_peak_.process(x);
        x = high_shelf_.process(x);

        // --- Waveshaping saturation ---
        // Soft clipping path (tube-like)
        float soft;
        if (x > 0.0f) {
            soft = 1.0f - std::exp(-x);
        } else {
            soft = -1.0f + std::exp(x);
            soft *= asym;
        }

        // Hard clipping path
        float hard = hard_clip(x, 1.0f);

        // Blend soft and hard clipping
        x = soft * (1.0f - sat_mix) + hard * sat_mix;

        // --- DC blocking high-pass filter ---
        x = dc_block_.hp(x, 0.005f);

        // --- Output level ---
        x *= model_output * level;

        // Safety clamp
        x = clamp(x, -1.0f, 1.0f);

        // Wet/dry mix
        buffer[i] = dry * (1.0f - mix_) + x * mix_;
    }
}

void AmpSimulator::reset() {
    low_shelf_.reset();
    mid_peak_.reset();
    high_shelf_.reset();
    envelope_ = 0.0f;
    dc_block_.reset();
}

} // namespace Amplitron
