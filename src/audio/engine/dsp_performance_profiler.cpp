#include "audio/engine/dsp_performance_profiler.h"

#include <algorithm>

namespace Amplitron {

void DspPerformanceProfiler::begin_callback(int frame_count, int sample_rate, int buffer_size) {
    frame_count_.store(frame_count, std::memory_order_relaxed);
    sample_rate_.store(sample_rate, std::memory_order_relaxed);
    buffer_size_.store(buffer_size, std::memory_order_relaxed);

    const float latency_ms =
        sample_rate > 0
            ? (static_cast<float>(buffer_size) / static_cast<float>(sample_rate)) * 1000.0f
            : 0.0f;

    const float budget_us =
        sample_rate > 0
            ? (static_cast<float>(frame_count) / static_cast<float>(sample_rate)) * 1000000.0f
            : 0.0f;

    latency_ms_.store(latency_ms, std::memory_order_relaxed);
    callback_budget_us_.store(budget_us, std::memory_order_relaxed);
}

void DspPerformanceProfiler::record_module_time(int module_index, float elapsed_us) {
    if (module_index < 0 || module_index >= MAX_PROFILED_MODULES) {
        return;
    }

    module_last_us_[module_index].store(elapsed_us, std::memory_order_relaxed);

    const float old_avg = module_average_us_[module_index].load(std::memory_order_relaxed);
    const float new_avg = old_avg <= 0.0f ? elapsed_us : (old_avg * 0.9f) + (elapsed_us * 0.1f);
    module_average_us_[module_index].store(new_avg, std::memory_order_relaxed);

    const float old_peak = module_peak_us_[module_index].load(std::memory_order_relaxed);
    module_peak_us_[module_index].store(std::max(old_peak, elapsed_us), std::memory_order_relaxed);

    int observed_count = module_count_.load(std::memory_order_relaxed);
    while (module_index + 1 > observed_count &&
           !module_count_.compare_exchange_weak(observed_count, module_index + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
    }
}

void DspPerformanceProfiler::end_callback(float callback_duration_us) {
    callback_duration_us_.store(callback_duration_us, std::memory_order_relaxed);

    const float budget_us = callback_budget_us_.load(std::memory_order_relaxed);
    const float cpu_percent = budget_us > 0.0f ? (callback_duration_us / budget_us) * 100.0f : 0.0f;

    cpu_load_percent_.store(cpu_percent, std::memory_order_relaxed);

    if (budget_us > 0.0f && callback_duration_us > budget_us) {
        deadline_miss_count_.fetch_add(1, std::memory_order_relaxed);
    }

    const uint64_t callback_number = callback_count_.fetch_add(1, std::memory_order_relaxed);
    const int history_index = static_cast<int>(callback_number % DSP_PROFILER_HISTORY_SIZE);

    latency_history_ms_[history_index].store(latency_ms_.load(std::memory_order_relaxed),
                                             std::memory_order_relaxed);
    cpu_history_percent_[history_index].store(cpu_percent, std::memory_order_relaxed);

    history_index_.store((history_index + 1) % DSP_PROFILER_HISTORY_SIZE,
                         std::memory_order_relaxed);
}

void DspPerformanceProfiler::record_underrun() {
    underrun_count_.fetch_add(1, std::memory_order_relaxed);
}

void DspPerformanceProfiler::record_overrun() {
    overrun_count_.fetch_add(1, std::memory_order_relaxed);
}

DspProfilerSnapshot DspPerformanceProfiler::snapshot() const {
    DspProfilerSnapshot snapshot;

    snapshot.sample_rate = sample_rate_.load(std::memory_order_relaxed);
    snapshot.history_offset = history_index_.load(std::memory_order_relaxed);
    snapshot.buffer_size = buffer_size_.load(std::memory_order_relaxed);
    snapshot.latency_ms = latency_ms_.load(std::memory_order_relaxed);
    snapshot.callback_budget_us = callback_budget_us_.load(std::memory_order_relaxed);
    snapshot.callback_duration_us = callback_duration_us_.load(std::memory_order_relaxed);
    snapshot.cpu_load_percent = cpu_load_percent_.load(std::memory_order_relaxed);

    snapshot.callback_count = callback_count_.load(std::memory_order_relaxed);
    snapshot.deadline_miss_count = deadline_miss_count_.load(std::memory_order_relaxed);
    snapshot.underrun_count = underrun_count_.load(std::memory_order_relaxed);
    snapshot.overrun_count = overrun_count_.load(std::memory_order_relaxed);

    for (int i = 0; i < DSP_PROFILER_HISTORY_SIZE; ++i) {
        snapshot.latency_history_ms[i] = latency_history_ms_[i].load(std::memory_order_relaxed);
        snapshot.cpu_history_percent[i] = cpu_history_percent_[i].load(std::memory_order_relaxed);
    }

    snapshot.module_count =
        std::min(module_count_.load(std::memory_order_relaxed), MAX_PROFILED_MODULES);
    const float budget_us = snapshot.callback_budget_us;

    for (int i = 0; i < snapshot.module_count; ++i) {
        const float last_us = module_last_us_[i].load(std::memory_order_relaxed);
        const float average_us = module_average_us_[i].load(std::memory_order_relaxed);
        const float peak_us = module_peak_us_[i].load(std::memory_order_relaxed);
        const float budget_percent = budget_us > 0.0f ? (last_us / budget_us) * 100.0f : 0.0f;

        snapshot.modules[i].index = i;
        snapshot.modules[i].last_us = last_us;
        snapshot.modules[i].average_us = average_us;
        snapshot.modules[i].peak_us = peak_us;
        snapshot.modules[i].budget_percent = budget_percent;
        snapshot.modules[i].overloaded = budget_percent >= 25.0f;
    }

    return snapshot;
}

void DspPerformanceProfiler::reset() {
    sample_rate_.store(0, std::memory_order_relaxed);
    buffer_size_.store(0, std::memory_order_relaxed);
    frame_count_.store(0, std::memory_order_relaxed);

    latency_ms_.store(0.0f, std::memory_order_relaxed);
    callback_budget_us_.store(0.0f, std::memory_order_relaxed);
    callback_duration_us_.store(0.0f, std::memory_order_relaxed);
    cpu_load_percent_.store(0.0f, std::memory_order_relaxed);

    callback_count_.store(0, std::memory_order_relaxed);
    deadline_miss_count_.store(0, std::memory_order_relaxed);
    underrun_count_.store(0, std::memory_order_relaxed);
    overrun_count_.store(0, std::memory_order_relaxed);
    history_index_.store(0, std::memory_order_relaxed);
    module_count_.store(0, std::memory_order_relaxed);

    for (auto& value : latency_history_ms_) {
        value.store(0.0f, std::memory_order_relaxed);
    }

    for (auto& value : cpu_history_percent_) {
        value.store(0.0f, std::memory_order_relaxed);
    }

    for (int i = 0; i < MAX_PROFILED_MODULES; ++i) {
        module_last_us_[i].store(0.0f, std::memory_order_relaxed);
        module_average_us_[i].store(0.0f, std::memory_order_relaxed);
        module_peak_us_[i].store(0.0f, std::memory_order_relaxed);
    }
}

}  // namespace Amplitron