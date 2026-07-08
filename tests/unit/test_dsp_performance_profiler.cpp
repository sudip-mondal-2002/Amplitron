#include "audio/engine/dsp_performance_profiler.h"
#include "gui/views/gui_dsp_profiler.h"
#include "imgui.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

TEST(DspPerformanceProfiler_DefaultSnapshot) {
    DspPerformanceProfiler profiler;

    const auto snapshot = profiler.snapshot();

    ASSERT_TRUE(snapshot.sample_rate == 0);
    ASSERT_TRUE(snapshot.buffer_size == 0);
    ASSERT_NEAR(snapshot.latency_ms, 0.0f, 0.001f);
    ASSERT_NEAR(snapshot.callback_budget_us, 0.0f, 0.001f);
    ASSERT_NEAR(snapshot.cpu_load_percent, 0.0f, 0.001f);
    ASSERT_TRUE(snapshot.callback_count == 0);
    ASSERT_TRUE(snapshot.deadline_miss_count == 0);
    ASSERT_TRUE(snapshot.underrun_count == 0);
    ASSERT_TRUE(snapshot.overrun_count == 0);
    ASSERT_TRUE(snapshot.module_count == 0);
}

TEST(DspPerformanceProfiler_CallbackMetricsAndHistory) {
    DspPerformanceProfiler profiler;

    profiler.begin_callback(64, 48000, 64);
    profiler.end_callback(1000.0f);

    const auto snapshot = profiler.snapshot();

    ASSERT_TRUE(snapshot.sample_rate == 48000);
    ASSERT_TRUE(snapshot.buffer_size == 64);
    ASSERT_NEAR(snapshot.latency_ms, 1.333f, 0.01f);
    ASSERT_NEAR(snapshot.callback_budget_us, 1333.333f, 0.1f);
    ASSERT_NEAR(snapshot.callback_duration_us, 1000.0f, 0.001f);
    ASSERT_NEAR(snapshot.cpu_load_percent, 75.0f, 0.1f);
    ASSERT_TRUE(snapshot.callback_count == 1);
    ASSERT_TRUE(snapshot.deadline_miss_count == 0);
    ASSERT_TRUE(snapshot.history_offset == 1);
    ASSERT_NEAR(snapshot.latency_history_ms[0], snapshot.latency_ms, 0.001f);
    ASSERT_NEAR(snapshot.cpu_history_percent[0], snapshot.cpu_load_percent, 0.001f);
}

TEST(DspPerformanceProfiler_DeadlineMissAndStreamCounters) {
    DspPerformanceProfiler profiler;

    profiler.begin_callback(64, 48000, 64);
    profiler.end_callback(2000.0f);
    profiler.record_underrun();
    profiler.record_underrun();
    profiler.record_overrun();

    const auto snapshot = profiler.snapshot();

    ASSERT_TRUE(snapshot.callback_count == 1);
    ASSERT_TRUE(snapshot.deadline_miss_count == 1);
    ASSERT_TRUE(snapshot.underrun_count == 2);
    ASSERT_TRUE(snapshot.overrun_count == 1);
    ASSERT_TRUE(snapshot.cpu_load_percent > 100.0f);
}

TEST(DspPerformanceProfiler_ModuleTimingStats) {
    DspPerformanceProfiler profiler;

    profiler.begin_callback(64, 48000, 64);
    profiler.record_module_time(2, 100.0f);
    profiler.record_module_time(2, 200.0f);
    profiler.record_module_time(5, 500.0f);
    profiler.end_callback(900.0f);

    const auto snapshot = profiler.snapshot();

    ASSERT_TRUE(snapshot.module_count >= 6);

    const auto& module_two = snapshot.modules[2];
    ASSERT_TRUE(module_two.index == 2);
    ASSERT_NEAR(module_two.last_us, 200.0f, 0.001f);
    ASSERT_TRUE(module_two.average_us > 100.0f);
    ASSERT_TRUE(module_two.average_us < 200.0f);
    ASSERT_NEAR(module_two.peak_us, 200.0f, 0.001f);
    ASSERT_TRUE(!module_two.overloaded);

    const auto& module_five = snapshot.modules[5];
    ASSERT_TRUE(module_five.index == 5);
    ASSERT_NEAR(module_five.last_us, 500.0f, 0.001f);
    ASSERT_NEAR(module_five.peak_us, 500.0f, 0.001f);
    ASSERT_TRUE(module_five.budget_percent > 0.0f);
}

