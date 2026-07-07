#include <memory>
#include <string>
#include <vector>

#include "test_fixtures.h"
#include "test_framework.h"

// Injecting access to private fields via TestAccessor
#define private public
#include "gui/pedalboard/pedal_widget.h"
#undef private

#include "audio/effects/amp_cab/amp_simulator.h"
#include "audio/effects/amp_cab/cabinet_sim.h"
#include "audio/effects/delay_reverb/delay.h"
#include "audio/effects/delay_reverb/reverb.h"
#include "audio/effects/distortion/distortion.h"
#include "audio/effects/distortion/overdrive.h"
#include "audio/effects/dynamics/compressor.h"
#include "audio/effects/dynamics/multiband_compressor.h"
#include "audio/effects/dynamics/noise_gate.h"
#include "audio/effects/eq_filter/equalizer.h"
#include "audio/effects/eq_filter/wah.h"
#include "audio/effects/modulation/chorus.h"
#include "audio/effects/modulation/flanger.h"
#include "audio/effects/modulation/phaser.h"
#include "audio/effects/pitch/octaver.h"
#include "audio/effects/pitch/pitch_shifter.h"
#include "audio/effects/utility/looper.h"
#include "audio/effects/utility/tuner.h"
#include "gui/commands/command_history.h"
#include "gui/theme/theme.h"
#include "gui/views/gui_midi.h"

using namespace Amplitron;
using namespace TestFramework;

TEST_F(PresetTest, test_pedal_widget_color_assignment_all_types) {
    AudioEngine engine;
    engine.initialize();

    // We instantiate one widget per effect name to verify correct color lookup
    std::vector<std::pair<std::shared_ptr<Effect>, std::string>> test_cases = {
        {std::make_shared<Distortion>(), "Distortion"},
        {std::make_shared<Overdrive>(), "Overdrive"},
        {std::make_shared<Delay>(), "Delay"},
        {std::make_shared<Reverb>(), "Reverb"},
        {std::make_shared<Looper>(), "Looper"},
        {std::make_shared<Chorus>(), "Chorus"},
        {std::make_shared<Phaser>(), "Phaser"},
        {std::make_shared<Flanger>(), "Flanger"},
        {std::make_shared<Equalizer>(), "Equalizer"},
        {std::make_shared<NoiseGate>(), "Noise Gate"},
        {std::make_shared<Compressor>(), "Compressor"},
        {std::make_shared<MultiBandCompressor>(), "MultiBand Compressor"},
        {std::make_shared<CabinetSim>(), "Cabinet"},
        {std::make_shared<Octaver>(), "Octaver"},
        {std::make_shared<PitchShifter>(), "Pitch Shifter"},
        {std::make_shared<TunerPedal>(), "Tuner"},
        {std::make_shared<WahPedal>(), "Default"}  // Wah should fall back to Default color
    };

    for (const auto& tc : test_cases) {
        PedalWidget widget(engine, tc.first, 0);

        // Assert colors are loaded (alpha should be 1.0f)
        ImVec4 pedal_c = TestAccessor::pedal_color(widget);
        ImVec4 led_c = TestAccessor::led_color(widget);

        ASSERT_EQ(pedal_c.w, 1.0f);
        ASSERT_EQ(led_c.w, 1.0f);

        // Verify they match get_effect_color results
        const auto* expected_colors = get_effect_color(tc.first->name());
        ASSERT_NEAR(pedal_c.x, expected_colors->pedal_color.x, 0.001f);
        ASSERT_NEAR(pedal_c.y, expected_colors->pedal_color.y, 0.001f);
        ASSERT_NEAR(pedal_c.z, expected_colors->pedal_color.z, 0.001f);

        ASSERT_NEAR(led_c.x, expected_colors->led_color.x, 0.001f);
        ASSERT_NEAR(led_c.y, expected_colors->led_color.y, 0.001f);
        ASSERT_NEAR(led_c.z, expected_colors->led_color.z, 0.001f);
    }

    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_widget_commit_param_change_history) {
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;

    auto od = std::make_shared<Overdrive>();
    PedalWidget widget(engine, od, 0);

    // 1. Without history injected
    TestAccessor::commit_param_change(widget, 0, 0.5f, 0.8f);
    ASSERT_EQ(history.undo_size(), 0);

    // 2. With history injected
    widget.set_history(&history);
    TestAccessor::commit_param_change(widget, 0, 0.5f, 0.8f);
    ASSERT_EQ(history.undo_size(), 1);
    ASSERT_TRUE(history.can_undo());

    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_widget_footswitch_click_interaction) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    od->set_enabled(true);
    PedalWidget widget(engine, od, 0);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Setup mouse position to hover directly over the footswitch
    // From p0 = (10, 10), pedal_width = 190.0f, pedal_height = 360.0f, SWITCH_BOTTOM_OFFSET
    // = 55.0f: switch_y = 10 + 360 - 55 = 315 switch_x = 10 + (190 - 50) / 2 = 80 sw_center = (80 +
    // 25, 315 + 15) = (105, 330)
    ImVec2 p0(10.0f, 10.0f);
    ImVec2 p1(200.0f, 370.0f);
    ImVec2 sw_center(105.0f, 330.0f);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = sw_center;
    io.MouseDown[0] = false;

    bool should_remove = false;
    // Render 1: Initial frame setup (setting the mouse pos on the window)
    TestAccessor::render_footswitch_and_extras(widget, dl, p0, p1, 190.0f, 360.0f, false, true,
                                               should_remove, 1.0f);
    advance_frame();

    // Render 2: Hover frame (now fully hovered)
    TestAccessor::render_footswitch_and_extras(widget, dl, p0, p1, 190.0f, 360.0f, false, true,
                                               should_remove, 1.0f);

    // Set mouse down BEFORE advancing to trigger the click
    io.MouseDown[0] = true;
    advance_frame();

    // Render 3: Click frame (mouse down processed while hovered -> click triggers)
    TestAccessor::render_footswitch_and_extras(widget, dl, p0, p1, 190.0f, 360.0f, false, true,
                                               should_remove, 1.0f);

    // Set mouse up BEFORE advancing to release click
    io.MouseDown[0] = false;
    advance_frame();

    // Render 4: Release frame
    TestAccessor::render_footswitch_and_extras(widget, dl, p0, p1, 190.0f, 360.0f, false, true,
                                               should_remove, 1.0f);

    // Check that overdrive enabled state toggled to false
    ASSERT_FALSE(od->is_enabled());

    ImGui::End();
    engine.shutdown();
}
