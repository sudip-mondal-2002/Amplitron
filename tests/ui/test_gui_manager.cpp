/**
 * @file test_gui_manager.cpp
 * @brief Tests for GuiManager lifecycle and subsystem association.
 *
 * Headless-safe: constructor and accessor paths only — initialize() and
 * run_frame() require SDL+OpenGL and are not exercised here.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#define private public
#include "gui/gui_manager.h"
#undef private

using namespace Amplitron;

TEST(gui_manager_basic_lifecycle) {
    AudioEngine engine;
    engine.initialize();

    // Construct GuiManager — safe without SDL/GL (constructor defers window setup)
    GuiManager gui(engine);

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
    AudioEngine engine;
    engine.initialize();

    GuiManager gui(engine);

    // shutdown() must guard against being called twice (initialized_ flag)
    gui.shutdown();
    gui.shutdown();  // Must not crash

    engine.shutdown();
}

TEST(gui_manager_midi_manager_association) {
    AudioEngine engine;
    engine.initialize();

    GuiManager gui(engine);

    // midi_manager() must return a stable reference (same address each call)
    ASSERT_EQ(&gui.midi_manager(), &gui.midi_manager());

    gui.shutdown();
    engine.shutdown();
}

TEST(gui_manager_private_rendering_methods) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    GuiManager gui(engine);

    // 1. Mute/unmute
    engine.running_ = true; // Headless-safe: bypass physical soundcard start requirement
    gui.toggle_audio_mute_state();
    ASSERT_TRUE(gui.audio_muted_);
    
    engine.running_ = false; // Headless-safe: manually update engine state since Pa_Stream is nullptr
    gui.toggle_audio_mute_state();
    ASSERT_FALSE(gui.audio_muted_);

    // 2. Render master controls
    gui.render_master_controls();

    // 3. Render menu bar (without update)
    gui.render_menu_bar();

    // 4. Render menu bar with a simulated update
    {
        std::lock_guard<std::mutex> lock(gui.update_mutex_);
        gui.has_new_release_ = true;
        gui.new_release_version_ = "v9.9.9";
        gui.new_release_url_ = "https://github.com/example/Amplitron";
    }
    gui.render_menu_bar();

    // 5. Test check_for_updates (runs popen, handles failure/success gracefully)
    gui.check_for_updates();

    gui.shutdown();
    engine.shutdown();
}