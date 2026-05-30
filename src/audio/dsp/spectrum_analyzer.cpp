#include "audio/dsp/spectrum_analyzer.h"
#include <algorithm>
#include <cmath>

namespace Amplitron {

namespace {

constexpr float kMinHz = 20.0f;
constexpr float kMaxHz = 20000.0f;
constexpr float kMinDb = -90.0f;
constexpr float kMaxDb = 0.0f;

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace

SpectrumAnalyzer::SpectrumAnalyzer() {
    for (int i = 0; i < FFT_SIZE; ++i) {
        const float phase = (TWO_PI * static_cast<float>(i)) / static_cast<float>(FFT_SIZE - 1);
        window_[i] = 0.5f * (1.0f - std::cos(phase));
    }

    smoothed_input_db_.fill(kMinDb);
    smoothed_output_db_.fill(kMinDb);
    input_peak_db_.fill(kMinDb);
    output_peak_db_.fill(kMinDb);
}

void SpectrumAnalyzer::run_fft(std::array<std::complex<float>, FFT_SIZE>& data) const {
    for (int i = 1, j = 0; i < FFT_SIZE; ++i) {
        int bit = FFT_SIZE >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    for (int len = 2; len <= FFT_SIZE; len <<= 1) {
        const float angle = -TWO_PI / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < FFT_SIZE; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            const int half = len >> 1;
            for (int j = 0; j < half; ++j) {
                const std::complex<float> u = data[i + j];
                const std::complex<float> v = data[i + j + half] * w;
                data[i + j] = u + v;
                data[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }
}

void SpectrumAnalyzer::compute_spectrum_bars(const float* samples,
                                             int sample_rate,
                                             std::array<float, DISPLAY_BARS>& bars_db) {
    if (!samples || sample_rate <= 0) {
        bars_db.fill(kMinDb);
        return;
    }

    for (int i = 0; i < FFT_SIZE; ++i) {
        fft_work_[i] = std::complex<float>(samples[i] * window_[i], 0.0f);
    }

    run_fft(fft_work_);

    std::array<float, FFT_BINS> mags_db{};
    mags_db.fill(kMinDb);

    const float norm = 2.0f / static_cast<float>(FFT_SIZE);
    for (int i = 1; i < FFT_BINS; ++i) {
        const float mag = std::abs(fft_work_[i]) * norm;
        mags_db[i] = clamp(20.0f * std::log10(std::max(mag, 1e-8f)), kMinDb, kMaxDb);
    }

    for (int bar = 0; bar < DISPLAY_BARS; ++bar) {
        const float t0 = static_cast<float>(bar) / static_cast<float>(DISPLAY_BARS);
        const float t1 = static_cast<float>(bar + 1) / static_cast<float>(DISPLAY_BARS);
        const float hz0 = std::pow(10.0f, lerp(std::log10(kMinHz), std::log10(kMaxHz), t0));
        const float hz1 = std::pow(10.0f, lerp(std::log10(kMinHz), std::log10(kMaxHz), t1));

        int bin0 = static_cast<int>(hz0 * FFT_SIZE / static_cast<float>(sample_rate));
        int bin1 = static_cast<int>(hz1 * FFT_SIZE / static_cast<float>(sample_rate));
        bin0 = std::max(1, std::min(bin0, FFT_BINS - 1));
        bin1 = std::max(bin0 + 1, std::min(bin1, FFT_BINS));

        float peak_db = kMinDb;
        for (int b = bin0; b < bin1; ++b) {
            peak_db = std::max(peak_db, mags_db[b]);
        }
        bars_db[bar] = peak_db;
    }
}

void SpectrumAnalyzer::update(const float* input_samples,
                              const float* output_samples,
                              int sample_rate,
                              float dt_seconds) {
    std::array<float, DISPLAY_BARS> input_db{};
    std::array<float, DISPLAY_BARS> output_db{};
    compute_spectrum_bars(input_samples, sample_rate, input_db);
    compute_spectrum_bars(output_samples, sample_rate, output_db);

    const float smooth_alpha = 0.26f;
    const float peak_decay_db_per_sec = 30.0f;
    const float decay = peak_decay_db_per_sec * std::max(dt_seconds, 1.0f / 240.0f);

    for (int i = 0; i < DISPLAY_BARS; ++i) {
        smoothed_input_db_[i] = lerp(smoothed_input_db_[i], input_db[i], smooth_alpha);
        smoothed_output_db_[i] = lerp(smoothed_output_db_[i], output_db[i], smooth_alpha);

        input_peak_db_[i] = std::max(smoothed_input_db_[i], input_peak_db_[i] - decay);
        output_peak_db_[i] = std::max(smoothed_output_db_[i], output_peak_db_[i] - decay);
    }
}

} // namespace Amplitron
