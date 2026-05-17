#include "gui/gui_snapshots.h"
#include "gui/pedal_board.h"
#include "gui/command.h"
#include "gui/theme.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

namespace Amplitron {

GuiSnapshots::GuiSnapshots(AudioEngine& engine, CommandHistory& history)
    : engine_(engine), history_(history) {}

void GuiSnapshots::save_to_slot(int slot) {
    manager_.save_slot(slot, engine_);
    manager_.set_active_slot(slot);
    std::snprintf(status_msg_, sizeof(status_msg_), "Saved to %s",
                  SnapshotManager::SLOT_LABELS[slot]);
    status_timer_ = STATUS_DISPLAY_SECONDS;
}

void GuiSnapshots::recall_slot(int slot) {
    if (!manager_.has_slot(slot)) return;

    // Capture before-state for undo
    auto before = SnapshotManager::capture(engine_);
    const auto* after_snap = manager_.get_slot(slot);

    history_.execute(std::make_unique<RecallSnapshotCommand>(
        engine_,
        before.effects, before.input_gain, before.output_gain,
        after_snap->effects, after_snap->input_gain, after_snap->output_gain
    ));

    manager_.set_active_slot(slot);
    std::snprintf(status_msg_, sizeof(status_msg_), "Recalled %s",
                  SnapshotManager::SLOT_LABELS[slot]);
    status_timer_ = STATUS_DISPLAY_SECONDS;

    if (pedal_board_) {
        pedal_board_->rebuild_widgets();
    }
}

void GuiSnapshots::render() {
    // Scale the baseline bar size based on actual style metrics
    float bar_height = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetStyle().WindowBorderSize * 2.0f;
    
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

        const bool is_active = (manager_.active_slot() == i);
        const bool is_filled = manager_.has_slot(i);

        // Colour: active = bright gold, filled = subtle tint, empty = very dim
        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.50f, 0.42f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                Theme::GoldHot());
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                Theme::Gold());
        } else if (is_filled) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.22f, 0.20f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.35f, 0.30f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(0.45f, 0.38f, 0.18f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.14f, 0.13f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.20f, 0.19f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                ImVec4(0.25f, 0.23f, 0.19f, 1.0f));
        }

        // Button label: "A##snap_0" etc.
        char btn_id[16];
        std::snprintf(btn_id, sizeof(btn_id), "%s##snap_%d",
                      SnapshotManager::SLOT_LABELS[i], i);

        if (ImGui::Button(btn_id)) {
            if (is_filled) {
                recall_slot(i);
            }
        }

        ImGui::PopStyleColor(3);

        // Tooltip
        if (ImGui::IsItemHovered()) {
            if (is_filled) {
                ImGui::SetTooltip(
                    "Left-click to recall snapshot %s (or Ctrl+%d)\n"
                    "Right-click to overwrite or clear",
                    SnapshotManager::SLOT_LABELS[i], i + 1);
            } else {
                ImGui::SetTooltip(
                    "Slot %s is empty\n"
                    "Right-click to save current board here",
                    SnapshotManager::SLOT_LABELS[i]);
            }
        }

        // Right-click context menu
        char popup_id[24];
        std::snprintf(popup_id, sizeof(popup_id), "SnapCtx_%d", i);
        if (ImGui::BeginPopupContextItem(popup_id)) {
            char save_label[40];
            std::snprintf(save_label, sizeof(save_label),
                          "Save current board to %s",
                          SnapshotManager::SLOT_LABELS[i]);
            if (ImGui::MenuItem(save_label)) {
                save_to_slot(i);
            }
            if (is_filled) {
                char clear_label[24];
                std::snprintf(clear_label, sizeof(clear_label),
                              "Clear %s", SnapshotManager::SLOT_LABELS[i]);
                ImGui::Separator();
                if (ImGui::MenuItem(clear_label)) {
                    manager_.clear_slot(i);
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();

    // Status message or hint text
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
