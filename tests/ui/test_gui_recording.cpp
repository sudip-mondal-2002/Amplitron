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
#include "gui/views/gui_recording.h"
#include <string>

using namespace Amplitron;

TEST_F(PresetTest, gui_recording_construction_no_crash) {
    GuiRecording gr;
    (void)gr;
}

TEST_F(PresetTest, gui_recording_initial_needs_save_dialog_is_false) {
    GuiRecording gr;
    ASSERT_FALSE(gr.needs_save_dialog());
}

TEST_F(PresetTest, gui_recording_set_state_works) {
    GuiRecording gr;
    gr.set_state([](RecordingState& s) {
        s.needs_save = true;
        s.status_msg = "Recording stopped";
    });
    ASSERT_TRUE(gr.needs_save_dialog());
}

TEST_F(PresetTest, gui_recording_render_controls_ready_state) {
    ScopedImGuiContext imgui;
    GuiRecording gr;

    RecordingProps props;
    gr.set_props(props);
    gr.render();
}

TEST_F(PresetTest, gui_recording_render_controls_recording_state) {
    ScopedImGuiContext imgui;
    GuiRecording gr;

    float dummy_waveform[16] = {0.0f};
    RecordingProps props;
    props.is_recording = true;
    props.waveform_buf = dummy_waveform;
    props.waveform_size = 16;
    props.duration = 4.5f;

    gr.set_props(props);
    gr.render();

    // Paused state
    props.is_paused = true;
    gr.set_props(props);
    gr.render();
}

TEST_F(PresetTest, gui_recording_render_save_dialog_triggers_safely) {
    ScopedImGuiContext imgui;
    GuiRecording gr;

    gr.set_state([](RecordingState& s) {
        s.needs_save = true;
    });

    std::string saved_path = "";
    gr.render_save_dialog([&](const std::string& path) {
        saved_path = path;
    });

    // In headless test environment, the native dialog handles gracefully and should
    // clear needs_save.
    ASSERT_FALSE(gr.needs_save_dialog());
}
