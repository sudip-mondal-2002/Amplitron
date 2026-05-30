#include "test_framework.h"
#include "test_fixtures.h"
#include <memory>

#include "midi/midi_manager.h"
#include "gui/pedalboard/pedal_widget.h"
#include "gui/views/gui_midi.h"
#include "gui/commands/command_history.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/tuner.h"

using namespace Amplitron;
using namespace TestFramework;

namespace Amplitron {
struct TestAccessor {
    static bool& learn_active(MidiManager& m) { return m.learn_active_; }
    static std::string& learn_effect_name(MidiManager& m) { return m.learn_effect_name_; }
    static std::string& learn_param_name(MidiManager& m) { return m.learn_param_name_; }

    static void render_knobs(PedalWidget& w, ImDrawList* dl, ImVec2 p0, float pedal_width, bool is_amp, bool is_tuner, bool is_ir_cab, float zoom) {
        w.render_knobs(dl, p0, pedal_width, is_amp, is_tuner, is_ir_cab, zoom);
    }
};
}

// Reusable helper to complete the current frame and begin a new one within a TestWindow context
static inline void advance_frame() {
    ImGui::End();
    ImGui::Render();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
}

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
    TestAccessor::learn_active(gui_midi.manager()) = true;
    TestAccessor::learn_effect_name(gui_midi.manager()) = od->name();
    TestAccessor::learn_param_name(gui_midi.manager()) = od->params()[0].name;

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
