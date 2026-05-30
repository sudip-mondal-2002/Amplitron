/**
 * @file test_gui_tuner.cpp
 * @brief Headless-safe tests for GuiTuner state management and rendering.
 *
 * Covers toggles, instances, and full visual tuner states (disabled, active without signal,
 * active with signal) using a software ImGui context.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include "gui/views/gui_tuner.h"
#include <memory>

using namespace Amplitron;

TEST_F(PresetTest, gui_tuner_construction_no_crash) {
    GuiTuner gt;
    (void)gt;
}

TEST_F(PresetTest, gui_tuner_render_disabled_state) {
    ScopedImGuiContext imgui;
    GuiTuner gt;

    TunerProps props;
    props.note_name_fn = [](int idx) { return "E"; };
    gt.set_props(props);

    bool show = false;
    gt.render(show);
}

TEST_F(PresetTest, gui_tuner_render_active_state_no_signal) {
    ScopedImGuiContext imgui;
    GuiTuner gt;

    bool mute_changed_called = false;
    TunerProps props;
    props.has_signal = false;
    props.note_name_fn = [](int idx) { return "E"; };
    props.on_mute_changed = [&](bool m) { mute_changed_called = true; };
    gt.set_props(props);

    bool show = true;
    gt.render(show);
}

TEST_F(PresetTest, gui_tuner_render_active_state_with_signal) {
    ScopedImGuiContext imgui;
    GuiTuner gt;

    TunerProps props;
    props.has_signal = true;
    props.note_idx = 4;
    props.octave = 2;
    props.cents = -5.5f;
    props.freq = 82.4f;
    props.note_name_fn = [](int idx) { return "E"; };
    gt.set_props(props);

    bool show = true;
    gt.render(show);
}
