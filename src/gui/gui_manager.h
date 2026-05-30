#pragma once

#include "common.h"
#include "audio/engine/audio_engine.h"
#include "gui/commands/command_history.h"
#include "gui/state/snapshot_manager.h"
#include "gui/views/gui_settings.h"
#include "gui/views/gui_presets.h"
#include "gui/views/gui_recording.h"
#include "gui/views/gui_tuner.h"
#include "gui/views/gui_analyzer.h"
#include "gui/views/gui_snapshots.h"
#include "gui/views/gui_midi.h"
#include "midi/midi_manager.h"
#include <thread>
#include <mutex>
#include <string>
#include <memory>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace Amplitron {

class PedalBoard;
class TunerPedal;

/**
 * @brief Top-level GUI controller — acts as the reactive root component.
 *
 * GuiManager owns the SDL window, OpenGL context, and Dear ImGui state.
 * It drives the main render loop, assembles Props from the AudioEngine, and
 * passes them down to each child UI component.  Mutations (callbacks from
 * children) are handled here, keeping all state-change logic in one place.
 *
 * UI concerns are delegated to focused reactive sub-modules:
 * - GuiSettings:  Audio device & latency settings
 * - GuiPresets:   Preset save/load/delete
 * - GuiRecording: Recording controls & waveform display
 * - GuiTuner:     Chromatic tuner modal
 * - GuiAnalyzer:  VU meters & spectrum analyzer
 * - GuiSnapshots: A/B/C/D board-state snapshots
 * - GuiMidi:      MIDI mapping & learn window
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
    CommandHistory& command_history() { return command_history_; }

private:
    // ── Menu bar ──
    void render_menu_bar();

    // ── Master controls strip ──
    void render_master_controls();

    // ── Prop-assembly helpers ──
    RecordingProps  build_recording_props();
    TunerProps      build_tuner_props();
    SettingsProps   build_settings_props();
    AnalyzerProps   build_analyzer_props();
    SnapshotsProps  build_snapshots_props();

    // ── Actions (called from child callbacks / keyboard shortcuts) ──
    void toggle_audio_mute_state();
    void set_show_tuner(bool show);
    void recallSnapshotFromSlot(int slot);

    // ─────────────────────────────────────────────────────────────────────
    // Core objects
    // ─────────────────────────────────────────────────────────────────────
    AudioEngine&   engine_;
    CommandHistory command_history_;

    SDL_Window*    window_     = nullptr;
    SDL_GLContext  gl_context_ = nullptr;

    std::unique_ptr<PedalBoard> pedal_board_;

    // Tuner pedal instance shared between engine tap and TunerProps assembly
    std::shared_ptr<TunerPedal> tuner_pedal_;

    bool initialized_      = false;
    int  window_width_     = 1280;
    int  window_height_    = 720;
    bool audio_muted_      = false;

    // ── Smoothed master level meters (computed in GuiManager, not in children) ──
    float smoothed_input_level_  = 0.0f;
    float smoothed_output_level_ = 0.0f;

    // ── Visibility flags (owned here, passed to child render calls) ──
    bool show_settings_      = false;
    bool show_save_preset_   = false;
    bool show_load_preset_   = false;
    bool show_tuner_         = false;
    bool show_midi_          = false;

    // ── Snapshot manager (state lives in GuiManager; GuiSnapshots is a pure view) ──
    SnapshotManager snapshot_manager_;

    // ── Reactive child components ──
    GuiSettings   gui_settings_;
    GuiPresets    gui_presets_;
    GuiRecording  gui_recording_;
    GuiTuner      gui_tuner_;
    GuiAnalyzer   gui_analyzer_;
    GuiSnapshots  gui_snapshots_;
    MidiManager   midi_manager_;
    GuiMidi       gui_midi_;

    // ── Toast notification ──
    std::string toast_message_;
    float       toast_timer_ = 0.0f;

    // ── Update checking ──
    void check_for_updates();
    std::thread update_check_thread_;
    std::mutex  update_mutex_;
    bool        has_new_release_     = false;
    std::string new_release_version_;
    std::string new_release_url_;

    // ── Waveform buffer (filled from recorder, passed to RecordingProps) ──
    float rec_waveform_buf_[512] = {};
};

} // namespace Amplitron
