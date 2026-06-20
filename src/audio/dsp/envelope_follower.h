#pragma once

#include <cmath>

#include "common.h"

namespace Amplitron {

/**
 * Attack/release envelope follower.
 * Used by Compressor, NoiseGate, WahPedal, and AmpSimulator.
 */
struct EnvelopeFollower {
    float envelope = 0.0f;

    /**
     * Compute attack/release coefficients from time constants.
     * @param ms       Time in milliseconds.
     * @param sr       Sample rate.
     * @return One-pole coefficient (pass to process()).
     */
    static float time_to_coeff(float ms, int sr) { return std::exp(-1.0f / (sr * ms * 0.001f)); }

    /**
     * Process one sample and return the new envelope value.
     * Coefficients are in "smoothing" form: higher = slower.
     */
    float process(float input, float attack_coeff, float release_coeff) {
        float abs_in = std::fabs(input);
        float coeff = (abs_in > envelope) ? attack_coeff : release_coeff;
        envelope = coeff * envelope + (1.0f - coeff) * abs_in;
        return envelope;
    }

    /**
     * Alternative form matching the Compressor's convention:
     * coeff = 1 - exp(...), applied as: envelope += coeff * (abs - envelope)
     */
    float process_additive(float input, float attack_coeff, float release_coeff) {
        float abs_in = std::fabs(input);
        float coeff = (abs_in > envelope) ? (1.0f - attack_coeff) : (1.0f - release_coeff);
        envelope += coeff * (abs_in - envelope);
        return envelope;
    }

    void reset() { envelope = 0.0f; }
};

}  // namespace Amplitron
