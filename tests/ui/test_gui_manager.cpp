#include <fcntl.h>
#include <sys/types.h>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

// Pre-include standard library headers to protect them from macro poisoning
#include <imgui.h>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <codecvt>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <ostream>
#include <queue>
#include <random>
#include <ratio>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <vector>

#define private public
#define protected public
#include "amplitron_session.h"
#include "audio/recorder/recorder.h"
#include "gui/gui_manager.h"
#include "midi/midi_manager.h"
#include "test_fixtures.h"
#include "test_framework.h"

namespace Amplitron {
struct GuiManagerTestAccessor {
    static bool& get_audio_muted(GuiManager& g) { return g.audio_muted_; }
    static void toggle_audio_mute_state(GuiManager& g) { g.toggle_audio_mute_state(); }
    static void render_master_controls(GuiManager& g) { g.render_master_controls(); }
    static void render_menu_bar(GuiManager& g) { g.render_menu_bar(); }
    static RecordingProps build_recording_props(GuiManager& g) { return g.build_recording_props(); }
    static TunerProps build_tuner_props(GuiManager& g) { return g.build_tuner_props(); }
    static SettingsProps build_settings_props(GuiManager& g) { return g.build_settings_props(); }
    static AnalyzerProps build_analyzer_props(GuiManager& g) { return g.build_analyzer_props(); }
    static SnapshotsProps build_snapshots_props(GuiManager& g) { return g.build_snapshots_props(); }
    static AudioMetricsService& get_metrics_service(GuiManager& g) { return g.metrics_service_; }
};
}  // namespace Amplitron

using namespace Amplitron;

TEST(amplitron_session_throws_on_null_arguments) {
    ASSERT_THROW(AmplitronSession(nullptr, std::make_unique<MidiManager>(),
                                  std::make_unique<PresetManagerService>()),
                 std::invalid_argument);
    ASSERT_THROW(AmplitronSession(std::make_unique<AudioEngine>(), nullptr,
                                  std::make_unique<PresetManagerService>()),
                 std::invalid_argument);
    ASSERT_THROW(
        AmplitronSession(std::make_unique<AudioEngine>(), std::make_unique<MidiManager>(), nullptr),
        std::invalid_argument);
}

TEST(gui_manager_basic_lifecycle) {
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    // Construct GuiManager
    GuiManager gui(session);
    gui.initialize();

    // Audio engine reference is correctly stored
    ASSERT_EQ(&gui.audio_engine(), &engine);

    // MIDI manager is reachable through GuiManager
    auto& mm = gui.midi_manager();
    (void)mm;

    // Explicit shutdown before engine teardown
    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_double_shutdown_is_safe) {
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);
    gui.initialize();

    // shutdown() must guard against being called twice (initialized_ flag)
    gui.shutdown();
    gui.shutdown();  // Must not crash

    engine.shutdown();
}

