#include "gui/views/gui_tuner.h"
#include "gui/theme/theme.h"
#include "common.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace Amplitron {

void GuiTuner::render(bool& show) {
    ImGui::SetNextWindowSize(ImVec2(360, 320), ImGuiCond_FirstUseEver);
    bool open = show;
    if (!ImGui::Begin("Chromatic Tuner", &open)) {
        if (!open) show = false;
        ImGui::End();
        return;
    }
    if (!open) {
        show = false;
        ImGui::End();
        return;
    }


    const TunerProps& p = props_;
    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       win_w = ImGui::GetContentRegionAvail().x;

    if (p.has_signal && p.note_idx >= 0 && p.note_name_fn) {
        // ── Note name (large, centered) ──
        char note_buf[16];
        std::snprintf(note_buf, sizeof(note_buf), "%s%d", p.note_name_fn(p.note_idx), p.octave);
        ImVec2 note_size = ImGui::CalcTextSize(note_buf);
        float  scale     = 3.0f;
        float  note_w    = note_size.x * scale;
        ImVec2 note_pos  = ImGui::GetCursorScreenPos();
        note_pos.x += (win_w - note_w) * 0.5f;
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale,
                    note_pos, Theme::TEXT_PRIMARY, note_buf);
        ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * scale + 8));

        // ── Cents indicator ──
        float abs_cents  = std::fabs(p.cents);
        ImVec4 cents_col = (abs_cents < 2.0f)
                           ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)
                           : (abs_cents < 15.0f)
                               ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                               : ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
        char cents_buf[32];
        std::snprintf(cents_buf, sizeof(cents_buf), "%+.1f cents", p.cents);
        ImVec2 cents_size = ImGui::CalcTextSize(cents_buf);
        ImGui::SetCursorPosX((win_w - cents_size.x) * 0.5f);
        ImGui::TextColored(cents_col, "%s", cents_buf);

        ImGui::Spacing();

        // ── Cents deviation bar ──
        float  bar_w   = win_w - 40;
        float  bar_h   = 14;
        ImVec2 bar_pos = ImGui::GetCursorScreenPos();
        bar_pos.x += 20;
        dl->AddRectFilled(bar_pos, ImVec2(bar_pos.x + bar_w, bar_pos.y + bar_h),
                          Theme::KNOB_BG, 4.0f);

        float cx = bar_pos.x + bar_w * 0.5f;
        dl->AddLine(ImVec2(cx, bar_pos.y - 2), ImVec2(cx, bar_pos.y + bar_h + 2),
                    Theme::TEXT_DIM, 2.0f);

        float needle_norm = clamp(p.cents / 50.0f, -1.0f, 1.0f);
        float needle_x    = cx + needle_norm * (bar_w * 0.5f);
        ImU32 needle_col  = ImGui::ColorConvertFloat4ToU32(cents_col);
        dl->AddRectFilled(ImVec2(needle_x - 4, bar_pos.y - 3),
                          ImVec2(needle_x + 4, bar_pos.y + bar_h + 3),
                          needle_col, 3.0f);
        ImGui::Dummy(ImVec2(0, bar_h + 12));

        // ── Frequency ──
        char freq_buf[32];
        std::snprintf(freq_buf, sizeof(freq_buf), "%.1f Hz", p.freq);
        float freq_w = ImGui::CalcTextSize(freq_buf).x;
        ImGui::SetCursorPosX((win_w - freq_w) * 0.5f);
        ImGui::TextColored(Theme::TextSecondary(), "%s", freq_buf);

    } else {
        // ── No signal ──
        const char* dash      = "---";
        ImVec2      dash_size = ImGui::CalcTextSize(dash);
        float       scale     = 3.0f;
        ImVec2      pos       = ImGui::GetCursorScreenPos();
        pos.x += (win_w - dash_size.x * scale) * 0.5f;
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, pos, Theme::TEXT_DIM, dash);
        ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * scale + 8));

        const char* waiting   = "Play a note...";
        ImVec2      wt_size   = ImGui::CalcTextSize(waiting);
        ImGui::SetCursorPosX((win_w - wt_size.x) * 0.5f);
        ImGui::TextColored(Theme::TextDim(), "%s", waiting);
        ImGui::Dummy(ImVec2(0, 40));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Mute toggle ──
    if (p.mute_on) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.2f, 0.2f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.4f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.2f, 1.0f));
    }
    float btn_w = 140;
    ImGui::SetCursorPosX((win_w - btn_w) * 0.5f);
    if (ImGui::Button(p.mute_on ? "MUTE ON" : "MUTE OFF", ImVec2(btn_w, 30))) {
        if (p.on_mute_changed) p.on_mute_changed(!p.mute_on);
    }
    ImGui::PopStyleColor(2);

    // ── A4 Reference ──
    ImGui::Spacing();
    float a4_ref = p.a4_ref;
    ImGui::SetNextItemWidth(win_w - 20);
    if (ImGui::SliderFloat("A4 Reference", &a4_ref, 430.0f, 450.0f, "%.0f Hz")) {
        if (p.on_a4_ref_changed) p.on_a4_ref_changed(a4_ref);
    }

    ImGui::End();
}

} // namespace Amplitron
