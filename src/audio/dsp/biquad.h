#pragma once

#include "common.h"

namespace Amplitron {

/**
 * Transposed Direct Form II biquad filter.
 * Used by Equalizer, AmpSimulator, and CabinetSim.
 */
struct Biquad {
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }

    void reset() { x1 = x2 = y1 = y2 = 0; }

    // --- Coefficient computation (Audio EQ Cookbook formulas) ---

    void set_low_shelf(float freq, float gain_db, float q, int sample_rate) {
        float A = std::pow(10.0f, gain_db / 40.0f);
        float w0 = TWO_PI * freq / sample_rate;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * q);
        float sqA = std::sqrt(A);

        float a0 = (A + 1) + (A - 1) * cos_w0 + 2 * sqA * alpha;
        b0 = (A * ((A + 1) - (A - 1) * cos_w0 + 2 * sqA * alpha)) / a0;
        b1 = (2 * A * ((A - 1) - (A + 1) * cos_w0)) / a0;
        b2 = (A * ((A + 1) - (A - 1) * cos_w0 - 2 * sqA * alpha)) / a0;
        a1 = (-2 * ((A - 1) + (A + 1) * cos_w0)) / a0;
        a2 = ((A + 1) + (A - 1) * cos_w0 - 2 * sqA * alpha) / a0;
    }

    void set_peaking(float freq, float gain_db, float q, int sample_rate) {
        float A = std::pow(10.0f, gain_db / 40.0f);
        float w0 = TWO_PI * freq / sample_rate;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * q);

        float a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0;
        b1 = (-2 * cos_w0) / a0;
        b2 = (1 - alpha * A) / a0;
        a1 = (-2 * cos_w0) / a0;
        a2 = (1 - alpha / A) / a0;
    }

    void set_high_shelf(float freq, float gain_db, float q, int sample_rate) {
        float A = std::pow(10.0f, gain_db / 40.0f);
        float w0 = TWO_PI * freq / sample_rate;
        float cos_w0 = std::cos(w0);
        float sin_w0 = std::sin(w0);
        float alpha = sin_w0 / (2.0f * q);
        float sqA = std::sqrt(A);

        float a0 = (A + 1) - (A - 1) * cos_w0 + 2 * sqA * alpha;
        b0 = (A * ((A + 1) + (A - 1) * cos_w0 + 2 * sqA * alpha)) / a0;
        b1 = (-2 * A * ((A - 1) + (A + 1) * cos_w0)) / a0;
        b2 = (A * ((A + 1) + (A - 1) * cos_w0 - 2 * sqA * alpha)) / a0;
        a1 = (2 * ((A - 1) - (A + 1) * cos_w0)) / a0;
        a2 = ((A + 1) - (A - 1) * cos_w0 - 2 * sqA * alpha) / a0;
    }
};

/**
 * One-pole low-pass filter for tone controls and DC blocking.
 */
struct OnePole {
    float state = 0.0f;

    float lp(float input, float coeff) {
        state += coeff * (input - state);
        return state;
    }

    float hp(float input, float coeff) {
        state += coeff * (input - state);
        return input - state;
    }

    void reset() { state = 0.0f; }
};

}  // namespace Amplitron
