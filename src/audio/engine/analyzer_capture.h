#pragma once

#include "audio/engine/i_audio_engine.h"
#include <array>
#include <mutex>
#include <atomic>

namespace Amplitron {

/**
 * @brief Handles real-time capture of input and output signals for visual analyzer.
 * Satisfies Single Responsibility Principle (SRP).
 */
class AnalyzerCapture : public IAnalyzerProvider {
public:
    static constexpr int ANALYZER_FFT_SIZE = 2048;
    static constexpr int ANALYZER_FFT_MASK = ANALYZER_FFT_SIZE - 1;
    static constexpr int ANALYZER_HOP_SIZE = 1024;

    AnalyzerCapture();
    ~AnalyzerCapture() override = default;

    // IAnalyzerProvider implementation
    void set_analyzer_enabled(bool enabled) override;
    bool is_analyzer_enabled() const override;
    uint64_t get_analyzer_sequence() const override;
    bool copy_analyzer_snapshot(float* input_dest, float* output_dest, int sample_count) const override;

    // Audio thread capture methods
    void capture_input(const float* input, int count);
    void capture_output(const float* output, int count);

private:
    std::atomic<bool> enabled_{false};

    // Ring buffers (written on audio thread only)
    std::array<float, ANALYZER_FFT_SIZE> capture_input_{};
    std::array<float, ANALYZER_FFT_SIZE> capture_output_{};
    int capture_index_ = 0;
    int samples_since_publish_ = 0;

    // Mutex-protected snapshots
    mutable std::mutex mutex_;
    std::array<float, ANALYZER_FFT_SIZE> snapshot_input_{};
    std::array<float, ANALYZER_FFT_SIZE> snapshot_output_{};
    std::atomic<uint64_t> sequence_{0};
};

} // namespace Amplitron
