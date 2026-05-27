#include "audio/dsp/level_analyzer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>

namespace Amplitron {

namespace {
inline bool is_invalid_rms(float x) {
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof(float));
    volatile uint32_t vbits = bits; // Bypass -ffast-math optimizations
    if ((vbits & 0x7F800000) == 0x7F800000) {
        return true; // NaN or Infinity
    }
    return x < 0.0f;
}

inline bool is_invalid_dt(float x) {
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof(float));
    volatile uint32_t vbits = bits; // Bypass -ffast-math optimizations
    if ((vbits & 0x7F800000) == 0x7F800000) {
        return true; // NaN or Infinity
    }
    return x <= 0.0f;
}
} // namespace

void LevelAnalyzer::update(float input_rms, float output_rms, bool input_clipped, bool output_clipped, float dt) {
    if (is_invalid_dt(dt) || is_invalid_rms(input_rms) || is_invalid_rms(output_rms)) {
        return;
    }

    // RMS smoothing
    smoothed_input_rms_ += (input_rms - smoothed_input_rms_) * 0.22f;
    smoothed_output_rms_ += (output_rms - smoothed_output_rms_) * 0.22f;

    // Peak hold decay
    const float peak_decay = 0.45f;
    input_peak_hold_ = std::max(smoothed_input_rms_, input_peak_hold_ - peak_decay * dt);
    output_peak_hold_ = std::max(smoothed_output_rms_, output_peak_hold_ - peak_decay * dt);

    // Clip flash indicator decays
    if (input_clipped) {
        input_clip_flash_ = 1.0f;
    }
    if (output_clipped) {
        output_clip_flash_ = 1.0f;
    }

    input_clip_flash_ = std::max(0.0f, input_clip_flash_ - dt * 2.0f);
    output_clip_flash_ = std::max(0.0f, output_clip_flash_ - dt * 2.0f);
}

} // namespace Amplitron
