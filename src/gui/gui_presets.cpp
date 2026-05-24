#include "gui/gui_presets.h"
#include "gui/pedal_board.h"
#include "gui/command.h"
#include "gui/theme.h"
#include "preset_json.h"
#include "audio/effects/cabinet_sim.h"
#include <cstring>
#include <imgui.h>
#include <cstdio>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

extern "C" {
EMSCRIPTEN_KEEPALIVE void load_preset_callback(uintptr_t gui_ptr, const char* path) {
    if (gui_ptr && path) {
        auto* gui = reinterpret_cast<Amplitron::GuiPresets*>(gui_ptr);
        gui->load_preset_by_path(path);
    }
}
}
#endif

namespace Amplitron {

/**
 * @brief Capture the current engine state into a PresetData snapshot.
 * @param engine The audio engine whose current setting should be captured.
 * @return PresetData representing the live engine configuration.
 */
static PresetData capture_current_state(AudioEngine& engine) {
    PresetData preset;
    preset.input_gain = engine.get_input_gain();
    preset.output_gain = engine.get_output_gain();

    for (auto& fx : engine.effects()) {
        PresetData::EffectData fd;
        fd.type = fx->name();
        fd.enabled = fx->is_enabled();
        fd.mix = fx->get_mix();
        for (auto& p : fx->params()) {
            fd.params.push_back({p.name, p.value});
        }

        if (std::strcmp(fx->name(), "Cabinet") == 0) {
            auto* cab = dynamic_cast<CabinetSim*>(fx.get());
            if (cab && cab->has_ir()) {
                fd.metadata["ir_path"] = cab->ir_path();
            }
        }

        preset.effects.push_back(std::move(fd));
    }

    return preset;
}

/**
 * @brief Compare two effect snapshots for exact equality.
 * @param a First effect snapshot.
 * @param b Second effect snapshot.
 * @return true if the effect data are identical.
 */
static bool equal_effect_data(const PresetData::EffectData& a,
                              const PresetData::EffectData& b) {
    if (a.type != b.type || a.enabled != b.enabled || a.mix != b.mix) return false;
    if (a.params.size() != b.params.size()) return false;
    if (a.metadata != b.metadata) return false;
    for (size_t i = 0; i < a.params.size(); ++i) {
        if (a.params[i] != b.params[i]) return false;
    }
    return true;
}

/**
 * @brief Compare two preset snapshots for exact equality.
 * @param a First preset snapshot.
 * @param b Second preset snapshot.
 * @return true if the preset snapshots are identical.
 */
static bool equal_preset_data(const PresetData& a, const PresetData& b) {
    if (a.input_gain != b.input_gain || a.output_gain != b.output_gain) return false;
    if (a.effects.size() != b.effects.size()) return false;
    for (size_t i = 0; i < a.effects.size(); ++i) {
        if (!equal_effect_data(a.effects[i], b.effects[i])) return false;
    }
    return true;
}

GuiPresets::GuiPresets(AudioEngine& engine, CommandHistory& history)
    : engine_(engine), history_(history) {
    mark_clean();
}

bool GuiPresets::is_dirty() const {
    if (!saved_state_valid_) return false;
    return !equal_preset_data(saved_state_, capture_current_state(engine_));
}

std::string GuiPresets::current_preset_name() const {
    if (preset_name_buf_[0] != '\0') {
        return std::string(preset_name_buf_);
    }
    return "Untitled";
}

void GuiPresets::mark_clean() {
    saved_state_ = capture_current_state(engine_);
    saved_state_valid_ = true;
}

std::string GuiPresets::preset_name_from_path(const std::string& filepath) const {
    size_t slash = filepath.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? filepath : filepath.substr(slash + 1);
    if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
        name = name.substr(0, name.size() - 5);
    }
    return name;
}

