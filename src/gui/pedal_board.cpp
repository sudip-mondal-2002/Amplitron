#include "gui/pedal_board.h"
#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include "gui/command.h"
#include "audio/effects/amp_simulator.h"
#include "gui/gui_midi.h"
#include "midi/midi_manager.h"
#include "gui/gui_graph_state.h"

#include <cstring>
#include <imgui.h>
#include <set>
#include <algorithm>

namespace Amplitron {

/** @brief Construct PedalBoard and build initial widget list from engine state. */
PedalBoard::PedalBoard(AudioEngine& engine, CommandHistory& history, GuiMidi* gui_midi)
    : engine_(engine), history_(history), gui_midi_(gui_midi) {
    rebuild_widgets();
}

/** @brief Default destructor. */
PedalBoard::~PedalBoard() = default;

/** @brief Recreate PedalWidget list to match the engine's current effect chain. */
void PedalBoard::rebuild_widgets() {
    std::set<Effect*> prev_visible;
    for (int idx : visible_indices_) {
        if (idx >= 0 && idx < static_cast<int>(widgets_.size())) {
            prev_visible.insert(widgets_[idx]->get_effect().get());
        }
    }

    widgets_.clear();
    visible_indices_.clear();
    auto& effects = engine_.effects();

    int amp_idx = find_amp_index();

    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        auto w = std::make_unique<PedalWidget>(engine_, effects[i], i);
        w->set_history(&history_);
        w->set_gui_midi(gui_midi_);
        widgets_.push_back(std::move(w));

        Effect* ptr = effects[i].get();
        bool is_amp = (amp_idx >= 0 && i == amp_idx);
        bool is_post_amp = (amp_idx >= 0 && i > amp_idx);

        if (is_post_amp) continue;

        if (prev_visible.count(ptr)) {
            visible_indices_.insert(i);
        } else if (effects[i]->is_enabled() || is_amp) {
            visible_indices_.insert(i);
        }
    }
}

/** @brief Find the index of the current AmpSimulator in the effect chain (-1 if none). */
int PedalBoard::find_amp_index() const {
    auto& fx = engine_.effects();
    for (int i = 0; i < static_cast<int>(fx.size()); ++i) {
        if (std::strcmp(fx[i]->name(), "Amp Sim") == 0) return i;
    }
    return -1;
}

/** @brief Render the toolbar (add/reset) and the scrollable signal chain area. */
void PedalBoard::render() {
    float bar_height = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetStyle().WindowBorderSize * 2.0f;
    ImGui::BeginChild("PedalToolbar", ImVec2(0, bar_height), true,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    {
        float avail = ImGui::GetContentRegionAvail().y;
        float row_h = ImGui::GetFrameHeight();
        float offset = std::max(0.0f, (avail - row_h) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset);
    }
    
    render_add_pedal_menu();
    ImGui::SameLine();

    if (ImGui::Button("Reset All")) {
        show_confirm_reset_ = true;
    }
    ImGui::SameLine();

    if (ImGui::Button("Clear All")) {
        show_confirm_clear_ = true;
    }
    ImGui::SameLine();

    auto& ui_state = GuiGraphState::get_instance();
    if (ui_state.hand_tool_active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        if (ImGui::Button("Hand Tool [Active]")) {
            ui_state.hand_tool_active = false;
        }
        ImGui::PopStyleColor();
    } else {
        if (ImGui::Button("Hand Tool")) {
            ui_state.hand_tool_active = true;
        }
    }
    ImGui::SameLine();

    render_midi_menu();
    ImGui::SameLine();

    if (show_confirm_reset_) {
        ImGui::OpenPopup("Confirm Reset##Modal");
        show_confirm_reset_ = false;
    }
    if (show_confirm_clear_) {
        ImGui::OpenPopup("Confirm Clear##Modal");
        show_confirm_clear_ = false;
    }

    if (ImGui::BeginPopupModal("Confirm Reset##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to reset ALL parameters to their default values?\nThis will affect every pedal on the board.");
        ImGui::Separator();
        if (ImGui::Button("Reset", ImVec2(120, 0))) {
            history_.execute(std::make_unique<ResetAllCommand>(engine_));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Confirm Clear##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to remove ALL pedals from the signal chain?\nThis cannot be undone easily if you have many complex settings.");
        ImGui::Separator();
        if (ImGui::Button("Clear All", ImVec2(120, 0))) {
            history_.execute(std::make_unique<ClearAllCommand>(engine_));
            rebuild_widgets();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_confirm_midi_clear_) {
        ImGui::OpenPopup("Confirm MIDI Clear##Modal");
        show_confirm_midi_clear_ = false;
    }

    if (ImGui::BeginPopupModal("Confirm MIDI Clear##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to clear ALL MIDI CC mappings?");
        ImGui::TextColored(Theme::Gold(), "This action cannot be undone.");
        ImGui::Spacing();

        if (ImGui::Button("Clear All", ImVec2(120, 0))) {
            if (gui_midi_) {
                gui_midi_->manager().clear_mappings();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    render_amp_selector();

    ImGui::SameLine();
    int pedal_count = static_cast<int>(engine_.effects().size());
    ImGui::TextColored(Theme::TextSecondary(),
        "  %d effects | Drag headers to route", pedal_count);

    ImGui::EndChild();

    // Canvas Window Viewport Layout Setup
    ImGui::BeginChild("PedalArea", ImVec2(0, 0), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    render_signal_chain();

    ImGui::EndChild();
}

/** @brief Add an effect to the chain via undo system, rebuild widgets, and make it visible. */
void PedalBoard::add_effect_and_show(std::shared_ptr<Effect> effect) {
    history_.execute(std::make_unique<AddEffectCommand>(engine_, std::move(effect)));
    rebuild_widgets();
}

} // namespace Amplitron