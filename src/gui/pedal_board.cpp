#include "gui/pedal_board.h"
#include "gui/pedal_widget.h"
#include "gui/theme.h"
#include "gui/command.h"
#include "audio/effects/amp_simulator.h"
#include "gui/gui_midi.h"
#include "midi/midi_manager.h"

#include <cstring>
#include <imgui.h>
#include <set>

namespace Amplitron {

/** @brief Construct PedalBoard and build initial widget list from engine state. */
PedalBoard::PedalBoard(AudioEngine& engine, CommandHistory& history, GuiMidi* gui_midi)
    : engine_(engine), history_(history), gui_midi_(gui_midi) {
    rebuild_widgets();
}

/** @brief Default destructor. */
PedalBoard::~PedalBoard() = default;

/** @brief Recreate PedalWidget list to match the engine's current effect chain.
 *  Visibility is preserved by effect pointer identity so that a footswitch-off pedal
 *  stays on the board.  Brand-new effects (unrecognised pointers, e.g. after a preset
 *  load or an add) are shown only if they are currently enabled or are the Amp Sim. */
void PedalBoard::rebuild_widgets() {
    // Snapshot which effect pointers are currently on the board before clearing.
    std::set<Effect*> prev_visible;
    for (int idx : visible_indices_) {
        if (idx >= 0 && idx < static_cast<int>(widgets_.size())) {
            prev_visible.insert(widgets_[idx]->get_effect().get());
        }
    }

    widgets_.clear();
    visible_indices_.clear();
    auto& effects = engine_.effects();

    // Determine amp position so post-amp effects are never shown on the board.
    int amp_idx = find_amp_index();

    for (int i = 0; i < static_cast<int>(effects.size()); ++i) {
        auto w = std::make_unique<PedalWidget>(engine_, effects[i], i);
        w->set_history(&history_);
        w->set_gui_midi(gui_midi_);
        widgets_.push_back(std::move(w));

        Effect* ptr = effects[i].get();
        bool is_amp = (amp_idx >= 0 && i == amp_idx);
        bool is_post_amp = (amp_idx >= 0 && i > amp_idx);

        // Post-amp effects are never shown on the pedalboard.
        if (is_post_amp) continue;

        if (prev_visible.count(ptr)) {
            // Effect was already on the board — keep it visible regardless of enabled state.
            visible_indices_.insert(i);
        } else if (effects[i]->is_enabled() || is_amp) {
            // New effect (add pedal, preset load, initial build) — show only if enabled.
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
    // Calculate robust dynamic height based on actual style metrics
    float bar_height = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetStyle().WindowBorderSize * 2.0f;
    ImGui::BeginChild("PedalToolbar", ImVec2(0, bar_height), true,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Vertically center the single button row so top and bottom padding are equal
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

    render_midi_menu();
    ImGui::SameLine();

    // --- Confirmation Modals ---
    // OpenPopup must only be called on the transition frame, not every frame.
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

    // Amp selector (separate dropdown to switch model)
    render_amp_selector();

    ImGui::SameLine();
    int pedal_count = static_cast<int>(engine_.effects().size());
    ImGui::TextColored(Theme::TextSecondary(),
        "  %d effects | Drag knobs to adjust", pedal_count);

    ImGui::EndChild();

    // Pedal board area with horizontal scroll
    ImGui::BeginChild("PedalArea", ImVec2(0, 0), true,
        ImGuiWindowFlags_HorizontalScrollbar);

    render_signal_chain();

    ImGui::EndChild();
}

/** @brief Add an effect to the chain via undo system, rebuild widgets, and make it visible. */
void PedalBoard::add_effect_and_show(std::shared_ptr<Effect> effect) {
    history_.execute(std::make_unique<AddEffectCommand>(engine_, std::move(effect)));
    rebuild_widgets();
}


} // namespace Amplitron