TEST(gui_manager_midi_manager_association) {
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);

    // midi_manager() must return a stable reference (same address each call)
    ASSERT_EQ(&gui.midi_manager(), &gui.midi_manager());

    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_private_rendering_methods) {
    ScopedImGuiContext imgui;
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);

    // 1. Mute/unmute
    engine.set_running_for_testing(
        true);  // Headless-safe: bypass physical soundcard start requirement
    GuiManagerTestAccessor::toggle_audio_mute_state(gui);
    ASSERT_TRUE(GuiManagerTestAccessor::get_audio_muted(gui));

    engine.set_running_for_testing(
        false);  // Headless-safe: manually update engine state since Pa_Stream is nullptr
    GuiManagerTestAccessor::toggle_audio_mute_state(gui);
    ASSERT_FALSE(GuiManagerTestAccessor::get_audio_muted(gui));

    // 2. Render master controls
    GuiManagerTestAccessor::render_master_controls(gui);

    // 3. Render menu bar (without update)
    GuiManagerTestAccessor::render_menu_bar(gui);

    // 4. UpdateChecker tests are handled elsewhere, render menu normally
    GuiManagerTestAccessor::render_menu_bar(gui);

    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_logical_builders) {
    ScopedImGuiContext imgui;
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);

    // 1. build_recording_props under various Recorder states
    {
        auto p1 = GuiManagerTestAccessor::build_recording_props(gui);
        ASSERT_FALSE(p1.is_recording);
        ASSERT_FALSE(p1.is_paused);
        ASSERT_FALSE(p1.has_unsaved);

        // Simulate start
        p1.on_start();
        auto p2 = GuiManagerTestAccessor::build_recording_props(gui);
        ASSERT_TRUE(p2.is_recording);

        // Pause and stop
        p2.on_pause();
        auto p3 = GuiManagerTestAccessor::build_recording_props(gui);
        ASSERT_TRUE(p3.is_paused);

        p3.on_resume();
        p3.on_stop();
        p3.on_discard();
    }

    // 2. build_tuner_props
    {
        auto p = GuiManagerTestAccessor::build_tuner_props(gui);
        ASSERT_FALSE(p.has_signal);
        p.on_mute_changed(true);
        p.on_a4_ref_changed(442.0f);

        auto p2 = GuiManagerTestAccessor::build_tuner_props(gui);
        ASSERT_TRUE(p2.mute_on);
        ASSERT_NEAR(p2.a4_ref, 442.0f, 0.01f);
    }

    // 3. build_settings_props
    {
        auto p = GuiManagerTestAccessor::build_settings_props(gui);
        ASSERT_EQ(p.buffer_size, engine.get_buffer_size());
        p.on_buffer_size_changed(256);
        p.on_sample_rate_changed(48000);
        p.on_auto_buf_changed(true);
        p.on_clear_error();
        p.on_input_device_changed(0);
        p.on_output_device_changed(0);

        auto p2 = GuiManagerTestAccessor::build_settings_props(gui);
        ASSERT_EQ(p2.buffer_size, 256);
        ASSERT_EQ(p2.sample_rate, 48000);
        ASSERT_TRUE(p2.auto_buf);
    }

    // 4. build_analyzer_props
    {
        auto p = GuiManagerTestAccessor::build_analyzer_props(gui);
        ASSERT_TRUE(p.spectrum.smoothed_input_db == GuiManagerTestAccessor::get_metrics_service(gui)
                                                        .spectrum_analyzer()
                                                        .smoothed_input_db());
        p.on_set_analyzer_enabled(true);
    }

    // 5. build_snapshots_props
    {
        auto p = GuiManagerTestAccessor::build_snapshots_props(gui);
        ASSERT_FALSE(p.slots[0].is_filled);

        p.on_save_slot(0);
        auto p2 = GuiManagerTestAccessor::build_snapshots_props(gui);
        ASSERT_TRUE(p2.slots[0].is_filled);
        ASSERT_TRUE(p2.slots[0].is_active);

        p2.on_recall_slot(0);
        p2.on_clear_slot(0);

        auto p3 = GuiManagerTestAccessor::build_snapshots_props(gui);
        ASSERT_FALSE(p3.slots[0].is_filled);
    }

    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_bpm_auto_detect_state_machine) {
    ScopedImGuiContext imgui;
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    GuiManager gui(session);

    // 1. Countdown state transition
    gui.bpm_detect_state_ = GuiManager::BpmDetectState::Countdown;
    gui.bpm_detect_timer_ = 0.05f;
    // Set DeltaTime to 0.1f to force expiration
    ImGui::GetIO().DeltaTime = 0.1f;
    gui.render_master_controls();
    ASSERT_TRUE(gui.bpm_detect_state_ == GuiManager::BpmDetectState::Recording);
    ASSERT_NEAR(gui.bpm_detect_timer_, 10.0f, 0.01f);

    // 2. Recording state transition (failure)
    gui.bpm_detect_state_ = GuiManager::BpmDetectState::Recording;
    gui.bpm_detect_timer_ = 0.05f;
    gui.render_master_controls();
    ASSERT_TRUE(gui.bpm_detect_state_ == GuiManager::BpmDetectState::Idle);
    ASSERT_EQ(gui.toast_message_, "BPM Detection Failed");
    ASSERT_NEAR(gui.toast_timer_, 3.0f, 0.01f);

    // 3. Recording state transition (success)
    // Write 4.5 seconds of 120 BPM signal into engine
    int sr = engine.get_sample_rate();
    float bpm = 120.0f;
    int total_samples = static_cast<int>(sr * 4.5f);
    std::vector<float> signal(total_samples, 0.0f);
    float beat_interval_seconds = 60.0f / bpm;
    int beat_interval_samples = static_cast<int>(sr * beat_interval_seconds);
    for (int sample_idx = 0; sample_idx < total_samples; sample_idx += beat_interval_samples) {
        int burst_len = static_cast<int>(sr * 0.015f);
        for (int i = 0; i < burst_len && (sample_idx + i) < total_samples; ++i) {
            float t = static_cast<float>(i) / sr;
            float env = std::exp(-200.0f * t);
            signal[sample_idx + i] = env * std::sin(2.0f * 3.14159f * 1000.0f * t);
        }
    }

    // Feed it to the tempo engine via AudioEngine's process in small chunks to prevent buffer
    // overflow
    const int chunk_size = 256;
    std::vector<float> out_buffer(chunk_size * 2, 0.0f);
    for (int i = 0; i < total_samples; i += chunk_size) {
        int to_process = std::min(total_samples - i, chunk_size);
        engine.process_audio(signal.data() + i, out_buffer.data(), to_process);
    }

    gui.bpm_detect_state_ = GuiManager::BpmDetectState::Recording;
    gui.bpm_detect_timer_ = 0.05f;
    gui.render_master_controls();
    ASSERT_TRUE(gui.bpm_detect_state_ == GuiManager::BpmDetectState::Idle);
    // Since detect_bpm should detect ~120 BPM, toast_message_ should contain "BPM Detected"
    ASSERT_TRUE(gui.toast_message_.find("BPM Detected:") != std::string::npos);

    gui.shutdown();
    engine.shutdown();
}
