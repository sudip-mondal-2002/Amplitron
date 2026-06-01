#include "gui/views/gui_snapshots.h"
#include "gui/theme/theme.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

namespace Amplitron {

void GuiSnapshots::render() {
    const SnapshotsProps& p = props_;

    float bar_height = ImGui::GetFrameHeight()
                     + ImGui::GetStyle().WindowPadding.y * 2.0f
                     + ImGui::GetStyle().WindowBorderSize * 2.0f;

    ImGui::BeginChild("SnapshotBar", ImVec2(0, bar_height), true,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Vertically center the button row
    {
        float avail_y = ImGui::GetContentRegionAvail().y;
        float row_h   = ImGui::GetFrameHeight();
        float pad_y   = std::max(0.0f, (avail_y - row_h) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad_y);
    }

    ImGui::TextColored(Theme::TextSecondary(), "SNAPSHOTS");

    for (int i = 0; i < SnapshotManager::NUM_SLOTS; ++i) {
        ImGui::SameLine();

        const auto& slot      = p.slots[i];
        const bool is_active  = slot.is_active;
        const bool is_filled  = slot.is_filled;
        const char* lbl       = slot.label;

        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.42f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::GoldHot());
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  Theme::Gold());
        } else if (is_filled) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.20f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.30f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.38f, 0.18f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f, 0.13f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.19f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.23f, 0.19f, 1.0f));
        }

        char btn_id[16];
        std::snprintf(btn_id, sizeof(btn_id), "%s##snap_%d", lbl, i);

        if (ImGui::Button(btn_id)) {
            if (is_filled) {
                if (p.on_recall_slot) p.on_recall_slot(i);
                std::snprintf(status_msg_, sizeof(status_msg_), "Recalled %s", lbl);
                status_timer_ = STATUS_DISPLAY_SECONDS;
            }
        }

        ImGui::PopStyleColor(3);

        // Tooltip
        if (ImGui::IsItemHovered()) {
            if (is_filled)
                ImGui::SetTooltip("Left-click to recall snapshot %s (or Ctrl+%d)\nRight-click to overwrite or clear", lbl, i + 1);
            else
                ImGui::SetTooltip("Slot %s is empty\nRight-click to save current board here", lbl);
        }

        // Right-click context menu
        char popup_id[24];
        std::snprintf(popup_id, sizeof(popup_id), "SnapCtx_%d", i);
        if (ImGui::BeginPopupContextItem(popup_id)) {
            char save_label[40];
            std::snprintf(save_label, sizeof(save_label), "Save current board to %s", lbl);
            if (ImGui::MenuItem(save_label)) {
                if (p.on_save_slot) p.on_save_slot(i);
                std::snprintf(status_msg_, sizeof(status_msg_), "Saved to %s", lbl);
                status_timer_ = STATUS_DISPLAY_SECONDS;
            }
            if (is_filled) {
                char clear_label[24];
                std::snprintf(clear_label, sizeof(clear_label), "Clear %s", lbl);
                ImGui::Separator();
                if (ImGui::MenuItem(clear_label)) {
                    if (p.on_clear_slot) p.on_clear_slot(i);
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();

    if (status_timer_ > 0.0f) {
        float alpha = std::min(status_timer_, 1.0f);
        ImGui::TextColored(ImVec4(0.90f, 0.78f, 0.39f, alpha), "  %s", status_msg_);
        status_timer_ -= ImGui::GetIO().DeltaTime;
    } else {
        ImGui::TextColored(Theme::TextDim(),
            "  Left-click to recall  |  Right-click to save / clear  |  Ctrl+1-4 to recall");
    }

    ImGui::EndChild();
}

} // namespace Amplitron