TEST(DspPerformanceProfiler_ResetClearsMetrics) {
    DspPerformanceProfiler profiler;

    profiler.begin_callback(64, 48000, 64);
    profiler.record_module_time(1, 300.0f);
    profiler.record_underrun();
    profiler.record_overrun();
    profiler.end_callback(2000.0f);

    profiler.reset();

    const auto snapshot = profiler.snapshot();

    ASSERT_TRUE(snapshot.callback_count == 0);
    ASSERT_TRUE(snapshot.deadline_miss_count == 0);
    ASSERT_TRUE(snapshot.underrun_count == 0);
    ASSERT_TRUE(snapshot.overrun_count == 0);
    ASSERT_TRUE(snapshot.module_count == 0);
    ASSERT_TRUE(snapshot.history_offset == 0);
    ASSERT_NEAR(snapshot.latency_history_ms[0], 0.0f, 0.001f);
    ASSERT_NEAR(snapshot.cpu_history_percent[0], 0.0f, 0.001f);
}

TEST(DspPerformanceProfiler_IgnoresInvalidModuleIndexes) {
    DspPerformanceProfiler profiler;

    profiler.begin_callback(64, 48000, 64);
    profiler.record_module_time(-1, 100.0f);
    profiler.record_module_time(MAX_PROFILED_MODULES, 100.0f);
    profiler.end_callback(100.0f);

    const auto snapshot = profiler.snapshot();

    ASSERT_TRUE(snapshot.module_count == 0);
    ASSERT_TRUE(snapshot.callback_count == 1);
}

TEST(DspPerformanceProfiler_HistoryWrapsAndTracksMultipleModules) {
    DspPerformanceProfiler profiler;

    for (std::size_t i = 0; i < DSP_PROFILER_HISTORY_SIZE + 3; ++i) {
        profiler.begin_callback(128, 48000, 128);
        profiler.record_module_time(0, 100.0f + static_cast<float>(i));
        profiler.record_module_time(1, 200.0f + static_cast<float>(i));
        profiler.record_module_time(2, 300.0f + static_cast<float>(i));
        profiler.end_callback(500.0f + static_cast<float>(i));
    }

    const auto snapshot = profiler.snapshot();

    ASSERT_TRUE(snapshot.sample_rate == 48000);
    ASSERT_TRUE(snapshot.buffer_size == 128);
    ASSERT_TRUE(snapshot.callback_count == DSP_PROFILER_HISTORY_SIZE + 3);
    ASSERT_TRUE(snapshot.history_offset == 3);
    ASSERT_TRUE(snapshot.module_count >= 3);

    ASSERT_TRUE(snapshot.modules[0].index == 0);
    ASSERT_TRUE(snapshot.modules[1].index == 1);
    ASSERT_TRUE(snapshot.modules[2].index == 2);

    ASSERT_TRUE(snapshot.modules[0].last_us > 100.0f);
    ASSERT_TRUE(snapshot.modules[1].last_us > 200.0f);
    ASSERT_TRUE(snapshot.modules[2].last_us > 300.0f);

    ASSERT_TRUE(snapshot.modules[0].average_us > 0.0f);
    ASSERT_TRUE(snapshot.modules[1].average_us > snapshot.modules[0].average_us);
    ASSERT_TRUE(snapshot.modules[2].average_us > snapshot.modules[1].average_us);

    ASSERT_TRUE(snapshot.modules[0].peak_us >= snapshot.modules[0].last_us);
    ASSERT_TRUE(snapshot.modules[1].peak_us >= snapshot.modules[1].last_us);
    ASSERT_TRUE(snapshot.modules[2].peak_us >= snapshot.modules[2].last_us);

    ASSERT_TRUE(snapshot.latency_history_ms[0] > 0.0f);
    ASSERT_TRUE(snapshot.cpu_history_percent[0] > 0.0f);
}

