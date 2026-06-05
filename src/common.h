#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Amplitron {

constexpr int DEFAULT_SAMPLE_RATE = 48000;
constexpr int DEFAULT_BUFFER_SIZE = 64;  // ~1.3ms latency at 48kHz
constexpr int MAX_BUFFER_SIZE = 512;
constexpr int MIN_BUFFER_SIZE = 32;
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

inline float clamp(float value, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, value));
}

inline float db_to_linear(float db) { return std::pow(10.0f, db / 20.0f); }

inline float linear_to_db(float linear) { return 20.0f * std::log10(std::max(linear, 1e-10f)); }

// Fast tanh approximation using Padé approximant.
// ~3× faster than std::tanh(), perceptually identical for musical signals.
inline float fast_tanh(float x) {
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float soft_clip(float x) {
    if (x > 1.0f) return 2.0f / 3.0f;
    if (x < -1.0f) return -2.0f / 3.0f;
    return x - (x * x * x) / 3.0f;
}

inline float hard_clip(float x, float threshold) { return clamp(x, -threshold, threshold); }

// Simple one-pole low-pass filter for parameter smoothing
struct SmoothParam {
    float current = 0.0f;
    float target = 0.0f;
    float coeff = 0.995f;

    void set(float val) { target = val; }
    float next() {
        current += (target - current) * (1.0f - coeff);
        return current;
    }
    void snap() { current = target; }
};

}  // namespace Amplitron
