#pragma once

#include "common.h"
#include "audio/audio_engine.h"
#include "gui/command_history.h"
#include "gui/gui_settings.h"
#include "gui/gui_presets.h"
#include "gui/gui_recording.h"
#include "gui/gui_tuner.h"
#include "gui/gui_analyzer.h"
#include "gui/gui_snapshots.h"
#include "gui/gui_midi.h"
#include "midi/midi_manager.h"
#include <thread>
#include <mutex>
#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace Amplitron {

class PedalBoard;

/**
 * @brief Top-level GUI controller.
 *
 * Owns the SDL window, OpenGL context, Dear ImGui state, and the
 * PedalBoard / CommandHistory instances. Drives the main render loop
 * and dispatches keyboard shortcuts.
 *
 * UI concerns are delegated to focused sub-modules:
 * - GuiSettings:  Audio device & latency settings
 * - GuiPresets:   Preset save/load/delete
 * - GuiRecording: Recording controls & waveform display
 * - GuiTuner:     Chromatic tuner modal
 * - GuiAnalyzer:  VU meters & spectrum analyzer
 */
class GuiManager {
public:
    GuiManager(AudioEngine& engine);
    ~GuiManager();

    bool initialize(int width = 1280, int height = 720);
    void shutdown();
    bool run_frame();

    MidiManager& midi_manager() { return midi_manager_; }
    AudioEngine& audio_engine() { return engine_; }

private:
    void render_menu_bar();
    void render_master_controls();
    void toggle_audio_mute_state();
    
    AudioEngine& engine_;
    CommandHistory command_history_;
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    std::unique_ptr<PedalBoard> pedal_board_;

    bool initialized_ = false;
    bool show_settings_ = false;
    bool show_save_preset_ = false;
    bool show_load_preset_ = false;
    bool show_tuner_ = false;
    int window_width_ = 1280;
    int window_height_ = 720;
    bool audio_muted_ = false;

    // Smoothed meter values for master controls
    float smoothed_input_level_ = 0.0f;
    float smoothed_output_level_ = 0.0f;

    // Extracted UI modules
    GuiSettings gui_settings_;
    GuiPresets gui_presets_;
    GuiRecording gui_recording_;
    GuiTuner gui_tuner_;
    GuiAnalyzer gui_analyzer_;
    GuiSnapshots gui_snapshots_;
    MidiManager midi_manager_;
    GuiMidi gui_midi_;
    bool show_midi_ = false;

    // Toast notification state
    std::string toast_message_;
    float toast_timer_ = 0.0f;

    // Update checking
    void check_for_updates();
    std::thread update_check_thread_;
    std::mutex update_mutex_;
    bool has_new_release_ = false;
    std::string new_release_version_;
    std::string new_release_url_;
};

} // namespace Amplitron
