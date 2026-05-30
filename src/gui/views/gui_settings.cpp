#include "gui/views/gui_settings.h"
#include "gui/theme/theme.h"
#include <imgui.h>
#include <cstdio>

namespace Amplitron {

void GuiSettings::render(bool& show) {
    const SettingsProps& p = props_;

    ImGui::SetNextWindowSize(ImVec2(600, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Audio Settings", &show)) {
        ImGui::End();
        return;
    }

    // ── Signal routing summary ──
    ImGui::TextColored(Theme::Gold(), "SIGNAL ROUTING");
    ImGui::BeginChild("RoutingSummary", ImVec2(0, 60), true);
    ImGui::TextColored(Theme::Live(), "Guitar IN:");
    ImGui::SameLine();
    ImGui::Text("%s", p.input_device_name.c_str());
    ImGui::TextColored(ImVec4(0.35f, 0.60f, 0.95f, 1.0f), "Speaker OUT:");
    ImGui::SameLine();
    ImGui::Text("%s", p.output_device_name.c_str());
    ImGui::EndChild();

    if (!p.device_error.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Device error: %s", p.device_error.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Dismiss") && p.on_clear_error) p.on_clear_error();
    }

    ImGui::Spacing();

    // ── Latency ──
    ImGui::TextColored(Theme::Gold(), "LATENCY");
    ImGui::Text("Buffer Size (lower = less latency, more CPU):");
    const int   buf_sizes[]  = {32, 64, 128, 256, 512};
    const char* buf_labels[] = {"32", "64", "128", "256", "512"};
    int current_idx = 1;
    for (int i = 0; i < 5; ++i)
        if (buf_sizes[i] == p.buffer_size) { current_idx = i; break; }
    if (ImGui::Combo("Buffer Size", &current_idx, buf_labels, 5)) {
        if (p.on_buffer_size_changed) p.on_buffer_size_changed(buf_sizes[current_idx]);
    }
    ImGui::Text("Estimated latency: %.1f ms", p.latency_ms);

#ifdef AMPLITRON_ANDROID_OBOE
    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f),
                       "Audio backend: Oboe (%s)", p.oboe_mode_label);
#endif

    // CPU load
    ImVec4 load_color = (p.cpu_load > 0.80f) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) :
                         (p.cpu_load > 0.50f) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                                ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    ImGui::Spacing();
    ImGui::TextColored(load_color, "CPU Load: %.0f%%", p.cpu_load * 100.0f);
    ImGui::SameLine();
    ImGui::ProgressBar(p.cpu_load, ImVec2(150, 0));

    if (p.suggested_buf != p.buffer_size) {
        ImGui::SameLine();
        char suggest_label[64];
        std::snprintf(suggest_label, sizeof(suggest_label), "Switch to %d", p.suggested_buf);
        if (ImGui::SmallButton(suggest_label) && p.on_buffer_size_changed)
            p.on_buffer_size_changed(p.suggested_buf);
    }

    bool auto_buf = p.auto_buf;
    if (ImGui::Checkbox("Auto-tune buffer size", &auto_buf) && p.on_auto_buf_changed)
        p.on_auto_buf_changed(auto_buf);

    ImGui::Spacing();

    // ── Sample rate ──
    const int   rates[]       = {44100, 48000, 96000};
    const char* rate_labels[] = {"44100", "48000", "96000"};
    int sr_idx = 1;
    for (int i = 0; i < 3; ++i)
        if (rates[i] == p.sample_rate) { sr_idx = i; break; }
    if (ImGui::Combo("Sample Rate", &sr_idx, rate_labels, 3)) {
        if (p.on_sample_rate_changed) p.on_sample_rate_changed(rates[sr_idx]);
    }

    ImGui::Separator();

    // ── Input devices ──
    ImGui::TextColored(Theme::Gold(), "INPUT DEVICE (USB Guitar Cable)");
    ImGui::TextWrapped("Select your USB guitar cable or audio interface. USB devices are highlighted with [USB].");
    ImGui::BeginChild("InputDevices", ImVec2(0, 120), true);
    for (const auto& dev : p.input_devices) {
        ImGui::PushID(dev.index);
        bool is_selected = (dev.index == p.current_input);
        std::string label = dev.name + (dev.is_usb ? "  [USB]" : "");
        if (dev.is_usb) ImGui::PushStyleColor(ImGuiCol_Text, Theme::GoldHot());
        if (ImGui::Selectable(label.c_str(), is_selected) && p.on_input_device_changed)
            p.on_input_device_changed(dev.index);
        if (dev.is_usb) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Spacing();

    // ── Output devices ──
    ImGui::TextColored(Theme::Gold(), "OUTPUT DEVICE (Speakers/Headphones)");
    ImGui::BeginChild("OutputDevices", ImVec2(0, 120), true);
    for (const auto& dev : p.output_devices) {
        ImGui::PushID(dev.index);
        bool is_selected = (dev.index == p.current_output);
        std::string label = dev.name + (dev.is_usb ? "  [USB - not recommended]" : "");
        if (dev.is_usb) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 0.7f));
        if (ImGui::Selectable(label.c_str(), is_selected) && p.on_output_device_changed)
            p.on_output_device_changed(dev.index);
        if (dev.is_usb) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("MIDI settings are managed in a separate window (Utilities > MIDI Settings).");

    ImGui::End();
}

} // namespace Amplitron
