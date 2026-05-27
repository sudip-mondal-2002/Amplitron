#pragma once

#include "common.h"
#include <array>
#include <complex>

namespace Amplitron {

/**
 * @brief Performs Fast Fourier Transform (FFT) and frequency magnitude calculations.
 * Completely decoupled from GUI rendering libraries (ImGui/SDL).
 */
class SpectrumAnalyzer {
public:
    static constexpr int FFT_SIZE = 2048;
    static constexpr int FFT_BINS = FFT_SIZE / 2;
    static constexpr int DISPLAY_BARS = 96;

    SpectrumAnalyzer();

    /**
     * @brief Perform FFT analysis and update the smoothed frequency DB arrays.
     */
    void update(const float* input_samples,
                const float* output_samples,
                int sample_rate,
                float dt_seconds);

    // Getters for display data (so the UI only reads raw arrays)
    const std::array<float, DISPLAY_BARS>& smoothed_input_db() const { return smoothed_input_db_; }
    const std::array<float, DISPLAY_BARS>& smoothed_output_db() const { return smoothed_output_db_; }
    const std::array<float, DISPLAY_BARS>& input_peak_db() const { return input_peak_db_; }
    const std::array<float, DISPLAY_BARS>& output_peak_db() const { return output_peak_db_; }

private:
    void compute_spectrum_bars(const float* samples,
                               int sample_rate,
                               std::array<float, DISPLAY_BARS>& bars_db);
    void run_fft(std::array<std::complex<float>, FFT_SIZE>& data) const;

    std::array<float, FFT_SIZE> window_{};
    std::array<std::complex<float>, FFT_SIZE> fft_work_{};

    std::array<float, DISPLAY_BARS> smoothed_input_db_{};
    std::array<float, DISPLAY_BARS> smoothed_output_db_{};
    std::array<float, DISPLAY_BARS> input_peak_db_{};
    std::array<float, DISPLAY_BARS> output_peak_db_{};
};

} // namespace Amplitron
