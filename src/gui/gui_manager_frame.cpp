#include "gui/gui_manager.h"
#include "gui/pedal_board.h"
#include "gui/theme.h"
#include "gui/gl_setup.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <SDL2/SDL.h>

namespace Amplitron {

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
        if (!io.WantTextInput) {
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
            // Ctrl/Cmd+1–4: recall snapshot slot A–D
            static const ImGuiKey digit_keys[4] = {
                ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4
            };
            for (int i = 0; i < 4; ++i) {
                if (mod && !io.KeyShift && ImGui::IsKeyPressed(digit_keys[i])) {
                    if (gui_snapshots_.manager().has_slot(i)) {
                        gui_snapshots_.recall_slot(i);
                        if (pedal_board_) pedal_board_->rebuild_widgets();
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
    // --- NEW: Render the Profiler Window ---
    if (show_profiler_) {
        render_profiler();
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

    ImGui::BeginChild("MasterControls", ImVec2(0, 80), true);

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
    float output_gain = engine_.get_output_gain();
    if (ImGui::SliderFloat("##OutputGain", &output_gain, 0.0f, 2.0f, "%.2f")) {
        engine_.set_output_gain(output_gain);
    }

    ImGui::Columns(1);
    ImGui::EndChild();
}

// --- NEW: The GUI implementation of the DSP Profiler ---
void GuiManager::render_profiler() {
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("DSP Performance Profiler", &show_profiler_)) {
        ImGui::End();
        return;
    }

    int sr = engine_.get_sample_rate();
    int bs = engine_.get_buffer_size();
    if (sr == 0) sr = 44100; // Safety fallback
    
    float block_duration_ms = (static_cast<float>(bs) / sr) * 1000.0f;
    float block_budget_us = block_duration_ms * 1000.0f;
    float total_cpu = engine_.get_cpu_load() * 100.0f;

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Global Audio Engine Stability");
    ImGui::Separator();
    ImGui::Text("Sample Rate: %d Hz", sr);
    ImGui::Text("Buffer Size: %d samples", bs);
    ImGui::Text("Block Duration: %.2f ms (%.0f us)", block_duration_ms, block_budget_us);
    
    float total_us = engine_.get_total_callback_us();
    ImGui::Text("Total Process Time: %.1f us", total_us);

    ImGui::Spacing();
    ImGui::Text("Audio Thread CPU Load:");
    ImVec4 load_color = (total_cpu > 80.0f) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) :
                        (total_cpu > 50.0f) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                              ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, load_color);
    char total_buf[32];
    snprintf(total_buf, sizeof(total_buf), "%.1f%%", total_cpu);
    ImGui::ProgressBar(engine_.get_cpu_load(), ImVec2(-1.0f, 0.0f), total_buf);
    ImGui::PopStyleColor();

    if (total_cpu > 80.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "WARNING: High risk of buffer underruns!");
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Per-Pedal DSP Processing Time");
    ImGui::Separator();

    auto& effects = engine_.effects();
    if (effects.empty()) {
        ImGui::TextDisabled("No pedals in the current chain.");
    } else {
        for (size_t i = 0; i < effects.size(); ++i) {
            float us = engine_.get_effect_process_time_us(static_cast<int>(i));
            float fraction = (block_budget_us > 0.0f) ? (us / block_budget_us) : 0.0f;

            ImGui::Text("Pedal Slot %zu", i + 1);
            ImGui::SameLine(ImGui::GetWindowWidth() - 100);
            ImGui::Text("%.1f us", us);

            ImVec4 bar_color = (fraction > 0.4f) ? ImVec4(1.0f, 0.2f, 0.2f, 1.0f) :
                               (fraction > 0.2f) ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :
                                                   ImVec4(0.2f, 0.6f, 1.0f, 1.0f); 
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, bar_color);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f%%", fraction * 100.0f);
            ImGui::ProgressBar(fraction, ImVec2(-1.0f, 8.0f), buf); 
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }
    }
    ImGui::End();
}

} // namespace Amplitron