std::string GuiPresets::preset_path_from_name(const std::string& preset_name) const {
    std::string filename = preset_name;
    for (char& c : filename) {
        if (c == ' ') c = '_';
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    if (filename.empty()) return "";
    return PresetManager::get_presets_dir() + "/" + filename + ".json";
}

void GuiPresets::refresh_presets(bool preserve_selection) {
    std::string selected_path;
    if (preserve_selection && selected_preset_index_ >= 0 &&
        selected_preset_index_ < static_cast<int>(preset_files_.size())) {
        selected_path = preset_files_[selected_preset_index_];
    }

    preset_files_ = PresetManager::list_presets();
    std::sort(preset_files_.begin(), preset_files_.end());

    selected_preset_index_ = -1;
    if (!selected_path.empty()) {
        for (int i = 0; i < static_cast<int>(preset_files_.size()); ++i) {
            if (preset_files_[i] == selected_path) {
                selected_preset_index_ = i;
                break;
            }
        }
    }
    if (selected_preset_index_ < 0 && !preset_files_.empty()) {
        selected_preset_index_ = 0;
    }

    if (selected_preset_index_ >= 0 && selected_preset_index_ < static_cast<int>(preset_files_.size())) {
        std::snprintf(preset_name_buf_, sizeof(preset_name_buf_), "%s",
                      preset_name_from_path(preset_files_[selected_preset_index_]).c_str());
    }
}

bool GuiPresets::save_named_preset(const std::string& preset_name,
                                   const std::string& description) {
    if (preset_name.empty()) {
        preset_status_msg_ = "Error: Preset name cannot be empty.";
        return false;
    }

    std::string path = preset_path_from_name(preset_name);
    if (path.empty()) {
        preset_status_msg_ = "Error: Invalid preset name.";
        return false;
    }

    if (PresetManager::save_preset(path, preset_name, description, engine_,
                                   midi_manager_ ? midi_manager_->mappings() : std::vector<MidiMapping>())) {
        preset_status_msg_ = "Saved: " + preset_name;
        refresh_presets(true);
        for (int i = 0; i < static_cast<int>(preset_files_.size()); ++i) {
            if (preset_files_[i] == path) {
                selected_preset_index_ = i;
                break;
            }
        }
        if (pedal_board_) pedal_board_->rebuild_widgets();
        mark_clean();

#ifdef __EMSCRIPTEN__
        std::string json_content = serialise_current_preset_to_json();
        EM_ASM({
            var filename = UTF8ToString($0);
            var content = UTF8ToString($1);
            var blob = new Blob([content], {type: "application/json"});
            var url = URL.createObjectURL(blob);
            var a = document.createElement("a");
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }, (preset_name + ".json").c_str(), json_content.c_str());
#endif

        return true;
    }

    preset_status_msg_ = "Error: " + PresetManager::last_error();
    return false;
}

bool GuiPresets::load_preset_by_index(int index) {
    if (index < 0 || index >= static_cast<int>(preset_files_.size())) {
        preset_status_msg_ = "Error: No preset selected.";
        return false;
    }

    const std::string& path = preset_files_[index];
    std::vector<LoadPresetCommand::EffectSnapshot> before_state;
    for (auto& fx : engine_.effects()) {
        LoadPresetCommand::EffectSnapshot snap;
        snap.effect = fx;
        snap.enabled = fx->is_enabled();
        snap.mix = fx->get_mix();
        for (auto& p : fx->params()) snap.param_values.push_back(p.value);
        before_state.push_back(std::move(snap));
    }
    float before_in = engine_.get_input_gain();
    float before_out = engine_.get_output_gain();

    if (PresetManager::load_preset(path, engine_, midi_manager_)) {
        std::vector<LoadPresetCommand::EffectSnapshot> after_state;
        for (auto& fx : engine_.effects()) {
            LoadPresetCommand::EffectSnapshot snap;
            snap.effect = fx;
            snap.enabled = fx->is_enabled();
            snap.mix = fx->get_mix();
            for (auto& p : fx->params()) snap.param_values.push_back(p.value);
            after_state.push_back(std::move(snap));
        }
        float after_in = engine_.get_input_gain();
        float after_out = engine_.get_output_gain();

        history_.clear();
        auto cmd = std::make_unique<LoadPresetCommand>(
            engine_, std::move(before_state), before_in, before_out,
            std::move(after_state), after_in, after_out);
        history_.push_executed(std::move(cmd));

        selected_preset_index_ = index;
        std::string display = preset_name_from_path(path);
        std::snprintf(preset_name_buf_, sizeof(preset_name_buf_), "%s", display.c_str());
        preset_status_msg_ = "Loaded: " + display;
        if (pedal_board_) pedal_board_->rebuild_widgets();
        mark_clean();
        return true;
    }

    preset_status_msg_ = "Error: " + PresetManager::last_error();
    return false;
}

bool GuiPresets::load_preset_by_path(const std::string& path) {
    refresh_presets(false);
    int found_idx = -1;
    for (int i = 0; i < static_cast<int>(preset_files_.size()); ++i) {
        if (preset_files_[i] == path) {
            found_idx = i;
            break;
        }
    }
    if (found_idx != -1) {
        return load_preset_by_index(found_idx);
    }
    preset_status_msg_ = "Error: Preset not found after upload.";
    return false;
}

bool GuiPresets::delete_preset_by_index(int index) {
    if (index < 0 || index >= static_cast<int>(preset_files_.size())) {
        preset_status_msg_ = "Error: No preset selected.";
        return false;
    }

    std::string path = preset_files_[index];
    std::string display = preset_name_from_path(path);
    if (std::remove(path.c_str()) == 0) {
        preset_status_msg_ = "Deleted: " + display;
        refresh_presets(false);
        return true;
    }

    preset_status_msg_ = "Error: Could not delete preset file.";
    return false;
}

void GuiPresets::ensure_factory_presets() {
    if (factory_presets_initialized_) return;
    factory_presets_initialized_ = true;

    if (!PresetManager::list_presets().empty()) return;

    std::vector<PresetData> factory_presets;

    PresetData clean;
    clean.name = "Clean";
    clean.description = "Low gain, slight reverb, flat EQ";
    clean.input_gain = 0.6f;
    clean.output_gain = 0.85f;
    clean.effects.push_back({"Compressor", true, 0.25f, {}, {}});
    clean.effects.push_back({"Equalizer", true, 1.0f, {}, {}});
    clean.effects.push_back({"Reverb", true, 0.2f, {}, {}});
    clean.effects.push_back({"Cabinet", true, 1.0f, {}, {}});
    factory_presets.push_back(clean);

    PresetData crunch;
    crunch.name = "Crunch";
    crunch.description = "Mild overdrive with mid-forward response";
    crunch.input_gain = 0.85f;
    crunch.output_gain = 0.9f;
    crunch.effects.push_back({"Noise Gate", true, 0.35f, {}, {}});
    crunch.effects.push_back({"Overdrive", true, 0.55f, {}, {}});
    crunch.effects.push_back({"Equalizer", true, 1.0f, {}, {}});
    crunch.effects.push_back({"Cabinet", true, 1.0f, {}, {}});
    factory_presets.push_back(crunch);

    PresetData metal;
    metal.name = "Metal";
    metal.description = "High distortion with scooped mids and tight cabinet";
    metal.input_gain = 1.15f;
    metal.output_gain = 0.75f;
    metal.effects.push_back({"Noise Gate", true, 0.85f, {}, {}});
    metal.effects.push_back({"Distortion", true, 0.9f, {}, {}});
    metal.effects.push_back({"Equalizer", true, 1.0f, {}, {}});
    metal.effects.push_back({"Cabinet", true, 1.0f, {}, {}});
    factory_presets.push_back(metal);

    PresetData jazz;
    jazz.name = "Jazz";
    jazz.description = "Clean, warm tone with light compression";
    jazz.input_gain = 0.55f;
    jazz.output_gain = 0.9f;
    jazz.effects.push_back({"Compressor", true, 0.4f, {}, {}});
    jazz.effects.push_back({"Equalizer", true, 1.0f, {}, {}});
    jazz.effects.push_back({"Reverb", true, 0.12f, {}, {}});
    jazz.effects.push_back({"Cabinet", true, 1.0f, {}, {}});
    factory_presets.push_back(jazz);

    for (const auto& preset : factory_presets) {
        PresetManager::save_preset_data(preset_path_from_name(preset.name), preset);
    }
}

void GuiPresets::begin_new_preset() {
    selected_preset_index_ = -1;
    preset_name_buf_[0] = '\0';
    preset_desc_buf_[0] = '\0';
    preset_dialog_is_new_ = true;
    mark_clean();
}

void GuiPresets::begin_save_preset() {
    preset_dialog_is_new_ = false;
}

void GuiPresets::render_save_popup(bool& show) {
    ImGui::SetNextWindowSize(ImVec2(420, 250), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Save Preset", &show)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Save current pedal configuration as a preset.");
    ImGui::Spacing();

    ImGui::Text("Preset Name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##preset_name", preset_name_buf_, sizeof(preset_name_buf_));

    ImGui::Spacing();
    ImGui::Text("Description (optional):");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextMultiline("##preset_desc", preset_desc_buf_, sizeof(preset_desc_buf_),
                               ImVec2(-1, 60));

    ImGui::Spacing();
    if (preset_dialog_is_new_) {
        if (ImGui::Button("Save", ImVec2(100, 30))) {
            if (save_named_preset(std::string(preset_name_buf_), std::string(preset_desc_buf_))) {
                show = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(100, 30))) {
            show = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 30))) {
            show = false;
        }
    } else {
        if (ImGui::Button("Save", ImVec2(120, 30))) {
            if (save_named_preset(std::string(preset_name_buf_), std::string(preset_desc_buf_))) {
                show = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 30))) {
            show = false;
        }
    }

    if (!preset_status_msg_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", preset_status_msg_.c_str());
    }

    ImGui::End();
}

