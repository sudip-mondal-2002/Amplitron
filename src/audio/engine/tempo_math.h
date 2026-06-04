#pragma once
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace Amplitron {

class TapTempo {
public:
    void tap(std::chrono::steady_clock::time_point now) {
        // If last tap was more than 4 seconds ago, clear history
        if (!history_.empty()) {
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - history_.back()).count();
            if (diff > 4000) {
                history_.clear();
            }
        }
        history_.push_back(now);
        if (history_.size() > 9) { // Keep up to 9 taps (8 intervals)
            history_.erase(history_.begin());
        }
    }

    void reset() {
        history_.clear();
    }

    bool has_enough_taps() const {
        return history_.size() >= 2;
    }

    float get_bpm(std::chrono::steady_clock::time_point now) {
        if (!history_.empty()) {
            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - history_.back()).count();
            if (diff > 4000) {
                history_.clear();
            }
        }

        if (history_.size() < 2) {
            return -1.0f;
        }

        std::vector<float> intervals;
        intervals.reserve(history_.size() - 1);
        for (size_t i = 1; i < history_.size(); ++i) {
            float ms = std::chrono::duration<float, std::milli>(history_[i] - history_[i - 1]).count();
            intervals.push_back(ms);
        }

        // Trimmed mean to reject outliers if we have at least 4 intervals (5 taps)
        float avg_ms = 0.0f;
        if (intervals.size() >= 4) {
            std::sort(intervals.begin(), intervals.end());
            // Remove the smallest and largest intervals
            float sum = 0.0f;
            for (size_t i = 1; i < intervals.size() - 1; ++i) {
                sum += intervals[i];
            }
            avg_ms = sum / (intervals.size() - 2);
        } else {
            float sum = std::accumulate(intervals.begin(), intervals.end(), 0.0f);
            avg_ms = sum / intervals.size();
        }

        if (avg_ms <= 0.0f) return -1.0f;

        float bpm = 60000.0f / avg_ms;
        // Clamp to valid range (40 - 240 BPM)
        if (bpm < 40.0f) bpm = 40.0f;
        if (bpm > 240.0f) bpm = 240.0f;
        return bpm;
    }

private:
    std::vector<std::chrono::steady_clock::time_point> history_;
};

} // namespace Amplitron
