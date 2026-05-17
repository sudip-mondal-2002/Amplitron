#include "gui/gui_recording.h"
#include "gui/file_dialog.h"
#include "gui/theme.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Amplitron {

void GuiRecording::render_controls() {
    auto& rec = engine_.recorder();
    bool is_recording = rec.is_recording();
    bool is_paused = rec.is_paused();
    bool has_unsaved = rec.has_unsaved();

    // Robustly calculate baseline heights using actual ImGui metrics
    // to prevent clipping when padding or font sizes change independently.
    float base_h = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetStyle().WindowBorderSize * 2.0f;
    float panel_height = is_recording ? (base_h + 80.0f) : base_h;
    
    // Kept font_scale for waveform scaling
    float font_scale = ImGui::GetFontSize() / 14.0f;
    // FIX: Enforce fixed-height layout properties with strict scrollbars suppression flags
    ImGui::BeginChild("RecordingPanel", ImVec2(0, panel_height), true,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (is_recording) {
        // === RECORDING ACTIVE ===

        // Record/Pause button
        if (is_paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("RESUME", ImVec2(80 * font_scale, 0))) {
                rec.resume();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.5f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button("PAUSE", ImVec2(80 * font_scale, 0))) {
                rec.pause();
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine();

        // Stop button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("STOP", ImVec2(80 * font_scale, 0))) {
            rec.stop();
            show_recording_save_ = true;
            recording_save_pending_ = true;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        // Blinking REC indicator
        float t = static_cast<float>(ImGui::GetTime());
        if (is_paused) {
            ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.2f, 1.0f), "  PAUSED");
        } else {
            float blink = (std::sin(t * 4.0f) > 0.0f) ? 1.0f : 0.3f;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.15f, 0.15f, blink));
            ImGui::Text("  REC");
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Timer MM:SS.ms
        float duration = rec.get_duration();
        int mins = static_cast<int>(duration) / 60;
        int secs = static_cast<int>(duration) % 60;
        int ms = static_cast<int>((duration - static_cast<int>(duration)) * 10);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f),
                           "  %02d:%02d.%d", mins, secs, ms);

        ImGui::SameLine();

        // Peak meter (compact)
        float peak = rec.get_current_peak();
        ImGui::TextColored(peak > 0.9f ? ImVec4(1, 0.2f, 0.2f, 1) :
                           peak > 0.6f ? ImVec4(1, 0.8f, 0.2f, 1) :
                                         ImVec4(0.2f, 0.8f, 0.2f, 1),
                           "  Peak: %.1f dB",
                           peak > 0.0001f ? 20.0f * std::log10(peak) : -96.0f);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
        int64_t file_bytes = rec.get_samples_written() * 2 * rec.get_channels(); // 16-bit PCM
        if (file_bytes > 1024 * 1024)
            ImGui::Text("%.1f MB", file_bytes / (1024.0f * 1024.0f));
        else
            ImGui::Text("%.0f KB", file_bytes / 1024.0f);

        // === WAVEFORM DISPLAY ===
        ImGui::Spacing();
        rec.get_waveform(rec_waveform_buf_, Recorder::WAVEFORM_SIZE);

        ImVec2 wave_pos = ImGui::GetCursorScreenPos();
        float wave_w = ImGui::GetContentRegionAvail().x;
        float wave_h = 50.0f * font_scale; 

        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Dark background for waveform
        draw->AddRectFilled(wave_pos,
                            ImVec2(wave_pos.x + wave_w, wave_pos.y + wave_h),
                            IM_COL32(20, 18, 16, 255), 4.0f);

        // Center line
        float center_y = wave_pos.y + wave_h * 0.5f;
        draw->AddLine(ImVec2(wave_pos.x, center_y),
                      ImVec2(wave_pos.x + wave_w, center_y),
                      IM_COL32(60, 55, 48, 255));

        // Waveform bars (mirrored around center)
        ImU32 wave_color = is_paused ? IM_COL32(180, 160, 50, 200)
                                      : IM_COL32(200, 80, 60, 220);
        ImU32 wave_color_bright = is_paused ? IM_COL32(220, 200, 80, 255)
                                             : IM_COL32(255, 100, 70, 255);

        int num_bars = std::max(1, static_cast<int>(wave_w));
        float samples_per_pixel = static_cast<float>(Recorder::WAVEFORM_SIZE) / num_bars;

        for (int i = 0; i < num_bars; ++i) {
            int idx = static_cast<int>(i * samples_per_pixel);
            if (idx >= Recorder::WAVEFORM_SIZE) idx = Recorder::WAVEFORM_SIZE - 1;
            float val = rec_waveform_buf_[idx];
            float bar_h = val * wave_h * 0.48f;
            if (bar_h < 0.5f) continue;

            float x = wave_pos.x + i;
            ImU32 col = val > 0.8f ? wave_color_bright : wave_color;
            draw->AddLine(ImVec2(x, center_y - bar_h),
                          ImVec2(x, center_y + bar_h), col);
        }

        // Border
        draw->AddRect(wave_pos,
                      ImVec2(wave_pos.x + wave_w, wave_pos.y + wave_h),
                      IM_COL32(70, 65, 55, 255), 4.0f);

        ImGui::Dummy(ImVec2(wave_w, wave_h));

    } else if (has_unsaved) {
        // === UNSAVED RECORDING ===
        {
            float avail = ImGui::GetContentRegionAvail().y;
            float row_h = ImGui::GetFrameHeight();
            float offset = std::max(0.0f, (avail - row_h) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
        }

        // FIX: Centrally align the saved recording context blocks
        {
            float content_w = ImGui::CalcTextSize("Recording complete").x;
            content_w += ImGui::CalcTextSize("  999.9 s  |  ").x;
            content_w += 100.0f + 80.0f; // Exact fixed size width budgets for the Save/Discard buttons
            content_w += (ImGui::GetStyle().ItemSpacing.x * 3.0f);

            float start_x = (ImGui::GetContentRegionAvail().x - content_w) * 0.5f;
            if (start_x > 0.0f) ImGui::SetCursorPosX(start_x);
        }

        ImGui::TextColored(Theme::Gold(), "Recording complete");
        ImGui::SameLine();
        ImGui::Text("  %.1f s  |  ", rec.get_duration());
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        if (ImGui::Button("Save As...", ImVec2(100 * font_scale, 0))) {
            show_recording_save_ = true;
            recording_save_pending_ = true;
        }
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("Discard", ImVec2(80 * font_scale, 0))) {
            rec.discard();
            status_msg_ = "Recording discarded.";
        }
        ImGui::PopStyleColor(2);

    } else {
        // === READY STATE ===
        {
            float avail = ImGui::GetContentRegionAvail().y;
            constexpr float row_h = 28.0f;
            float offset = std::max(0.0f, (avail - row_h) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.05f, 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button("REC", ImVec2(90 * font_scale, 0))) {
            std::string filepath = Recorder::generate_filename();
            rec.start(filepath, engine_.get_sample_rate());
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "  Ready to record  |  WAV 16-bit %d Hz",
                           engine_.get_sample_rate());
    }

    ImGui::EndChild();
}

void GuiRecording::render_save_dialog(bool& show) {
    if (!recording_save_pending_) {
        show = false;
        return;
    }

    // Launch native save dialog (runs on this frame, blocks briefly)
    recording_save_pending_ = false;
    show = false;

    auto& rec = engine_.recorder();
    std::string dest = show_save_dialog("recording.wav", "WAV Audio", "wav");

    if (!dest.empty()) {
        if (rec.save_to(dest)) {
            rec.write_metadata(dest, engine_);
            status_msg_ = "Saved: " + dest;
        } else {
            status_msg_ = "Failed to save recording.";
        }
    }
}

} // namespace Amplitron
