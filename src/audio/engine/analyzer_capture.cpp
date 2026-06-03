#include "audio/engine/analyzer_capture.h"
#include <cstring>
#include <algorithm>

namespace Amplitron {

AnalyzerCapture::AnalyzerCapture() {
    capture_input_.fill(0.0f);
    capture_output_.fill(0.0f);
    snapshot_input_.fill(0.0f);
    snapshot_output_.fill(0.0f);
}

void AnalyzerCapture::set_analyzer_enabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_release);
}

bool AnalyzerCapture::is_analyzer_enabled() const {
    return enabled_.load(std::memory_order_acquire);
}

uint64_t AnalyzerCapture::get_analyzer_sequence() const {
    return sequence_.load(std::memory_order_acquire);
}

bool AnalyzerCapture::copy_analyzer_snapshot(float* input_dest, float* output_dest, int sample_count) const {
    if (!input_dest || !output_dest || sample_count <= 0) {
        return false;
    }

    const int count = std::min(sample_count, ANALYZER_FFT_SIZE);
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t seq = sequence_.load(std::memory_order_relaxed);
    if (seq == 0) {
        return false;
    }

    std::memcpy(input_dest, snapshot_input_.data(), static_cast<size_t>(count) * sizeof(float));
    std::memcpy(output_dest, snapshot_output_.data(), static_cast<size_t>(count) * sizeof(float));
    return true;
}

void AnalyzerCapture::capture_input(const float* input, int count) {
    if (!enabled_.load(std::memory_order_relaxed)) return;

    int cap = capture_index_;
    for (int i = 0; i < count; ++i) {
        capture_input_[cap] = input[i];
        cap = (cap + 1) & ANALYZER_FFT_MASK;
    }
    capture_index_ = cap;
}

void AnalyzerCapture::capture_output(const float* output, int count) {
    if (!enabled_.load(std::memory_order_relaxed)) return;

    int cap = (capture_index_ - count) & ANALYZER_FFT_MASK;
    for (int i = 0; i < count; ++i) {
        capture_output_[cap] = output[i];
        cap = (cap + 1) & ANALYZER_FFT_MASK;
    }

    samples_since_publish_ += count;
    if (samples_since_publish_ >= ANALYZER_HOP_SIZE) {
        if (mutex_.try_lock()) {
            const int start = capture_index_;
            const int first_chunk = ANALYZER_FFT_SIZE - start;
            std::memcpy(snapshot_input_.data(),
                        capture_input_.data() + start,
                        static_cast<size_t>(first_chunk) * sizeof(float));
            std::memcpy(snapshot_input_.data() + first_chunk,
                        capture_input_.data(),
                        static_cast<size_t>(start) * sizeof(float));
            std::memcpy(snapshot_output_.data(),
                        capture_output_.data() + start,
                        static_cast<size_t>(first_chunk) * sizeof(float));
            std::memcpy(snapshot_output_.data() + first_chunk,
                        capture_output_.data(),
                        static_cast<size_t>(start) * sizeof(float));
            sequence_.fetch_add(1, std::memory_order_release);
            samples_since_publish_ = 0;
            mutex_.unlock();
        }
    }
}

} // namespace Amplitron
