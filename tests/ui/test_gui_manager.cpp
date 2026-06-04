/**
 * @file test_gui_manager.cpp
 * @brief Tests for GuiManager lifecycle and subsystem association.
 *
 * Headless-safe: constructor and accessor paths only — initialize() and
 * run_frame() require SDL+OpenGL and are not exercised here.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "amplitron_session.h"
#define private public
#include "gui/gui_manager.h"
#undef private

using namespace Amplitron;

TEST(amplitron_session_throws_on_null_arguments) {
    ASSERT_THROW(AmplitronSession(nullptr, std::make_unique<MidiManager>(), std::make_unique<PresetManagerService>()), std::invalid_argument);
    ASSERT_THROW(AmplitronSession(std::make_unique<AudioEngine>(), nullptr, std::make_unique<PresetManagerService>()), std::invalid_argument);
    ASSERT_THROW(AmplitronSession(std::make_unique<AudioEngine>(), std::make_unique<MidiManager>(), nullptr), std::invalid_argument);
}

TEST(gui_manager_basic_lifecycle) {
    AmplitronSession session;
    auto& engine = session.concrete_engine();
    engine.initialize();

    // Construct GuiManager — safe without SDL/GL (constructor defers window setup)
    GuiManager gui(session);

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
    engine.set_running_for_testing(true); // Headless-safe: bypass physical soundcard start requirement
    gui.toggle_audio_mute_state();
    ASSERT_TRUE(gui.audio_muted_);
    
    engine.set_running_for_testing(false); // Headless-safe: manually update engine state since Pa_Stream is nullptr
    gui.toggle_audio_mute_state();
    ASSERT_FALSE(gui.audio_muted_);

    // 2. Render master controls
    gui.render_master_controls();

    // 3. Render menu bar (without update)
    gui.render_menu_bar();

    // 4. UpdateChecker tests are handled elsewhere, render menu normally
    gui.render_menu_bar();

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
        auto p1 = gui.build_recording_props();
        ASSERT_FALSE(p1.is_recording);
        ASSERT_FALSE(p1.is_paused);
        ASSERT_FALSE(p1.has_unsaved);

        // Simulate start
        p1.on_start();
        auto p2 = gui.build_recording_props();
        ASSERT_TRUE(p2.is_recording);
        
        // Pause and stop
        p2.on_pause();
        auto p3 = gui.build_recording_props();
        ASSERT_TRUE(p3.is_paused);

        p3.on_resume();
        p3.on_stop();
        p3.on_discard();
    }

    // 2. build_tuner_props
    {
        auto p = gui.build_tuner_props();
        ASSERT_FALSE(p.has_signal);
        p.on_mute_changed(true);
        p.on_a4_ref_changed(442.0f);
        
        auto p2 = gui.build_tuner_props();
        ASSERT_TRUE(p2.mute_on);
        ASSERT_NEAR(p2.a4_ref, 442.0f, 0.01f);
    }

    // 3. build_settings_props
    {
        auto p = gui.build_settings_props();
        ASSERT_EQ(p.buffer_size, engine.get_buffer_size());
        p.on_buffer_size_changed(256);
        p.on_sample_rate_changed(48000);
        p.on_auto_buf_changed(true);
        p.on_clear_error();
        p.on_input_device_changed(0);
        p.on_output_device_changed(0);

        auto p2 = gui.build_settings_props();
        ASSERT_EQ(p2.buffer_size, 256);
        ASSERT_EQ(p2.sample_rate, 48000);
        ASSERT_TRUE(p2.auto_buf);
    }

    // 4. build_analyzer_props
    {
        auto p = gui.build_analyzer_props();
        ASSERT_TRUE(p.spectrum.smoothed_input_db == gui.metrics_service_.spectrum_analyzer().smoothed_input_db());
        p.on_set_analyzer_enabled(true);
    }

    // 5. build_snapshots_props
    {
        auto p = gui.build_snapshots_props();
        ASSERT_FALSE(p.slots[0].is_filled);
        
        p.on_save_slot(0);
        auto p2 = gui.build_snapshots_props();
        ASSERT_TRUE(p2.slots[0].is_filled);
        ASSERT_TRUE(p2.slots[0].is_active);

        p2.on_recall_slot(0);
        p2.on_clear_slot(0);
        
        auto p3 = gui.build_snapshots_props();
        ASSERT_FALSE(p3.slots[0].is_filled);
    }

    gui.shutdown();
    engine.shutdown();
}