class ScopedImGuiContext {
   public:
    ScopedImGuiContext()
        : previous_context_(ImGui::GetCurrentContext()), context_(ImGui::CreateContext()) {
        ImGui::SetCurrentContext(context_);

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(800.0f, 600.0f);

        io.Fonts->AddFontDefault();
        unsigned char* font_pixels = nullptr;
        int font_width = 0;
        int font_height = 0;
        io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

        ASSERT_TRUE(font_pixels != nullptr);
        ASSERT_TRUE(font_width > 0);
        ASSERT_TRUE(font_height > 0);
    }

    ~ScopedImGuiContext() {
        ImGui::DestroyContext(context_);
        ImGui::SetCurrentContext(previous_context_);
    }

   private:
    ImGuiContext* previous_context_;
    ImGuiContext* context_;
};

TEST(GuiDspProfiler_RenderSmoke) {
    ScopedImGuiContext imgui_context;

    DspProfilerSnapshot snapshot;
    snapshot.sample_rate = 48000;
    snapshot.buffer_size = 64;
    snapshot.latency_ms = 1.33f;
    snapshot.callback_budget_us = 1333.33f;
    snapshot.callback_duration_us = 1500.0f;
    snapshot.cpu_load_percent = 112.5f;
    snapshot.callback_count = 3;
    snapshot.deadline_miss_count = 1;
    snapshot.underrun_count = 1;
    snapshot.overrun_count = 1;
    snapshot.history_offset = 2;
    snapshot.latency_history_ms[0] = 1.33f;
    snapshot.latency_history_ms[1] = 12.5f;
    snapshot.cpu_history_percent[0] = 75.0f;
    snapshot.cpu_history_percent[1] = 112.5f;
    snapshot.module_count = 2;
    snapshot.modules[0] = {0, 100.0f, 100.0f, 120.0f, 7.5f, false};
    snapshot.modules[1] = {1, 500.0f, 450.0f, 600.0f, 37.5f, true};

    bool open = true;
    GuiDspProfiler view;

    ImGui::NewFrame();
    view.render(&open, snapshot);
    ImGui::Render();

    ASSERT_TRUE(open);
}

TEST(GuiDspProfiler_ClosedWindowSmoke) {
    ScopedImGuiContext imgui_context;

    bool open = false;
    DspProfilerSnapshot snapshot;
    GuiDspProfiler view;

    ImGui::NewFrame();
    view.render(&open, snapshot);
    ImGui::Render();

    ASSERT_TRUE(!open);
}

TEST(GuiDspProfiler_EmptySnapshotSmoke) {
    ScopedImGuiContext imgui_context;

    bool open = true;
    DspProfilerSnapshot snapshot;
    GuiDspProfiler view;

    ImGui::NewFrame();
    view.render(&open, snapshot);
    ImGui::Render();

    ASSERT_TRUE(open);
}

TEST(GuiDspProfiler_NormalLoadSnapshotSmoke) {
    ScopedImGuiContext imgui_context;

    DspProfilerSnapshot snapshot;
    snapshot.sample_rate = 48000;
    snapshot.buffer_size = 128;
    snapshot.latency_ms = 2.66f;
    snapshot.callback_budget_us = 2666.67f;
    snapshot.callback_duration_us = 700.0f;
    snapshot.cpu_load_percent = 26.25f;
    snapshot.callback_count = 8;
    snapshot.history_offset = 4;
    snapshot.latency_history_ms[0] = 2.66f;
    snapshot.cpu_history_percent[0] = 26.25f;

    bool open = true;
    GuiDspProfiler view;

    ImGui::NewFrame();
    view.render(&open, snapshot);
    ImGui::Render();

    ASSERT_TRUE(open);
}
