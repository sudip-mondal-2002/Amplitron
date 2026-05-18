#include "gui/pedal_board.h"
#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include "gui/command.h"

#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace Amplitron {

void PedalBoard::render_signal_chain() {
    std::vector<int> visible;
    for (int idx : visible_indices_) {
        if (idx >= 0 && idx < static_cast<int>(widgets_.size())) {
            visible.push_back(idx);
        }
    }

    if (visible.empty()) {
        ImGui::SetCursorPos(ImVec2(
            ImGui::GetWindowWidth() / 2 - 150,
            ImGui::GetWindowHeight() / 2 - 30
        ));
        ImGui::TextColored(Theme::TextDim(),
            "No pedals in chain.\nClick '+ Add Pedal' to get started.");
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    float line_y = origin.y + 160;
    // Compute the actual content width of the chain. Keeping this tight prevents
    // the horizontal scrollbar from appearing due to a few extra pixels of padding.
    float total_width = 20.0f + static_cast<float>(visible.size()) * 195.0f;
    dl->AddLine(
        ImVec2(origin.x, line_y),
        ImVec2(origin.x + total_width, line_y),
        Theme::CHAIN_LINE, 3.0f
    );

    dl->AddCircleFilled(ImVec2(origin.x + 5, line_y), 6, Theme::CHAIN_JACK);
    dl->AddCircle(ImVec2(origin.x + 5, line_y), 6, Theme::BORDER_DARK, 0, 1.5f);

    float pedal_x = origin.x + 20;
    int remove_idx = -1;
    bool needs_rebuild = false;

    for (int vi = 0; vi < static_cast<int>(visible.size()); ++vi) {
        int i = visible[vi];
        ImVec2 pedal_min = ImVec2(pedal_x, origin.y + 5);

        ImGui::SetCursorScreenPos(pedal_min);
        char dnd_id[32];
        snprintf(dnd_id, sizeof(dnd_id), "##dnd_%d", i);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(dnd_id, ImVec2(Theme::PEDAL_WIDTH, Theme::PEDAL_HEIGHT));

        bool is_amp = std::strcmp(widgets_[i]->get_effect()->name(), "Amp Sim") == 0;
        if (!is_amp) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("PEDAL_REORDER", &i, sizeof(int));
                ImGui::Text("Move %s", widgets_[i]->get_effect()->name());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PEDAL_REORDER")) {
                    int source_idx = *static_cast<const int*>(payload->Data);
                    if (source_idx != i) {
                        history_.execute(std::make_unique<ReorderEffectCommand>(engine_, source_idx, i));
                        needs_rebuild = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        ImGui::SetCursorScreenPos(pedal_min);
        if (widgets_[i]->render()) {
            remove_idx = i;
        }

        if (vi < static_cast<int>(visible.size()) - 1) {
            float dot_x = pedal_x + 190;
            dl->AddCircleFilled(ImVec2(dot_x, line_y), 4, Theme::CHAIN_DOT);
        }

        pedal_x += 195;
    }

    dl->AddCircleFilled(ImVec2(pedal_x, line_y), 6, Theme::CHAIN_JACK);
    dl->AddCircle(ImVec2(pedal_x, line_y), 6, Theme::BORDER_DARK, 0, 1.5f);

    if (remove_idx >= 0) {
        visible_indices_.erase(remove_idx);
        history_.execute(std::make_unique<RemoveEffectCommand>(engine_, remove_idx));
        needs_rebuild = true;
    }

    if (needs_rebuild) {
        rebuild_widgets();
    }

    // Reserve space so the child window's content size matches the drawn chain.
    // Add only a small tail so the end jack isn't flush against the edge.
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::Dummy(ImVec2(total_width + 8.0f, 340));
}

} // namespace Amplitron
