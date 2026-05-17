#include "gui/gui_manager.h"
#include "gui/pedal_board.h"
#include "gui/file_dialog.h"
#include "gui/theme.h"
#include "preset_manager.h"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <cstdio>
#include <string>
#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__) && !TARGET_OS_IOS
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

namespace Amplitron {

// Safe URL opener that avoids shell injection
static void open_url_safe(const std::string& url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__) && !TARGET_OS_IOS
    // Use fork+exec to invoke open without shell
    int pipefd[2];
    if (pipe(pipefd) != 0) return;
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    if (pid == 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execl("/usr/bin/open", "open", url.c_str(), nullptr);
        _exit(1);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    int status = 0;
    waitpid(pid, &status, 0);
#elif defined(__linux__)
    // Use fork+exec to invoke xdg-open without shell
    int pipefd[2];
    if (pipe(pipefd) != 0) return;
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    if (pid == 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execl("/usr/bin/xdg-open", "xdg-open", url.c_str(), nullptr);
        _exit(1);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    int status = 0;
    waitpid(pid, &status, 0);
#endif
}

void GuiManager::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Preset...")) {
                gui_presets_.begin_new_preset();
                show_save_preset_ = true;
            }
            if (ImGui::MenuItem("Save Preset...", "Ctrl+S")) {
                gui_presets_.begin_save_preset();
                show_save_preset_ = true;
            }
            if (ImGui::MenuItem("Load Preset...", "Ctrl+O")) {
                show_load_preset_ = true;
                gui_presets_.ensure_factory_presets();
                gui_presets_.refresh_presets(true);
            }
            bool has_selected_preset = gui_presets_.selected_preset_index() >= 0 &&
                                       gui_presets_.selected_preset_index() < gui_presets_.preset_count();
            if (ImGui::MenuItem("Delete Selected Preset", nullptr, false, has_selected_preset)) {
                gui_presets_.delete_preset_by_index(gui_presets_.selected_preset_index());
            }
            ImGui::Separator();
#ifndef AMPLITRON_NO_DESKTOP_SHELL
            if (ImGui::MenuItem("Change Presets Directory...")) {
                std::string chosen = show_folder_dialog("Select Presets Directory");
                if (!chosen.empty()) {
                    PresetManager::set_presets_dir(chosen);
                    PresetManager::save_config();
                    gui_presets_.refresh_presets(false);
                }
            }
            if (ImGui::MenuItem("Reset to Default Presets Directory")) {
                PresetManager::set_presets_dir("");
                PresetManager::save_config();
                gui_presets_.refresh_presets(false);
            }
#endif
            ImGui::Separator();
            if (ImGui::MenuItem("Settings")) show_settings_ = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                SDL_Event quit_event;
                quit_event.type = SDL_QUIT;
                SDL_PushEvent(&quit_event);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            bool can_undo = command_history_.can_undo();
            bool can_redo = command_history_.can_redo();

            const char* undo_label = command_history_.undo_description();
            char undo_buf[64] = "Undo";
            if (undo_label) snprintf(undo_buf, sizeof(undo_buf), "Undo %s", undo_label);

            const char* redo_label = command_history_.redo_description();
            char redo_buf[64] = "Redo";
            if (redo_label) snprintf(redo_buf, sizeof(redo_buf), "Redo %s", redo_label);

