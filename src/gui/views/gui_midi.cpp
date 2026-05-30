#include "gui/views/gui_midi.h"
#include "gui/theme/theme.h"
#include "midi/midi_manager.h"
#include <imgui.h>
#include <cstdio>

namespace Amplitron {

GuiMidi::GuiMidi(MidiManager& midi)
    : midi_(midi) {}

// ---------------------------------------------------------------------------
// MIDI Settings window
// ---------------------------------------------------------------------------

static const char* target_type_label(MidiTargetType type) {
    switch (type) {
        case MidiTargetType::EffectParam:  return "Param";
        case MidiTargetType::EffectBypass: return "Bypass";
        case MidiTargetType::InputGain:    return "Input Gain";
        case MidiTargetType::OutputGain:   return "Output Gain";
        default: break;  // Fail at compile time if enum is extended
    }
    return "?";  // Unreachable if all cases handled
}

static const char* mode_label(MidiMappingMode mode) {
    switch (mode) {
        case MidiMappingMode::Continuous: return "Continuous";
        case MidiMappingMode::Toggle:     return "Toggle";
        default: break;  // Fail at compile time if enum is extended
    }
    return "?";  // Unreachable if all cases handled
}

void GuiMidi::render(bool& show) {
    ImGui::SetNextWindowSize(ImVec2(620, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("MIDI Settings", &show)) {
        ImGui::End();
        return;
    }

    // --- Port selection ---
    ImGui::TextColored(Theme::Gold(), "MIDI INPUT PORT");
    ImGui::BeginChild("PortSection", ImVec2(0, 70), true);

    // Refresh cached ports on first render or when user clicks refresh
    cached_ports_ = midi_.get_available_ports();

    if (cached_ports_.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No MIDI input devices detected");
    } else {
        int current = midi_.current_port();
        std::string preview = (current >= 0) ? midi_.current_port_name() : "(none)";

        ImGui::SetNextItemWidth(400);
        if (ImGui::BeginCombo("##MidiPort", preview.c_str())) {
            // "(none)" option to close port
            if (ImGui::Selectable("(none)", current < 0)) {
                midi_.close_port();
            }
            for (int i = 0; i < static_cast<int>(cached_ports_.size()); ++i) {
                bool selected = (i == current);
                if (ImGui::Selectable(cached_ports_[i].c_str(), selected)) {
                    midi_.open_port(i);
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        // Re-query ports from RtMidi
        cached_ports_ = midi_.get_available_ports();
    }

    ImGui::EndChild();

    ImGui::Spacing();

    // --- Learn status ---
    if (midi_.is_learning()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s",
                           midi_.learn_status().c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel Learn")) {
            midi_.cancel_learn();
        }
        ImGui::Spacing();
    }

    // --- Mapping table ---
    ImGui::TextColored(Theme::Gold(), "CC MAPPINGS");

    if (ImGui::SmallButton("Add Defaults")) {
        midi_.install_default_mappings();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear All")) {
        midi_.clear_mappings();
    }

    ImGui::Spacing();

    const auto& mappings = midi_.mappings();
    if (mappings.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "No mappings. Right-click any knob to MIDI Learn, "
                           "or click \"Add Defaults\".");
    } else {
        ImGui::BeginChild("MappingTable", ImVec2(0, 0), true);

        // Table header
        if (ImGui::BeginTable("##MidiMappings", 6,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp)) {

            ImGui::TableSetupColumn("CC#", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("Ch", ImGuiTableColumnFlags_WidthFixed, 35);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Param", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("##Del", ImGuiTableColumnFlags_WidthFixed, 25);
            ImGui::TableHeadersRow();

            int remove_index = -1;
            for (int i = 0; i < static_cast<int>(mappings.size()); ++i) {
                const auto& m = mappings[i];
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%d", m.cc_number);

                ImGui::TableNextColumn();
                if (m.midi_channel < 0) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "All");
                } else {
                    ImGui::Text("%d", m.midi_channel + 1);  // Display 1-based
                }

                ImGui::TableNextColumn();
                switch (m.target_type) {
                    case MidiTargetType::EffectParam:
                    case MidiTargetType::EffectBypass:
                        ImGui::Text("%s", m.effect_name.c_str());
                        break;
                    default:
                        ImGui::Text("%s", target_type_label(m.target_type));
                        break;
                }

                ImGui::TableNextColumn();
                switch (m.target_type) {
                    case MidiTargetType::EffectParam:
                        ImGui::Text("%s", m.param_name.c_str());
                        break;
                    case MidiTargetType::EffectBypass:
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(bypass)");
                        break;
                    default:
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "-");
                        break;
                }

                ImGui::TableNextColumn();
                ImGui::Text("%s", mode_label(m.mode));

                ImGui::TableNextColumn();
                char btn_id[16];
                snprintf(btn_id, sizeof(btn_id), "X##%d", i);
                if (ImGui::SmallButton(btn_id)) {
                    remove_index = i;
                }
            }

            ImGui::EndTable();

            if (remove_index >= 0) {
                midi_.remove_mapping(remove_index);
            }
        }

        ImGui::EndChild();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// MIDI Learn menu items (for knob right-click popups)
// ---------------------------------------------------------------------------

bool GuiMidi::render_learn_menu_item(const std::string& effect_name,
                                     const std::string& param_name) {
    // Check if this param already has a mapping
    bool has_mapping = false;
    int mapping_cc = -1;
    for (const auto& m : midi_.mappings()) {
        if (m.target_type == MidiTargetType::EffectParam &&
            m.effect_name == effect_name &&
            m.param_name == param_name) {
            has_mapping = true;
            mapping_cc = m.cc_number;
            break;
        }
    }

    if (has_mapping) {
        char label[64];
        snprintf(label, sizeof(label), "MIDI: CC %d", mapping_cc);
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "%s", label);

        if (ImGui::MenuItem("Remove MIDI Mapping")) {
            midi_.remove_mapping_for_param(effect_name, param_name);
            return true;
        }
    }

    if (ImGui::MenuItem("MIDI Learn")) {
        midi_.start_learn(MidiTargetType::EffectParam, effect_name, param_name);
        return true;
    }
    return false;
}

bool GuiMidi::render_learn_bypass_item(const std::string& effect_name) {
    if (ImGui::MenuItem("MIDI Learn (Bypass)")) {
        midi_.start_learn(MidiTargetType::EffectBypass, effect_name, "");
        return true;
    }
    return false;
}

bool GuiMidi::render_remove_mapping_item(const std::string& effect_name,
                                         const std::string& param_name) {
    int remove_idx = -1;
    const auto& mappings = midi_.mappings();
    for (int i = 0; i < static_cast<int>(mappings.size()); ++i) {
        if (mappings[i].target_type == MidiTargetType::EffectParam &&
            mappings[i].effect_name == effect_name &&
            mappings[i].param_name == param_name) {
            remove_idx = i;
            break;
        }
    }

    if (remove_idx >= 0) {
        if (ImGui::MenuItem("Remove MIDI Mapping")) {
            midi_.remove_mapping(remove_idx);
            return true;
        }
    }
    return false;
}

bool GuiMidi::render_remove_bypass_item(const std::string& effect_name) {
    int remove_idx = -1;
    const auto& mappings = midi_.mappings();
    for (int i = 0; i < static_cast<int>(mappings.size()); ++i) {
        if (mappings[i].target_type == MidiTargetType::EffectBypass &&
            mappings[i].effect_name == effect_name) {
            remove_idx = i;
            break;
        }
    }

    if (remove_idx >= 0) {
        if (ImGui::MenuItem("Remove MIDI Bypass Mapping")) {
            midi_.remove_mapping(remove_idx);
            return true;
        }
    }
    return false;
}

std::string GuiMidi::get_mapping_info(const std::string& effect_name,
                                      const std::string& param_name) const {
    for (const auto& m : midi_.mappings()) {
        if (m.target_type == MidiTargetType::EffectParam &&
            m.effect_name == effect_name &&
            m.param_name == param_name) {
            return "\n\n[MIDI: CC" + std::to_string(m.cc_number) +
                   (m.mode == MidiMappingMode::Toggle ? " Toggle]" : " Range]");
        }
    }
    return "";
}

} // namespace Amplitron
