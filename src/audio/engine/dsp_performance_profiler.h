#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace Amplitron {

constexpr int DSP_PROFILER_HISTORY_SIZE = 240;
constexpr int MAX_PROFILED_MODULES = 128;

struct DspProfilerModuleSnapshot {
    int index = -1;
    float last_us = 0.0f;
    float average_us = 0.0f;
    float peak_us = 0.0f;
    float budget_percent = 0.0f;
    bool overloaded = false;
};

struct DspProfilerSnapshot {
    int sample_rate = 0;
    int buffer_size = 0;

    float latency_ms = 0.0f;
    float callback_budget_us = 0.0f;
    float callback_duration_us = 0.0f;
    float cpu_load_percent = 0.0f;

    uint64_t callback_count = 0;
    uint64_t deadline_miss_count = 0;
    uint64_t underrun_count = 0;
    uint64_t overrun_count = 0;

    std::array<float, DSP_PROFILER_HISTORY_SIZE> latency_history_ms{};
    std::array<float, DSP_PROFILER_HISTORY_SIZE> cpu_history_percent{};
    int history_offset = 0;

    std::array<DspProfilerModuleSnapshot, MAX_PROFILED_MODULES> modules{};
    int module_count = 0;
};

class DspPerformanceProfiler {
   public:
    void begin_callback(int frame_count, int sample_rate, int buffer_size);
    void record_module_time(int module_index, float elapsed_us);
    void end_callback(float callback_duration_us);

    void record_underrun();
    void record_overrun();

    DspProfilerSnapshot snapshot() const;
    void reset();

   private:
    std::atomic<int> sample_rate_{0};
    std::atomic<int> buffer_size_{0};
    std::atomic<int> frame_count_{0};

    std::atomic<float> latency_ms_{0.0f};
    std::atomic<float> callback_budget_us_{0.0f};
    std::atomic<float> callback_duration_us_{0.0f};
    std::atomic<float> cpu_load_percent_{0.0f};

    std::atomic<uint64_t> callback_count_{0};
    std::atomic<uint64_t> deadline_miss_count_{0};
    std::atomic<uint64_t> underrun_count_{0};
    std::atomic<uint64_t> overrun_count_{0};

    std::array<std::atomic<float>, DSP_PROFILER_HISTORY_SIZE> latency_history_ms_{};
    std::array<std::atomic<float>, DSP_PROFILER_HISTORY_SIZE> cpu_history_percent_{};
    std::atomic<int> history_index_{0};

    std::array<std::atomic<float>, MAX_PROFILED_MODULES> module_last_us_{};
    std::array<std::atomic<float>, MAX_PROFILED_MODULES> module_average_us_{};
    std::array<std::atomic<float>, MAX_PROFILED_MODULES> module_peak_us_{};
    std::atomic<int> module_count_{0};
};

}  // namespace Amplitron