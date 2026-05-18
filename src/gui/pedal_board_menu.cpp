#include "gui/pedal_board.h"
#include "gui/gui_midi.h"
#include "midi/midi_manager.h"
#include "gui/theme.h"

#include "audio/effects/noise_gate.h"
#include "audio/effects/compressor.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/distortion.h"
#include "audio/effects/equalizer.h"
#include "audio/effects/chorus.h"
#include "audio/effects/phaser.h"
#include "audio/effects/flanger.h"
#include "audio/effects/delay.h"
#include "audio/effects/reverb.h"
#include "audio/effects/looper.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/ir_cabinet.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/wah.h"
#include "audio/effects/octaver.h"
#include "audio/effects/pitch_shifter.h"

#include <imgui.h>
#include <cstdio>

namespace Amplitron {

void PedalBoard::render_add_pedal_menu() {
    if (ImGui::Button("+ Add Pedal")) {
        ImGui::OpenPopup("AddPedalPopup");
    }

    if (ImGui::BeginPopup("AddPedalPopup")) {
        ImGui::TextColored(Theme::Gold(), "DRIVE");
        if (ImGui::MenuItem("Overdrive")) {
            add_effect_and_show(std::make_shared<Overdrive>());
        }
        if (ImGui::MenuItem("Distortion")) {
            add_effect_and_show(std::make_shared<Distortion>());
        }

        ImGui::Separator();
        ImGui::TextColored(Theme::Live(), "DYNAMICS");
        if (ImGui::MenuItem("Noise Gate")) {
            add_effect_and_show(std::make_shared<NoiseGate>());
        }
        if (ImGui::MenuItem("Compressor")) {
            add_effect_and_show(std::make_shared<Compressor>());
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.35f, 0.60f, 0.95f, 1.0f), "MODULATION");
        if (ImGui::MenuItem("Chorus")) {
            add_effect_and_show(std::make_shared<Chorus>());
        }
        if (ImGui::MenuItem("Phaser")) {
            add_effect_and_show(std::make_shared<Phaser>());
        }
        if (ImGui::MenuItem("Flanger")) {
            add_effect_and_show(std::make_shared<Flanger>());
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.65f, 0.35f, 0.95f, 1.0f), "TIME");
        if (ImGui::MenuItem("Delay")) {
            add_effect_and_show(std::make_shared<Delay>());
        }
        if (ImGui::MenuItem("Reverb")) {
            add_effect_and_show(std::make_shared<Reverb>());
        }
        if (ImGui::MenuItem("Looper")) {
            add_effect_and_show(std::make_shared<Looper>());
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.30f, 0.75f, 0.60f, 1.0f), "FILTER");
        if (ImGui::MenuItem("Wah")) {
            add_effect_and_show(std::make_shared<WahPedal>());
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.85f, 0.40f, 0.55f, 1.0f), "PITCH");
        if (ImGui::MenuItem("Octaver")) {
            add_effect_and_show(std::make_shared<Octaver>());
        }
        if (ImGui::MenuItem("Pitch Shifter")) {
            add_effect_and_show(std::make_shared<PitchShifter>());
        }

        ImGui::Separator();
        ImGui::TextColored(Theme::GoldDim(), "TONE");
        if (ImGui::MenuItem("Equalizer")) {
            add_effect_and_show(std::make_shared<Equalizer>());
        }
        if (ImGui::MenuItem("Cabinet Sim")) {
            add_effect_and_show(std::make_shared<CabinetSim>());
        }
        if (ImGui::MenuItem("IR Cabinet")) {
            add_effect_and_show(std::make_shared<IRCabinet>());
        }

        ImGui::EndPopup();
    }
}


void PedalBoard::render_amp_selector() {
    const auto& models = get_amp_models();
    int amp_idx = find_amp_index();

    if (amp_idx < 0) {
        auto amp = std::make_shared<AmpSimulator>();
        amp->params()[0].value = 0.0f;
        engine_.add_effect(amp);
        rebuild_widgets();
        amp_idx = find_amp_index();
    }

    const char* current_label = "Amp";
    int current_model = 0;
    if (amp_idx >= 0) {
        auto& amp_fx = engine_.effects()[amp_idx];
        current_model = static_cast<int>(amp_fx->params()[0].value);
        if (current_model >= 0 && current_model < static_cast<int>(models.size())) {
            current_label = models[current_model].name;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.18f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.25f, 0.10f, 1.0f));
    char amp_label[64];
    std::snprintf(amp_label, sizeof(amp_label), "Amp: %s", current_label);
    if (ImGui::Button(amp_label)) {
        ImGui::OpenPopup("AmpSelectorPopup");
    }
    ImGui::PopStyleColor(2);

    if (ImGui::BeginPopup("AmpSelectorPopup")) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.20f, 1.0f), "AMP MODEL");
        for (int m = 0; m < static_cast<int>(models.size()); ++m) {
            bool is_selected = (current_model == m);
            if (ImGui::MenuItem(models[m].name, models[m].inspiration, is_selected)) {
                if (amp_idx >= 0) {
                    engine_.effects()[amp_idx]->params()[0].value = static_cast<float>(m);
                }
            }
        }
        ImGui::EndPopup();
    }
}

// ============================================================
// MIDI MENU — QUICK STATUS AND ACTIONS
// ============================================================
void PedalBoard::render_midi_menu() {
    if (!gui_midi_) return;
    auto& midi = gui_midi_->manager();

    if (ImGui::Button("MIDI")) {
        ImGui::OpenPopup("MidiMenuPopup");
    }

    if (ImGui::BeginPopup("MidiMenuPopup")) {
        // Device status
        if (midi.is_port_open()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● Connected");
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", midi.current_port_name().c_str());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "● Disconnected");
        }

        ImGui::Separator();

        // Learn mode status
        if (midi.is_learning()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⚡ Learn Mode Active");
            if (ImGui::MenuItem("Cancel Learn Mode", "Esc")) {
                midi.cancel_learn();
            }
        } else {
            ImGui::TextDisabled("Right-click any knob to MIDI learn");
        }

        ImGui::Separator();

        // Quick actions
        if (ImGui::MenuItem("Clear All Mappings")) {
            show_confirm_midi_clear_ = true;
        }

        if (ImGui::MenuItem("Save Config")) {
            midi.save_config();
        }

        if (ImGui::MenuItem("Load Config")) {
            midi.load_config();
        }

        ImGui::Separator();

        // Show active mappings count
        auto& mappings = midi.mappings();
        ImGui::TextDisabled("%zu active mappings", mappings.size());

        ImGui::EndPopup();
    }
}

} // namespace Amplitron
