#include "gui/gui_manager.h"
#include "gui/pedalboard/pedal_board.h"
#include "gui/theme/theme.h"
#include "gui/gl_setup.h"
#include "gui/commands/command.h"
#include "gui/state/gui_graph_state.h"
#include "audio/effects/tuner.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <SDL2/SDL.h>
#include <SDL.h>
#include <algorithm>
#include <cmath>

namespace Amplitron {

// ─────────────────────────────────────────────────────────────────────────────
// Prop-assembly helpers
// ─────────────────────────────────────────────────────────────────────────────

RecordingProps GuiManager::build_recording_props() {
    auto& rec = engine_.recorder();
    const bool is_recording = rec.is_recording();

    // Fill waveform buffer (raw copy, no math)
    if (is_recording) {
        rec.get_waveform(rec_waveform_buf_, Recorder::WAVEFORM_SIZE);
    }

    RecordingProps p;
    p.is_recording    = is_recording;
    p.is_paused       = rec.is_paused();
    p.has_unsaved     = rec.has_unsaved();
    p.duration        = rec.get_duration();
    p.current_peak    = rec.get_current_peak();
    p.samples_written = rec.get_samples_written();
    p.channels        = rec.get_channels();
    p.sample_rate     = engine_.get_sample_rate();
    p.waveform_buf    = is_recording ? rec_waveform_buf_ : nullptr;
    p.waveform_size   = is_recording ? Recorder::WAVEFORM_SIZE : 0;

    p.on_resume  = [&rec]() { rec.resume(); };
    p.on_pause   = [&rec]() { rec.pause(); };
    p.on_stop    = [&rec]() { rec.stop(); };
    p.on_start   = [this, &rec]() {
        rec.start(Recorder::generate_filename(), engine_.get_sample_rate(), 2);
    };
    p.on_discard = [&rec]() { rec.discard(); };
    return p;
}

TunerProps GuiManager::build_tuner_props() {
    TunerPedal* t = tuner_pedal_.get();
    TunerProps p;
    p.has_signal = t->signal_detected.load(std::memory_order_relaxed);
    p.note_idx   = t->detected_note.load(std::memory_order_relaxed);
    p.octave     = t->detected_octave.load(std::memory_order_relaxed);
    p.cents      = t->detected_cents.load(std::memory_order_relaxed);
    p.freq       = t->detected_freq.load(std::memory_order_relaxed);
    p.mute_on    = t->params()[0].value >= 0.5f;
    p.a4_ref     = t->params()[1].value;
    p.note_name_fn = [](int idx) { return TunerPedal::note_name(idx); };
    p.on_mute_changed    = [t](bool mute) { t->params()[0].value = mute ? 1.0f : 0.0f; };
    p.on_a4_ref_changed  = [t](float ref) { t->params()[1].value = ref; };
    return p;
}

SettingsProps GuiManager::build_settings_props() {
    SettingsProps p;
    p.input_device_name  = engine_.get_input_device_name();
    p.output_device_name = engine_.get_output_device_name();
    p.device_error       = engine_.get_last_error();
    p.buffer_size        = engine_.get_buffer_size();
    p.sample_rate        = engine_.get_sample_rate();
    p.suggested_buf      = engine_.get_suggested_buffer_size();
    p.latency_ms         = (p.sample_rate > 0) ? (1000.0f * p.buffer_size / p.sample_rate) : 0.0f;
    p.cpu_load           = engine_.get_cpu_load();
    p.auto_buf           = engine_.is_auto_buffer_enabled();
    p.current_input      = engine_.get_input_device();
    p.current_output     = engine_.get_output_device();

    for (auto& dev : engine_.get_input_devices())
        p.input_devices.push_back({dev.index, dev.name, dev.is_usb_device});
    for (auto& dev : engine_.get_output_devices())
        p.output_devices.push_back({dev.index, dev.name, dev.is_usb_device});

#ifdef AMPLITRON_ANDROID_OBOE
    p.oboe_mode_label = engine_.get_oboe_sharing_mode_label();
#endif

    p.on_buffer_size_changed   = [this](int s) { engine_.set_buffer_size(s); };
    p.on_sample_rate_changed   = [this](int r) { engine_.set_sample_rate(r); };
    p.on_auto_buf_changed      = [this](bool b) { engine_.set_auto_buffer_enabled(b); };
    p.on_clear_error           = [this]() { engine_.clear_error(); };
    p.on_input_device_changed  = [this](int i) { engine_.set_input_device(i); };
    p.on_output_device_changed = [this](int i) { engine_.set_output_device(i); };
    return p;
}

AnalyzerProps GuiManager::build_analyzer_props() {
    const float dt = std::max(ImGui::GetIO().DeltaTime, 1.0f / 240.0f);

    // Drive DSP updates in the engine (no math in UI thread)
    engine_.update_level_analyzer(dt);
    engine_.update_spectrum_analyzer(dt);

    const auto& la = engine_.level_analyzer();

    AnalyzerProps p;
    p.smoothed_input_rms  = la.smoothed_input_rms();
    p.smoothed_output_rms = la.smoothed_output_rms();
    p.input_peak_hold     = la.input_peak_hold();
    p.output_peak_hold    = la.output_peak_hold();
    p.input_clip_active   = la.input_clip_flash() > 0.01f;
    p.output_clip_active  = la.output_clip_flash() > 0.01f;
    p.input_clip_flash    = la.input_clip_flash();
    p.output_clip_flash   = la.output_clip_flash();
    const auto& sa = engine_.spectrum_analyzer();
    p.spectrum.smoothed_input_db  = sa.smoothed_input_db();
    p.spectrum.smoothed_output_db = sa.smoothed_output_db();
    p.spectrum.input_peak_db      = sa.input_peak_db();
    p.spectrum.output_peak_db     = sa.output_peak_db();

    p.on_set_analyzer_enabled = [this](bool enabled) {
        engine_.set_analyzer_enabled(enabled);
    };
    return p;
}

SnapshotsProps GuiManager::build_snapshots_props() {
    SnapshotsProps p;
    for (int i = 0; i < SnapshotManager::NUM_SLOTS; ++i) {
        p.slots[i].is_filled = snapshot_manager_.has_slot(i);
        p.slots[i].is_active = (snapshot_manager_.active_slot() == i);
        p.slots[i].label     = SnapshotManager::SLOT_LABELS[i];
    }
    p.on_recall_slot = [this](int slot) {
        recallSnapshotFromSlot(slot);
    };
    p.on_save_slot = [this](int slot) {
        snapshot_manager_.save_slot(slot, engine_);
        snapshot_manager_.set_active_slot(slot);
    };
    p.on_clear_slot = [this](int slot) {
        snapshot_manager_.clear_slot(slot);
    };
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Toggle audio mute
// ─────────────────────────────────────────────────────────────────────────────
void GuiManager::toggle_audio_mute_state() {
    if (engine_.is_running()) {
        engine_.stop();
        audio_muted_ = true;
    } else {
        engine_.restart();
        audio_muted_ = false;
    }
}

void GuiManager::set_show_tuner(bool show) {
    show_tuner_ = show;
    if (show_tuner_) {
        tuner_pedal_->set_enabled(true);
        engine_.set_tuner_tap(tuner_pedal_);
    } else {
        engine_.clear_tuner_tap();
        tuner_pedal_->set_enabled(false);
    }
}

void GuiManager::recallSnapshotFromSlot(int slot) {
    if (!snapshot_manager_.has_slot(slot)) return;
    auto before          = SnapshotManager::capture(engine_);
    const auto* after    = snapshot_manager_.get_slot(slot);
    command_history_.execute(std::make_unique<RecallSnapshotCommand>(
        engine_,
        before.effects, before.input_gain, before.output_gain,
        after->effects,  after->input_gain,  after->output_gain
    ));
    snapshot_manager_.set_active_slot(slot);
    if (pedal_board_) pedal_board_->rebuild_widgets();
}

// ─────────────────────────────────────────────────────────────────────────────
// run_frame — reactive root render loop
// ─────────────────────────────────────────────────────────────────────────────
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

    // ── Keyboard shortcuts ──
    {
        ImGuiIO& io = ImGui::GetIO();
        bool mod = io.KeySuper || io.KeyCtrl;

        if (mod && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (command_history_.undo() && pedal_board_)
                pedal_board_->rebuild_widgets();
        }
        if ((mod && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) ||
            (mod && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Y))) {
            if (command_history_.redo() && pedal_board_)
                pedal_board_->rebuild_widgets();
        }
        if (!io.WantTextInput && !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_M))
            toggle_audio_mute_state();

