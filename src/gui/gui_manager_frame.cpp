#include "gui/gui_manager.h"
#include "gui/pedal_board.h"
#include "gui/theme.h"
#include "gui/gl_setup.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <SDL2/SDL.h>

namespace Amplitron {

void GuiManager::toggle_audio_mute_state() {

    if (engine_.is_running()) {
        engine_.stop();
        audio_muted_ = true;
    } else {
        engine_.restart();
        audio_muted_ = false;
    }
}
bool GuiManager::run_frame() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) return false;
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(window_))
            return false;
    }

    midi_manager_.poll(engine_);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Keyboard shortcuts for undo/redo and snapshot save
    {
        ImGuiIO& io = ImGui::GetIO();

    bool mod = io.KeySuper || io.KeyCtrl;

    if (mod && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (command_history_.undo() && pedal_board_) {
            pedal_board_->rebuild_widgets();
        }
    }

    

    if (mod && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (command_history_.redo() && pedal_board_) {
            pedal_board_->rebuild_widgets();
        }
    }

    if (mod && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (command_history_.redo() && pedal_board_) {
            pedal_board_->rebuild_widgets();
        }
    }

    if (!ImGui::GetIO().WantTextInput &&!ImGui::IsAnyItemActive() &&ImGui::IsKeyPressed(ImGuiKey_M)) {
        toggle_audio_mute_state();
    }

    // Ctrl/Cmd+1–4: recall snapshot slot A–D
    static const ImGuiKey digit_keys[4] = {
        ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4
    };

    for (int i = 0; i < 4; ++i) {
        if (mod && !io.KeyShift &&
            ImGui::IsKeyPressed(digit_keys[i])) {

            if (gui_snapshots_.manager().has_slot(i)) {
                gui_snapshots_.recall_slot(i);

                if (pedal_board_) {
                    pedal_board_->rebuild_widgets();
                }
            }
        }
    }
}

    // Main menu bar
    render_menu_bar();

    // Full-window layout
    SDL_GetWindowSize(window_, &window_width_, &window_height_);

    ImGui::SetNextWindowPos(ImVec2(0, 20));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(window_width_),
                                    static_cast<float>(window_height_) - 20));
    ImGui::Begin("##MainArea", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    render_master_controls();

    ImGui::Separator();

    // Recording controls (above pedal board)
    gui_recording_.render_controls();

    ImGui::Separator();

    // In-session snapshots (A/B/C/D slot row)
    gui_snapshots_.render();

    ImGui::Separator();

    float analyzer_reserved_h = gui_analyzer_.analyzer_reserved_height();
    ImGui::BeginChild("PedalBoardRegion", ImVec2(0, -analyzer_reserved_h), false);
    if (pedal_board_) {
        pedal_board_->render();
    }
    ImGui::EndChild();

    ImGui::Separator();
    gui_analyzer_.render();

    ImGui::End();

    // Popups / floating windows
    if (show_settings_) {
        gui_settings_.render(show_settings_);
    }
    if (show_save_preset_) {
        gui_presets_.render_save_popup(show_save_preset_);
    }
    if (show_load_preset_) {
        gui_presets_.render_load_popup(show_load_preset_);
    }
    if (gui_recording_.show_save()) {
        gui_recording_.render_save_dialog(gui_recording_.show_save());
    }
    if (show_tuner_) {
        gui_tuner_.render(show_tuner_);
    }
    if (show_midi_) {
        gui_midi_.render(show_midi_);
    }

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.078f, 0.071f, 0.063f, 1.0f);  // #141210 BG_DARKEST
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);

    return true;
}

void GuiManager::render_master_controls() {
    // Smooth metering
    float input_lvl = engine_.get_input_level();
    float output_lvl = engine_.get_output_level();
    smoothed_input_level_ += (input_lvl - smoothed_input_level_) * 0.3f;
    smoothed_output_level_ += (output_lvl - smoothed_output_level_) * 0.3f;

    ImGui::BeginChild("MasterControls", ImVec2(0, 150), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::Columns(4, "master_cols", false);

    // Input gain
    ImGui::Text("INPUT");
    float input_gain = engine_.get_input_gain();
    if (ImGui::SliderFloat("##InputGain", &input_gain, 0.0f, 5.0f, "%.2f")) {
        engine_.set_input_gain(input_gain);
    }

    ImGui::NextColumn();

    // Input meter
    ImGui::Text("IN LEVEL");
    ImVec2 meter_pos = ImGui::GetCursorScreenPos();
    float meter_w = ImGui::GetColumnWidth() - 20;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(meter_pos, ImVec2(meter_pos.x + meter_w, meter_pos.y + 20),
                      Theme::METER_BG, Theme::ROUNDING_SM);
    float fill = std::min(smoothed_input_level_, 1.0f) * meter_w;
    ImU32 meter_color = (smoothed_input_level_ > 0.9f) ? Theme::METER_RED :
                        (smoothed_input_level_ > 0.6f) ? Theme::METER_YELLOW :
                                                          Theme::METER_GREEN;
    dl->AddRectFilled(meter_pos, ImVec2(meter_pos.x + fill, meter_pos.y + 20),
                      meter_color, Theme::ROUNDING_SM);
    ImGui::Dummy(ImVec2(meter_w, 20));

    ImGui::NextColumn();

    // Output meter
    ImGui::Text("OUT LEVEL");
    meter_pos = ImGui::GetCursorScreenPos();
    meter_w = ImGui::GetColumnWidth() - 20;
    dl->AddRectFilled(meter_pos, ImVec2(meter_pos.x + meter_w, meter_pos.y + 20),
                      Theme::METER_BG, Theme::ROUNDING_SM);
    fill = std::min(smoothed_output_level_, 1.0f) * meter_w;
    meter_color = (smoothed_output_level_ > 0.9f) ? Theme::METER_RED :
                  (smoothed_output_level_ > 0.6f) ? Theme::METER_YELLOW :
                                                     Theme::METER_GREEN;
    dl->AddRectFilled(meter_pos, ImVec2(meter_pos.x + fill, meter_pos.y + 20),
                      meter_color, Theme::ROUNDING_SM);
    ImGui::Dummy(ImVec2(meter_w, 20));

    ImGui::NextColumn();

    // Output gain
    ImGui::Text("OUTPUT");
    if (audio_muted_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "MUTED");
    }
    float output_gain = engine_.get_output_gain();
    if (ImGui::SliderFloat("##OutputGain", &output_gain, 0.0f, 2.0f, "%.2f")) {
        engine_.set_output_gain(output_gain);
    }

    ImGui::Columns(1);

    ImGui::Separator();

    ImGui::Columns(3, "metronome_cols", false);

    ImGui::Text("METRONOME");
    bool metronome_on = engine_.get_metronome_enabled();
    if (ImGui::Button(metronome_on ? "Stop" : "Play")) {
        engine_.toggle_metronome();
    }

    ImGui::NextColumn();

    int bpm = engine_.get_metronome_bpm();
    if (ImGui::SliderInt("BPM", &bpm, 40, 240)) {
        engine_.set_metronome_bpm(bpm);
    }

    ImGui::NextColumn();

    float click = engine_.get_metronome_volume();
    if (ImGui::SliderFloat("Click", &click, 0.0f, 1.0f, "%.2f")) {
        engine_.set_metronome_volume(click);
    }

    ImGui::Columns(1);
    ImGui::EndChild();
}

} // namespace Amplitron