            if (ImGui::MenuItem(undo_buf, "Ctrl+Z", false, can_undo)) {
                if (command_history_.undo() && pedal_board_) {
                    pedal_board_->rebuild_widgets();
                }
            }
            if (ImGui::MenuItem(redo_buf, "Ctrl+Shift+Z", false, can_redo)) {
                if (command_history_.redo() && pedal_board_) {
                    pedal_board_->rebuild_widgets();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Audio")) {
            if (engine_.is_running()) {
                if (ImGui::MenuItem("Stop Audio")) engine_.stop();
            } else {
                if (ImGui::MenuItem("Start Audio")) {
                    engine_.restart();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Restart Audio")) {
                engine_.restart();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Utilities")) {
            if (ImGui::MenuItem("Open Tuner", nullptr, show_tuner_)) {
                gui_tuner_.toggle(show_tuner_);
            }
            if (ImGui::MenuItem("MIDI Settings", nullptr, show_midi_)) {
                show_midi_ = !show_midi_;
            }
            ImGui::EndMenu();
        }

        // Status bar (right-aligned items computed dynamically)
        float bar_w = ImGui::GetWindowWidth();
        float padding = 8.0f;

        // Build a vector of status items from right to left (for right-alignment)
        struct StatusItem {
            std::string label;
            bool is_clickable = false;
        };
        std::vector<StatusItem> items;

        // Preset status with dirty indicator
        std::string preset_label = "Preset: " + gui_presets_.current_preset_name();
        if (gui_presets_.is_dirty()) {
            preset_label += " *";
        }
        items.push_back({preset_label, false});

        // Sample rate (rightmost)
        char sr_buf[16];
        snprintf(sr_buf, sizeof(sr_buf), "%dHz", engine_.get_sample_rate());
        items.push_back({sr_buf, false});

        // Audio status (LIVE/STOPPED)
        items.push_back({engine_.is_running() ? "LIVE" : "STOPPED", false});

        // Recording indicator if recording
        if (engine_.recorder().is_recording()) {
            char rec_dur[32];
            snprintf(rec_dur, sizeof(rec_dur), "%.1fs", engine_.recorder().get_duration());
            items.push_back({rec_dur, false});
            items.push_back({"REC", false});
        }

        // MIDI status
        items.push_back({midi_manager_.is_port_open() ? "MIDI" : "MIDI", false});

        // Check for update (leftmost of right-aligned group)
        bool show_update = false;
        std::string update_version;
        std::string update_url;
        {
            std::lock_guard<std::mutex> lock(update_mutex_);
            if (has_new_release_) {
                show_update = true;
                update_version = new_release_version_;
                update_url = new_release_url_;
            }
        }

        if (show_update) {
            std::string release_text = "New Release Available: " + update_version;
            items.push_back({release_text, true});  // Clickable
        }

        // Measure widths and compute right-aligned positions
        float x_pos = bar_w - padding;
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            ImVec2 text_size = ImGui::CalcTextSize(it->label.c_str());
            x_pos -= text_size.x + padding;
        }

        // Render items from left to right
        x_pos = bar_w - padding;
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            ImVec2 text_size = ImGui::CalcTextSize(it->label.c_str());
            x_pos -= text_size.x + padding;

            ImGui::SameLine(x_pos);

            if (it->label.find("MIDI") == 0) {
                if (midi_manager_.is_port_open()) {
                    ImGui::TextColored(Theme::Live(), "%s", it->label.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "%s", it->label.c_str());
                }
            } else if (it->label == "LIVE") {
                ImGui::TextColored(Theme::Live(), "%s", it->label.c_str());
            } else if (it->label == "STOPPED") {
                ImGui::TextColored(Theme::Stopped(), "%s", it->label.c_str());
            } else if (it->label == "REC") {
                float t = static_cast<float>(ImGui::GetTime());
                ImGui::TextColored(Theme::RecBlink(t), "%s", it->label.c_str());
            } else if (it->label.find("New Release") == 0) {
                ImGui::TextColored(Theme::GoldHot(), "%s", it->label.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Click to open release in browser");
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
                if (ImGui::IsItemClicked()) {
                    open_url_safe(update_url);
                }
            } else {
                ImGui::Text("%s", it->label.c_str());
            }
        }

        ImGui::EndMainMenuBar();
    }

    // Error banner when audio is stopped
    if (!engine_.is_running()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.35f, 0.08f, 0.08f, 0.95f));
        ImGui::BeginChild("AudioErrorBanner", ImVec2(0, 36), true);
        ImGui::TextColored(Theme::Stopped(), "Audio stream is STOPPED.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Restart Audio")) {
            engine_.restart();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Settings")) {
            show_settings_ = true;
        }
        std::string err = engine_.get_last_error();
        if (!err.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(Theme::GoldHot(), "  %s", err.c_str());
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

} // namespace Amplitron
