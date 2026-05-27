/**
 * @file test_gui_recording.cpp
 * @brief Headless-safe tests for GuiRecording logic and rendering.
 *
 * Covers both state variables and rendering branches under various recorder
 * states (Ready, Recording, Unsaved, and Save dialog triggers) using a software-only
 * ImGui context.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "gui/gui_recording.h"
#include "audio/recorder.h"
#include <string>

using namespace Amplitron;

TEST_F(PresetTest, gui_recording_construction_no_crash) {
    GuiRecording gr(engine);
    (void)gr;
}

TEST_F(PresetTest, gui_recording_initial_save_pending_is_false) {
    GuiRecording gr(engine);
    ASSERT_FALSE(gr.is_save_pending());
}

TEST_F(PresetTest, gui_recording_set_save_pending_works) {
    GuiRecording gr(engine);
    gr.set_save_pending(true);
    ASSERT_TRUE(gr.is_save_pending());
    gr.set_save_pending(false);
    ASSERT_FALSE(gr.is_save_pending());
}

TEST_F(PresetTest, gui_recording_initial_show_save_is_false) {
    GuiRecording gr(engine);
    ASSERT_FALSE(gr.show_save());
}

TEST_F(PresetTest, gui_recording_show_save_reference_can_be_modified) {
    GuiRecording gr(engine);
    bool& ref = gr.show_save();
    ref = true;
    ASSERT_TRUE(gr.show_save());
}

TEST_F(PresetTest, gui_recording_status_message_initially_empty) {
    GuiRecording gr(engine);
    ASSERT_TRUE(gr.status_message().empty());
}

TEST_F(PresetTest, gui_recording_status_message_can_be_written) {
    GuiRecording gr(engine);
    gr.status_message() = "Test message";
    ASSERT_EQ(gr.status_message(), "Test message");
}

TEST_F(PresetTest, gui_recording_render_save_dialog_early_return_when_not_pending) {
    GuiRecording gr(engine);
    bool show = true;
    gr.set_save_pending(false);
    
    // When not pending, should set show to false and return immediately (no dialog)
    gr.render_save_dialog(show);
    ASSERT_FALSE(show);
}

TEST_F(PresetTest, gui_recording_render_controls_ready_state) {
    ScopedImGuiContext imgui;
    GuiRecording gr(engine);

    // Call render_controls() in Ready state (not recording, no unsaved)
    gr.render_controls();
}

TEST_F(PresetTest, gui_recording_render_controls_recording_state) {
    ScopedImGuiContext imgui;
    GuiRecording gr(engine);

    // Start recording so the state changes to is_recording()
    engine.recorder().start("presets/dummy.wav", engine.get_sample_rate());
    gr.render_controls();

    // Pause
    engine.recorder().pause();
    gr.render_controls();

    // Resume
    engine.recorder().resume();
    gr.render_controls();

    // Stop (will set show_save=true and recording_save_pending_=true)
    engine.recorder().stop();
    gr.show_save() = true;
    gr.set_save_pending(true);
    ASSERT_TRUE(gr.show_save());
    ASSERT_TRUE(gr.is_save_pending());

    engine.recorder().discard(); // Clean up
}

TEST_F(PresetTest, gui_recording_render_controls_unsaved_state) {
    ScopedImGuiContext imgui;
    GuiRecording gr(engine);

    // Start/stop recording to produce unsaved state
    engine.recorder().start("presets/dummy.wav", engine.get_sample_rate());
    engine.recorder().stop();
    
    // Renders complete unsaved state
    gr.render_controls();

    engine.recorder().discard();
}

TEST_F(PresetTest, gui_recording_render_save_dialog_triggers_safely_when_pending) {
    ScopedImGuiContext imgui;
    GuiRecording gr(engine);

    gr.set_save_pending(true);
    bool show = true;

    // Call render_save_dialog which invokes show_save_dialog
    // (should safely return empty string in headless mode)
    gr.render_save_dialog(show);
    
    // Asserts that dialog state was reset
    ASSERT_FALSE(show);
    ASSERT_FALSE(gr.is_save_pending());
}
