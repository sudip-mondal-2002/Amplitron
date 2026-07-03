#include "gui/views/gui_dsp_profiler.h"

#include <algorithm>

#include "imgui.h"

namespace Amplitron {
namespace {

const char* load_status_text(float cpu_percent) {
    if (cpu_percent >= 100.0f) {
        return "CRITICAL: audio callback is exceeding its real-time budget.";
    }
    if (cpu_percent >= 80.0f) {
        return "WARNING: audio callback is close to the real-time limit.";
    }
    return "OK: audio callback is within the real-time budget.";
}

}  // namespace

void GuiDspProfiler::render(bool* open, const DspProfilerSnapshot& snapshot) {
    if (!open || !*open) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(720, 520), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("DSP Performance Profiler", open)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Real-time DSP performance");
    ImGui::Separator();

    ImGui::Text("Sample rate: %d Hz", snapshot.sample_rate);
    ImGui::Text("Buffer size: %d samples", snapshot.buffer_size);
    ImGui::Text("Estimated latency: %.2f ms", snapshot.latency_ms);
    ImGui::Text("Callback budget: %.2f us", snapshot.callback_budget_us);
    ImGui::Text("Last callback time: %.2f us", snapshot.callback_duration_us);
    ImGui::Text("CPU load: %.1f%%", snapshot.cpu_load_percent);

    const float normalized_cpu = std::clamp(snapshot.cpu_load_percent / 100.0f, 0.0f, 1.0f);
    ImGui::ProgressBar(normalized_cpu, ImVec2(-1.0f, 0.0f));

    ImGui::Spacing();
    ImGui::TextWrapped("%s", load_status_text(snapshot.cpu_load_percent));

    if (snapshot.deadline_miss_count > 0) {
        ImGui::TextWrapped(
            "Deadline misses detected: %llu. This means the callback exceeded its processing "
            "budget.",
            static_cast<unsigned long long>(snapshot.deadline_miss_count));
    }

    if (snapshot.underrun_count > 0 || snapshot.overrun_count > 0) {
        ImGui::TextWrapped("Stream warnings: underruns=%llu, overruns=%llu.",
                           static_cast<unsigned long long>(snapshot.underrun_count),
                           static_cast<unsigned long long>(snapshot.overrun_count));
    }

    ImGui::SeparatorText("History");

    float max_latency_history = snapshot.latency_ms;
    for (float value : snapshot.latency_history_ms) {
        max_latency_history = std::max(max_latency_history, value);
    }

    float max_cpu_history = snapshot.cpu_load_percent;
    for (float value : snapshot.cpu_history_percent) {
        max_cpu_history = std::max(max_cpu_history, value);
    }

    const int history_offset = std::clamp(snapshot.history_offset, 0,
                                          static_cast<int>(snapshot.latency_history_ms.size()) - 1);

    ImGui::PlotLines("Latency history (ms)", snapshot.latency_history_ms.data(),
                     static_cast<int>(snapshot.latency_history_ms.size()), history_offset, nullptr,
                     0.0f, std::max(10.0f, max_latency_history * 1.2f), ImVec2(-1.0f, 80.0f));

    ImGui::PlotLines("CPU history (%)", snapshot.cpu_history_percent.data(),
                     static_cast<int>(snapshot.cpu_history_percent.size()), history_offset, nullptr,
                     0.0f, std::max(100.0f, max_cpu_history * 1.2f), ImVec2(-1.0f, 80.0f));

    ImGui::SeparatorText("DSP Modules");

    if (snapshot.module_count <= 0) {
        ImGui::TextWrapped(
            "No DSP module timings have been recorded yet. Start audio playback to populate this "
            "table.");
    } else if (ImGui::BeginTable(
                   "DspModuleTimingTable", 6,
                   ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Module");
        ImGui::TableSetupColumn("Last us");
        ImGui::TableSetupColumn("Average us");
        ImGui::TableSetupColumn("Peak us");
        ImGui::TableSetupColumn("Budget %");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();

        for (int i = 0; i < snapshot.module_count; ++i) {
            const auto& module = snapshot.modules[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Node %d", module.index);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", module.last_us);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", module.average_us);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", module.peak_us);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.1f%%", module.budget_percent);

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%s", module.overloaded ? "OVERLOADED" : "OK");
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextWrapped(
        "Accessibility note: warnings are shown as explicit text labels and table status values, "
        "not color alone.");

    ImGui::End();
}

}  // namespace Amplitron