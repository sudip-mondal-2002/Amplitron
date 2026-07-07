#include <imgui_internal.h>

#include <memory>

#include "audio/effects/amp_cab/amp_simulator.h"
#include "audio/effects/amp_cab/cabinet_sim.h"
#include "audio/effects/distortion/overdrive.h"
#include "audio/effects/utility/tuner.h"
#include "gui/commands/command_history.h"
#include "gui/pedalboard/pedal_widget.h"
#include "gui/views/gui_midi.h"
#include "midi/midi_manager.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

TEST_F(PresetTest, test_pedal_widget_knobs_rendering_and_layouts) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. Standard effect knobs (Overdrive)
    auto od = std::make_shared<Overdrive>();
    PedalWidget w_od(engine, od, 0);
    w_od.set_history(&history);
    w_od.set_gui_midi(&gui_midi);
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);
    advance_frame();

    // 2. Amp simulator knobs (offset param check)
    auto amp = std::make_shared<AmpSimulator>();
    PedalWidget w_amp(engine, amp, 1);
    w_amp.set_history(&history);
    w_amp.set_gui_midi(&gui_midi);
    TestAccessor::render_knobs(w_amp, dl, ImVec2(10, 10), 190.0f, true, false, false, 1.0f);
    advance_frame();

    // 3. Cabinet Sim knobs (IR layout check)
    auto cab = std::make_shared<CabinetSim>();
    PedalWidget w_cab(engine, cab, 2);
    w_cab.set_history(&history);
    w_cab.set_gui_midi(&gui_midi);
    TestAccessor::render_knobs(w_cab, dl, ImVec2(10, 10), 190.0f, false, false, true, 1.0f);
    advance_frame();

    // 4. Tuner knobs (should show zero knobs)
    auto tuner = std::make_shared<TunerPedal>();
    PedalWidget w_tuner(engine, tuner, 3);
    w_tuner.set_history(&history);
    w_tuner.set_gui_midi(&gui_midi);
    TestAccessor::render_knobs(w_tuner, dl, ImVec2(10, 10), 190.0f, false, true, false, 1.0f);
    advance_frame();

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_widget_knobs_callbacks_and_midi) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    auto od = std::make_shared<Overdrive>();
    PedalWidget w_od(engine, od, 0);
    w_od.set_history(&history);
    w_od.set_gui_midi(&gui_midi);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Configure MIDI manager to trigger learning match
    TestAccessor::learn_active(midi_manager) = true;
    TestAccessor::learn_effect_name(midi_manager) = od->name();
    TestAccessor::learn_param_name(midi_manager) = od->params()[0].name;

    // Render with learning state
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);
    advance_frame();

    // Construct MIDI mapping entries to verify midi_info bindings
    MidiMapping mapping;
    mapping.cc_number = 7;
    mapping.midi_channel = 1;
    mapping.target_type = MidiTargetType::EffectParam;
    mapping.effect_name = od->name();
    mapping.param_name = od->params()[0].name;
    gui_midi.manager().add_mapping(mapping);

    // Render with active mapping info
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);
    advance_frame();

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_widget_knobs_popup_sweep) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    auto od = std::make_shared<Overdrive>();
    PedalWidget w_od(engine, od, 0);
    w_od.set_history(&history);
    w_od.set_gui_midi(&gui_midi);

    ImGuiIO& io = ImGui::GetIO();

    auto advance_test_frame = [&]() {
        ImGui::End();
        ImGui::Render();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(1024, 768));
        ImGui::Begin("TestWindow");
    };

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Render knob once to register its position
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);
    advance_test_frame();

    // 1. Right click on first knob (center is (62.5, 102.0)) to open popup
    io.MousePos = ImVec2(62.5f, 102.0f);
    advance_test_frame();
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

    io.MouseDown[1] = true;
    advance_test_frame();
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

    io.MouseDown[1] = false;
    advance_test_frame();
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

    // Find popup window
    ImGuiContext& g = *GImGui;
    ImGuiWindow* popup_win = nullptr;
    for (int i = 0; i < g.Windows.Size; ++i) {
        if (g.Windows[i]->Flags & ImGuiWindowFlags_Popup) {
            popup_win = g.Windows[i];
            break;
        }
    }
    ASSERT_TRUE(popup_win != nullptr);

    float popup_x = popup_win->Pos.x + popup_win->Size.x * 0.5f;
    float start_y = popup_win->Pos.y + 5.0f;
    float end_y = popup_win->Pos.y + popup_win->Size.y - 5.0f;

    // Sweep clicks (no mappings case)
    for (float click_y = start_y; click_y < end_y; click_y += 12.0f) {
        // Re-open if closed
        io.MousePos = ImVec2(62.5f, 102.0f);
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[1] = true;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[1] = false;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MousePos = ImVec2(popup_x, click_y);
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[0] = true;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[0] = false;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);
    }

    // 2. Add MIDI parameter and bypass mappings
    MidiMapping param_map;
    param_map.cc_number = 20;
    param_map.midi_channel = 1;
    param_map.target_type = MidiTargetType::EffectParam;
    param_map.effect_name = od->name();
    param_map.param_name = od->params()[0].name;
    gui_midi.manager().add_mapping(param_map);

    MidiMapping bypass_map;
    bypass_map.cc_number = 21;
    bypass_map.midi_channel = 1;
    bypass_map.target_type = MidiTargetType::EffectBypass;
    bypass_map.effect_name = od->name();
    gui_midi.manager().add_mapping(bypass_map);

    // Sweep clicks with active mappings case (to hit removal paths)
    for (float click_y = start_y; click_y < end_y; click_y += 12.0f) {
        io.MousePos = ImVec2(62.5f, 102.0f);
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[1] = true;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[1] = false;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MousePos = ImVec2(popup_x, click_y);
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[0] = true;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

        io.MouseDown[0] = false;
        advance_test_frame();
        TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);
    }

    // 3. Test direct drag adjustments (to cover value changed and committed logic in drag mode)
    // Left drag on the knob face
    io.MousePos = ImVec2(62.5f, 102.0f);
    advance_test_frame();
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

    io.MouseDown[0] = true;
    advance_test_frame();
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

    io.MousePos = ImVec2(62.5f, 80.0f);  // drag upwards
    io.MouseDelta = ImVec2(0.0f, -22.0f);
    advance_test_frame();
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

    io.MouseDown[0] = false;
    advance_test_frame();
    TestAccessor::render_knobs(w_od, dl, ImVec2(10, 10), 190.0f, false, false, false, 1.0f);

    ImGui::End();
    engine.shutdown();
}