        // Ctrl/Cmd+1–4: recall snapshot slot A–D
        static const ImGuiKey digit_keys[4] = { ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4 };
        for (int i = 0; i < 4; ++i) {
            if (mod && !io.KeyShift && ImGui::IsKeyPressed(digit_keys[i])) {
                recallSnapshotFromSlot(i);
            }
        }
    }

    // ── Menu bar ──
    render_menu_bar();

    // ── Full-window layout ──
    SDL_GetWindowSize(window_, &window_width_, &window_height_);
    ImGui::SetNextWindowPos(ImVec2(0, 20));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(window_width_),
                                    static_cast<float>(window_height_) - 20));
    ImGui::Begin("##MainArea", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    const bool is_fullscreen = GuiGraphState::get_instance().is_fullscreen;

    if (!is_fullscreen) {
        render_master_controls();
        ImGui::Separator();

        // ── GuiRecording (reactive) ──
        gui_recording_.set_props(build_recording_props());
        gui_recording_.render();

        ImGui::Separator();

        // ── GuiSnapshots (reactive) ──
        gui_snapshots_.set_props(build_snapshots_props());
        gui_snapshots_.render();

        ImGui::Separator();
    }

    float analyzer_reserved_h = is_fullscreen ? 0.0f : gui_analyzer_.analyzer_reserved_height();
    ImGui::BeginChild("PedalBoardRegion", ImVec2(0, -analyzer_reserved_h), false);
    if (pedal_board_) pedal_board_->render();
    ImGui::EndChild();

    if (!is_fullscreen) {
        ImGui::Separator();
        // ── GuiAnalyzer (reactive) ──
        gui_analyzer_.set_props(build_analyzer_props());
        gui_analyzer_.render();
    }

    ImGui::End();

    // ── Popups / floating windows ──
    if (show_settings_) {
        gui_settings_.set_props(build_settings_props());
        gui_settings_.render(show_settings_);
    }
    if (show_save_preset_)  gui_presets_.render_save_popup(show_save_preset_);
    if (show_load_preset_)  gui_presets_.render_load_popup(show_load_preset_);
    if (gui_recording_.needs_save_dialog()) {
        gui_recording_.render_save_dialog([this](const std::string& dest) {
            auto& rec = engine_.recorder();
            if (rec.save_to(dest)) {
                rec.write_metadata(dest, engine_);
            }
        });
    }
    if (show_tuner_) {
        gui_tuner_.set_props(build_tuner_props());
        bool current_show = show_tuner_;
        gui_tuner_.render(current_show);
        if (!current_show) {
            set_show_tuner(false);
        }
    }
    if (show_midi_) gui_midi_.render(show_midi_);

    // ── Toast overlay ──
    if (toast_timer_ > 0.0f) {
        toast_timer_ -= ImGui::GetIO().DeltaTime;
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 toast_pos = ImVec2(io.DisplaySize.x - 20.0f, io.DisplaySize.y - 20.0f);
        ImGui::SetNextWindowPos(toast_pos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("##toast", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
        ImGui::Text("%s", toast_message_.c_str());
        ImGui::End();
    }

    // ── Render ──
    ImGui::Render();
    int display_w, display_h;
    SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.078f, 0.071f, 0.063f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window_);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Master controls strip (smooth metering stays in GuiManager)
// ─────────────────────────────────────────────────────────────────────────────
void GuiManager::render_master_controls() {
    smoothed_input_level_  += (engine_.get_input_level()  - smoothed_input_level_)  * 0.3f;
    smoothed_output_level_ += (engine_.get_output_level() - smoothed_output_level_) * 0.3f;

    ImGui::BeginChild("MasterControls", ImVec2(0, 150), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::Columns(4, "master_cols", false);

    // Input gain
    ImGui::Text("INPUT");
    float input_gain = engine_.get_input_gain();
    if (ImGui::SliderFloat("##InputGain", &input_gain, 0.0f, 5.0f, "%.2f"))
        engine_.set_input_gain(input_gain);

    ImGui::NextColumn();

    // Input meter
    ImGui::Text("IN LEVEL");
    ImVec2 meter_pos = ImGui::GetCursorScreenPos();
    float  meter_w   = ImGui::GetColumnWidth() - 20;
    ImDrawList* dl   = ImGui::GetWindowDrawList();
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
    meter_w   = ImGui::GetColumnWidth() - 20;
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
    if (ImGui::SliderFloat("##OutputGain", &output_gain, 0.0f, 2.0f, "%.2f"))
        engine_.set_output_gain(output_gain);

    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::Columns(3, "metronome_cols", false);

    ImGui::Text("METRONOME");
    bool metronome_on = engine_.get_metronome_enabled();
    if (ImGui::Button(metronome_on ? "Stop" : "Play"))
        engine_.toggle_metronome();

    ImGui::NextColumn();

    int bpm = engine_.get_metronome_bpm();
    if (ImGui::SliderInt("BPM", &bpm, 40, 240))
        engine_.set_metronome_bpm(bpm);

    ImGui::NextColumn();

    float click = engine_.get_metronome_volume();
    if (ImGui::SliderFloat("Click", &click, 0.0f, 1.0f, "%.2f"))
        engine_.set_metronome_volume(click);

    ImGui::Columns(1);
    ImGui::EndChild();
}

} // namespace Amplitron
