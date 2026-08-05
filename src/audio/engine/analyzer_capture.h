#pragma once

#include <array>
#include <atomic>
#include <mutex>

#include "audio/engine/i_audio_engine.h"

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
    static constexpr int MAX_PEDAL_ANALYZERS = 4;

    AnalyzerCapture();
    ~AnalyzerCapture() override = default;

    // IAnalyzerProvider implementation
    void set_analyzer_enabled(bool enabled) override;
    bool is_analyzer_enabled() const override;
    uint64_t get_analyzer_sequence() const override;
    bool copy_analyzer_snapshot(float* input_dest, float* output_dest,
                                int sample_count) const override;
    bool register_pedal_analyzer(int node_id) override;
    void unregister_pedal_analyzer(int node_id) override;
    uint64_t get_pedal_analyzer_sequence(int node_id) const override;
    bool copy_pedal_analyzer_snapshot(int node_id, float* input_dest, float* output_dest,
                                      int sample_count) const override;

    // Audio thread capture methods
    void capture_input(const float* input, int count);
    void capture_output(const float* output, int count);
    void capture_pedal(int node_id, const float* input, const float* output, int count);

   private:
    std::atomic<bool> enabled_{false};

    // Ring buffers (written on audio thread only)
    std::array<float, ANALYZER_FFT_SIZE> capture_input_{};
    std::array<float, ANALYZER_FFT_SIZE> capture_output_{};
    int capture_index_ = 0;
    int samples_since_publish_ = 0;

    static constexpr int SLOT_FREE = -1;
    static constexpr int SLOT_RESERVED = -2;

    // Mutex-protected snapshots
    mutable std::mutex mutex_;
    std::array<float, ANALYZER_FFT_SIZE> snapshot_input_{};
    std::array<float, ANALYZER_FFT_SIZE> snapshot_output_{};
    std::atomic<uint64_t> sequence_{0};

    // Per-pedal analyzer captures
    struct PedalCapture {
        std::atomic<int> node_id_{SLOT_FREE};
        std::atomic<uint32_t> active_writers_{0};
        std::array<float, ANALYZER_FFT_SIZE> capture_input_{};
        std::array<float, ANALYZER_FFT_SIZE> capture_output_{};
        std::atomic<int> capture_index_{0};
        std::atomic<int> samples_since_publish_{0};

        mutable std::mutex mutex_;
        std::array<float, ANALYZER_FFT_SIZE> snapshot_input_{};
        std::array<float, ANALYZER_FFT_SIZE> snapshot_output_{};
        std::atomic<uint64_t> sequence_{0};

        void reset() {
            std::lock_guard<std::mutex> lock(mutex_);
            capture_input_.fill(0.0f);
            capture_output_.fill(0.0f);
            snapshot_input_.fill(0.0f);
            snapshot_output_.fill(0.0f);
            capture_index_.store(0, std::memory_order_relaxed);
            samples_since_publish_.store(0, std::memory_order_relaxed);
            sequence_.store(0, std::memory_order_release);
        }
    };
    std::array<PedalCapture, MAX_PEDAL_ANALYZERS> pedal_captures_{};
};

}  // namespace Amplitron
