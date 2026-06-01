#include "gui/views/gui_recording.h"
#include "gui/dialogs/file_dialog.h"
#include "gui/theme/theme.h"
#include "common.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Amplitron {

void GuiRecording::render() {
    const RecordingProps& p = props_;

    float font_scale = ImGui::GetFontSize() / 14.0f;
    float base_h = ImGui::GetFrameHeight()
                 + ImGui::GetStyle().WindowPadding.y * 2.0f
                 + ImGui::GetStyle().WindowBorderSize * 2.0f;
    float panel_height = p.is_recording ? (base_h + 80.0f) : base_h;

    ImGui::BeginChild("RecordingPanel", ImVec2(0, panel_height), true,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (p.is_recording) {
        // ── Pause / Resume ──
        if (p.is_paused) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("RESUME", ImVec2(80 * font_scale, 0)) && p.on_resume) p.on_resume();
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.6f, 0.5f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button("PAUSE", ImVec2(80 * font_scale, 0)) && p.on_pause) p.on_pause();
            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine();

        // ── Stop ──
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("STOP", ImVec2(80 * font_scale, 0))) {
            if (p.on_stop) p.on_stop();
            set_state([](RecordingState& st) { st.needs_save = true; });
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        // ── Blink indicator ──
        float t = static_cast<float>(ImGui::GetTime());
        if (p.is_paused) {
            ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.2f, 1.0f), "  PAUSED");
        } else {
            float blink = (std::sin(t * 4.0f) > 0.0f) ? 1.0f : 0.3f;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.15f, 0.15f, blink));
            ImGui::Text("  REC");
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // ── Timer ──
        int mins = static_cast<int>(p.duration) / 60;
        int secs = static_cast<int>(p.duration) % 60;
        int ms   = static_cast<int>((p.duration - static_cast<int>(p.duration)) * 10);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "  %02d:%02d.%d", mins, secs, ms);

        ImGui::SameLine();

        // ── Peak meter ──
        float peak = p.current_peak;
        ImGui::TextColored(peak > 0.9f ? ImVec4(1, 0.2f, 0.2f, 1) :
                           peak > 0.6f ? ImVec4(1, 0.8f, 0.2f, 1) :
                                         ImVec4(0.2f, 0.8f, 0.2f, 1),
                           "  Peak: %.1f dB",
                           peak > 0.0001f ? 20.0f * std::log10(peak) : -96.0f);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
        int64_t file_bytes = p.samples_written * 2 * p.channels;
        if (file_bytes > 1024 * 1024)
            ImGui::Text("%.1f MB", file_bytes / (1024.0f * 1024.0f));
        else
            ImGui::Text("%.0f KB", file_bytes / 1024.0f);

        // ── Waveform ──
        ImGui::Spacing();
        ImVec2 wave_pos = ImGui::GetCursorScreenPos();
        float  wave_w   = ImGui::GetContentRegionAvail().x;
        float  wave_h   = 50.0f * font_scale;
        ImDrawList* draw = ImGui::GetWindowDrawList();

        draw->AddRectFilled(wave_pos,
                            ImVec2(wave_pos.x + wave_w, wave_pos.y + wave_h),
                            IM_COL32(20, 18, 16, 255), 4.0f);

        float center_y = wave_pos.y + wave_h * 0.5f;
        draw->AddLine(ImVec2(wave_pos.x, center_y),
                      ImVec2(wave_pos.x + wave_w, center_y),
                      IM_COL32(60, 55, 48, 255));

        ImU32 wave_color       = p.is_paused ? IM_COL32(180, 160, 50, 200) : IM_COL32(200, 80, 60, 220);
        ImU32 wave_color_bright = p.is_paused ? IM_COL32(220, 200, 80, 255) : IM_COL32(255, 100, 70, 255);

        if (p.waveform_buf && p.waveform_size > 0) {
            int num_bars = std::max(1, static_cast<int>(wave_w));
            float samples_per_pixel = static_cast<float>(p.waveform_size) / num_bars;
            for (int i = 0; i < num_bars; ++i) {
                int idx = static_cast<int>(i * samples_per_pixel);
                if (idx >= p.waveform_size) idx = p.waveform_size - 1;
                float val   = p.waveform_buf[idx];
                float bar_h = val * wave_h * 0.48f;
                if (bar_h < 0.5f) continue;
                float x   = wave_pos.x + i;
                ImU32 col = val > 0.8f ? wave_color_bright : wave_color;
                draw->AddLine(ImVec2(x, center_y - bar_h), ImVec2(x, center_y + bar_h), col);
            }
        }

        draw->AddRect(wave_pos,
                      ImVec2(wave_pos.x + wave_w, wave_pos.y + wave_h),
                      IM_COL32(70, 65, 55, 255), 4.0f);
        ImGui::Dummy(ImVec2(wave_w, wave_h));

    } else if (p.has_unsaved) {
        // ── Unsaved recording ──
        {
            float avail  = ImGui::GetContentRegionAvail().y;
            float row_h  = ImGui::GetFrameHeight();
            float offset = std::max(0.0f, (avail - row_h) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
        }

        float content_w = ImGui::CalcTextSize("Recording complete").x
                        + ImGui::CalcTextSize("  999.9 s  |  ").x
                        + 100.0f + 80.0f
                        + ImGui::GetStyle().ItemSpacing.x * 3.0f;
        float start_x = (ImGui::GetContentRegionAvail().x - content_w) * 0.5f;
        if (start_x > 0.0f) ImGui::SetCursorPosX(start_x);

        ImGui::TextColored(Theme::Gold(), "Recording complete");
        ImGui::SameLine();
        ImGui::Text("  %.1f s  |  ", p.duration);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        if (ImGui::Button("Save As...", ImVec2(100 * font_scale, 0))) {
            set_state([](RecordingState& st) { st.needs_save = true; });
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Discard", ImVec2(80 * font_scale, 0))) {
            if (p.on_discard) p.on_discard();
            set_state([](RecordingState& st) { st.status_msg = "Recording discarded."; });
        }
        ImGui::PopStyleColor(2);

    } else {
        // ── Ready ──
        {
            float avail  = ImGui::GetContentRegionAvail().y;
            constexpr float row_h = 28.0f;
            float offset = std::max(0.0f, (avail - row_h) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
        }
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.05f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f,  0.1f,  1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("REC", ImVec2(90 * font_scale, 0))) {
            if (p.on_start) p.on_start();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "  Ready to record  |  WAV 16-bit %d Hz",
                           p.sample_rate);
    }

    ImGui::EndChild();
}

void GuiRecording::render_save_dialog(std::function<void(const std::string& dest)> on_save_done) {
    if (!state_.needs_save) return;
    state_.needs_save = false;

    std::string dest = show_save_dialog("recording.wav", "WAV Audio", "wav");
    if (!dest.empty() && on_save_done)
        on_save_done(dest);
}

} // namespace Amplitron
