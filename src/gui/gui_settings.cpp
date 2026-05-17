#include "gui/gui_settings.h"
#include "gui/theme.h"
#include <imgui.h>
#include <cstdio>

namespace Amplitron {

void GuiSettings::render(bool& show) {
    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);  // Increased height for MIDI section
    if (!ImGui::Begin("Audio Settings", &show)) {
        ImGui::End();
        return;
    }

    // --- Current routing summary ---
    ImGui::TextColored(Theme::Gold(), "SIGNAL ROUTING");
    ImGui::BeginChild("RoutingSummary", ImVec2(0, 60), true);
    ImGui::TextColored(Theme::Live(), "Guitar IN:");
    ImGui::SameLine();
    ImGui::Text("%s", engine_.get_input_device_name().c_str());
    ImGui::TextColored(ImVec4(0.35f, 0.60f, 0.95f, 1.0f), "Speaker OUT:");
    ImGui::SameLine();
    ImGui::Text("%s", engine_.get_output_device_name().c_str());
    ImGui::EndChild();

    const std::string& dev_error = engine_.get_last_error();
    if (!dev_error.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Device error: %s", dev_error.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss")) {
            engine_.clear_error();
        }
    }

    ImGui::Spacing();

    // --- Latency settings ---
    ImGui::TextColored(Theme::Gold(), "LATENCY");

    // Buffer size
    ImGui::Text("Buffer Size (lower = less latency, more CPU):");
    int buf_size = engine_.get_buffer_size();
    const int buf_sizes[] = {32, 64, 128, 256, 512};
    const char* buf_labels[] = {"32", "64", "128", "256", "512"};
    int current_idx = 1;
    for (int i = 0; i < 5; ++i) {
        if (buf_sizes[i] == buf_size) { current_idx = i; break; }
    }
    if (ImGui::Combo("Buffer Size", &current_idx, buf_labels, 5)) {
        engine_.set_buffer_size(buf_sizes[current_idx]);
    }

    float latency_ms = 1000.0f * engine_.get_buffer_size() / engine_.get_sample_rate();
    ImGui::Text("Estimated latency: %.1f ms", latency_ms);
#ifdef AMPLITRON_ANDROID_OBOE
    // Fix: show the actual runtime-negotiated sharing mode, not a hardcoded string.
    // oboe::SharingMode::Exclusive = AAudio direct path; Shared = mixed/OpenSL fallback.
    const char* backendLabel = engine_.get_oboe_sharing_mode_label();
    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f),
                       "Audio backend: Oboe (%s)", backendLabel);
    if (engine_.is_running()) {
        ImGui::Text("Measured round-trip: see logcat for Oboe latency report");
    }
#endif

    // CPU load watchdog & auto-tuning
    float cpu_load = engine_.get_cpu_load();
    ImGui::Spacing();
    ImVec4 load_color = (cpu_load > 0.80f) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) :
                         (cpu_load > 0.50f) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                              ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    ImGui::TextColored(load_color, "CPU Load: %.0f%%", cpu_load * 100.0f);
    ImGui::SameLine();
    ImGui::ProgressBar(cpu_load, ImVec2(150, 0));

    int suggested = engine_.get_suggested_buffer_size();
    if (suggested != engine_.get_buffer_size()) {
        ImGui::SameLine();
        char suggest_label[64];
        std::snprintf(suggest_label, sizeof(suggest_label),
                      "Switch to %d", suggested);
        if (ImGui::SmallButton(suggest_label)) {
            engine_.set_buffer_size(suggested);
        }
    }

    bool auto_buf = engine_.is_auto_buffer_enabled();
    if (ImGui::Checkbox("Auto-tune buffer size", &auto_buf)) {
        engine_.set_auto_buffer_enabled(auto_buf);
    }
    if (auto_buf && suggested != engine_.get_buffer_size()) {
        engine_.set_buffer_size(suggested);
    }
    ImGui::Spacing();

    // Sample rate
    int sr = engine_.get_sample_rate();
    const int rates[] = {44100, 48000, 96000};
    const char* rate_labels[] = {"44100", "48000", "96000"};
    int sr_idx = 1;
    for (int i = 0; i < 3; ++i) {
        if (rates[i] == sr) { sr_idx = i; break; }
    }
    if (ImGui::Combo("Sample Rate", &sr_idx, rate_labels, 3)) {
        engine_.set_sample_rate(rates[sr_idx]);
    }

    ImGui::Separator();

    // --- Input device (USB Guitar Cable) ---
    ImGui::TextColored(Theme::Gold(),
        "INPUT DEVICE (USB Guitar Cable)");
    ImGui::TextWrapped(
        "Select your USB guitar cable or audio interface. "
        "USB devices are highlighted with [USB].");

    int current_input = engine_.get_input_device();
    auto input_devs = engine_.get_input_devices();
    ImGui::BeginChild("InputDevices", ImVec2(0, 120), true);
    for (auto& dev : input_devs) {
        bool is_selected = (dev.index == current_input);
        std::string label = dev.name;
        if (dev.is_usb_device) {
            label += "  [USB]";
        }

        if (dev.is_usb_device) {
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::GoldHot());
        }

        if (ImGui::Selectable(label.c_str(), is_selected)) {
            engine_.set_input_device(dev.index);
        }

        if (dev.is_usb_device) {
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // --- Output device ---
    ImGui::TextColored(Theme::Gold(), "OUTPUT DEVICE (Speakers/Headphones)");

    int current_output = engine_.get_output_device();
    auto output_devs = engine_.get_output_devices();
    ImGui::BeginChild("OutputDevices", ImVec2(0, 120), true);
    for (auto& dev : output_devs) {
        bool is_selected = (dev.index == current_output);
        std::string label = dev.name;
        if (dev.is_usb_device) {
            label += "  [USB - not recommended]";
        }

        if (dev.is_usb_device) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 0.7f));
        }

        if (ImGui::Selectable(label.c_str(), is_selected)) {
            engine_.set_output_device(dev.index);
        }

        if (dev.is_usb_device) {
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();

    // MIDI settings are managed in a separate "MIDI Settings" window 
    // accessible from the Utilities menu or the status bar.
    ImGui::Separator();
    ImGui::TextDisabled("MIDI settings are managed in a separate window (Utilities > MIDI Settings).");

    ImGui::End();
}

} // namespace Amplitron