void GuiPresets::render_load_popup(bool& show) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Load Preset", &show)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Select a preset to load:");
    ImGui::Spacing();

    if (ImGui::Button("Refresh List")) {
        refresh_presets(true);
    }

#ifdef __EMSCRIPTEN__
    ImGui::SameLine();
    if (ImGui::Button("Upload from Computer...")) {
        EM_ASM({
            var gui_ptr = $0;
            var input = document.createElement('input');
            input.type = 'file';
            input.accept = '.json';
            input.onchange = function(e) {
                var file = e.target.files[0];
                var reader = new FileReader();
                reader.onload = function(re) {
                    var content = re.target.result;
                    var path = "presets/" + file.name;
                    FS.writeFile(path, content);
                    Module.ccall('load_preset_callback', 'v', ['number', 'string'], [gui_ptr, path]);
                };
                reader.readAsText(file);
            };
            input.click();
        }, (uintptr_t)this);
        show = false;
    }
#endif

    ImGui::Spacing();
    ImGui::BeginChild("PresetList", ImVec2(0, -70), true);

    if (preset_files_.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "No presets found in '%s/' folder.\nSave a preset first, or place .json files there.",
            PresetManager::get_presets_dir().c_str());
    }

    for (int i = 0; i < static_cast<int>(preset_files_.size()); ++i) {
        auto& filepath = preset_files_[i];
        std::string display = preset_name_from_path(filepath);

        bool is_selected = (i == selected_preset_index_);
        if (ImGui::Selectable(display.c_str(), is_selected)) {
            if (load_preset_by_index(i)) {
                show = false;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();
    if (ImGui::Button("Cancel", ImVec2(120, 30))) {
        show = false;
    }

    if (!preset_status_msg_.empty()) {
        ImGui::SameLine();
        ImGui::TextWrapped("%s", preset_status_msg_.c_str());
    }

    ImGui::End();
}

std::string GuiPresets::serialise_current_preset_to_json() const {
    PresetData preset = capture_current_state(engine_);
    preset.name = current_preset_name();
    if (midi_manager_) {
        preset.midi_mappings = midi_manager_->mappings();
    }
    return to_json_ext(preset);
}

} // namespace Amplitron
