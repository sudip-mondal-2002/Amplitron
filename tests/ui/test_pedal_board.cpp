/**
 * @file test_pedal_board.cpp
 * @brief Headless-safe tests for PedalBoard and PedalWidget logic and rendering.
 *
 * Covers: widget rebuild logic, active-only filtering, AmpSimulator detection,
 * PedalWidget index/effect/history accessors, and ImGui rendering for all pedal visual types
 * (standard, Amp, Tuner, Cabinet, Looper, and MultiBand Compressor) using a software ImGui context.
 */
#include "test_framework.h"
#include "test_fixtures.h"
#include <memory>
#define private public
#include "gui/pedalboard/pedal_widget.h"
#undef private
#include "gui/views/gui_midi.h"
#include "gui/commands/command_history.h"
#include "gui/state/gui_graph_state.h"
#include "gui/components/screen.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/reverb.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/tuner.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/looper.h"
#include "audio/effects/multiband_compressor.h"

#define private public
#include "gui/pedalboard/pedal_board.h"
#undef private

using namespace Amplitron;

TEST(pedal_board_construction_empty_engine) {
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;

    PedalBoard board(engine, history);
    ASSERT_TRUE(board.show_active_only());

    engine.shutdown();
}

TEST(pedal_board_rebuild_after_add_effect) {
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;

    PedalBoard board(engine, history);

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    board.rebuild_widgets();   // Must not crash

    engine.shutdown();
}

TEST(pedal_board_rebuild_with_amp_simulator) {
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;

    PedalBoard board(engine, history);

    auto od  = std::make_shared<Overdrive>();
    auto amp = std::make_shared<AmpSimulator>();
    engine.add_effect(od);
    engine.add_effect(amp);
    board.rebuild_widgets();

    // Post-amp effects are excluded from visible indices — board must remain stable
    auto rev = std::make_shared<Reverb>();
    engine.add_effect(rev);
    board.rebuild_widgets();

    engine.shutdown();
}

TEST(pedal_board_multiple_rebuilds_are_stable) {
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;

    PedalBoard board(engine, history);

    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);

    for (int i = 0; i < 10; ++i) {
        board.rebuild_widgets();
    }

    engine.shutdown();
}

TEST(pedal_board_nullptr_gui_midi_is_safe) {
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;

    // Explicit nullptr for gui_midi — must not crash during rebuild
    PedalBoard board(engine, history, nullptr);
    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    board.rebuild_widgets();

    engine.shutdown();
}

TEST(pedal_widget_index_accessor) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    PedalWidget widget(engine, od, 0);

    ASSERT_EQ(widget.get_index(), 0);
    widget.set_index(5);
    ASSERT_EQ(widget.get_index(), 5);

    engine.shutdown();
}

TEST(pedal_widget_effect_accessor_returns_same_ptr) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    PedalWidget widget(engine, od, 2);

    ASSERT_EQ(widget.get_effect(), od);

    engine.shutdown();
}

TEST(pedal_widget_set_history) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    CommandHistory history;
    PedalWidget widget(engine, od, 0);

    widget.set_history(&history);    // Must not crash
    widget.set_history(nullptr);     // nullptr also safe

    engine.shutdown();
}

TEST(pedal_widget_set_gui_midi_nullptr_is_safe) {
    AudioEngine engine;
    engine.initialize();

    auto od = std::make_shared<Overdrive>();
    PedalWidget widget(engine, od, 0);

    widget.set_gui_midi(nullptr);    // Must not crash

    engine.shutdown();
}

TEST(pedal_board_render) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;

    PedalBoard board(engine, history);
    
    // Add various pedals
    engine.add_effect(std::make_shared<Overdrive>());
    board.rebuild_widgets();

    // Call render!
    board.render();

    engine.shutdown();
}

TEST(pedal_widget_render_all_types) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();

    // 1. Standard pedal
    auto od = std::make_shared<Overdrive>();
    PedalWidget w1(engine, od, 0);
    w1.render();

    // 2. Amp pedal
    auto amp = std::make_shared<AmpSimulator>();
    PedalWidget w2(engine, amp, 1);
    w2.render();

    // 3. Tuner pedal
    auto tuner = std::make_shared<TunerPedal>();
    PedalWidget w3(engine, tuner, 2);
    w3.render();

    // 4. Cabinet pedal
    auto cab = std::make_shared<CabinetSim>();
    PedalWidget w4(engine, cab, 3);
    w4.render();

    // 5. Looper pedal
    auto looper = std::make_shared<Looper>();
    PedalWidget w5(engine, looper, 4);
    w5.render();

    // 6. MultiBand Compressor pedal
    auto mb_comp = std::make_shared<MultiBandCompressor>();
    PedalWidget w6(engine, mb_comp, 5);
    w6.render();

    engine.shutdown();
}

TEST(pedal_board_private_menu_rendering) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    PedalBoard board(engine, history, &gui_midi);

    // Call all private menu rendering routines under ScopedImGuiContext
    board.render_add_pedal_menu();
    board.render_amp_selector();
    board.render_midi_menu();
    board.render_signal_chain();

    // Toggle confirm flags to render modals
    board.show_confirm_reset_ = true;
    board.show_confirm_clear_ = true;
    board.show_confirm_midi_clear_ = true;
    board.render();

    engine.shutdown();
}

TEST(pedal_board_signal_chain_ui_interactions) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    PedalBoard board(engine, history, &gui_midi);
    auto od = std::make_shared<Overdrive>();
    engine.add_effect(od);
    board.rebuild_widgets();

    // 1. Simulate scroll zooming (Ctrl + mouse wheel)
    ImGuiIO& io = ImGui::GetIO();
    io.KeyCtrl = true;
    io.MouseWheel = 1.5f;
    board.render_signal_chain();

    // 2. Panning drag (Middle mouse button dragging)
    io.KeyCtrl = false;
    io.MouseDown[2] = true;
    io.MouseDelta = ImVec2(25.0f, -10.0f);
    board.render_signal_chain();
    io.MouseDown[2] = false;

    // 3. Zooming via scroll without Ctrl (should do scrolling)
    io.MouseWheel = -2.0f;
    io.MouseWheelH = 1.0f;
    board.render_signal_chain();
    io.MouseWheel = 0.0f;
    io.MouseWheelH = 0.0f;

    // 4. Wire Spline drafting state (dragging from pin)
    auto& graph_state = GuiGraphState::get_instance();
    graph_state.active_src_pin_id = 1;
    graph_state.active_src_pin_pos = ImVec2(100.0f, 150.0f);
    io.MousePos = ImVec2(200.0f, 250.0f);
    board.render_signal_chain();

    // 5. Dropping the wire spline (releasing dragging)
    io.MouseReleased[0] = true;
    board.render_signal_chain();
    io.MouseReleased[0] = false;
    graph_state.active_src_pin_id = -1;

    // 6. Test bypass pulses / active glows on standard pedals
    od->set_enabled(false);
    board.render_signal_chain();
    od->set_enabled(true);
    board.render_signal_chain();

    engine.shutdown();
}

TEST(pedal_widget_body_and_knob_adjustments) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    auto od = std::make_shared<Overdrive>();
    PedalWidget widget(engine, od, 0);
    widget.set_history(&history);
    widget.set_gui_midi(&gui_midi);

    // Call individual rendering helpers in PedalWidget
    ImDrawList* dl = ImGui::GetWindowDrawList();
    widget.render_standard_pedal(dl, ImVec2(0, 0), ImVec2(200, 300), 200.0f, true, 1.0f);
    widget.render_knobs(dl, ImVec2(0, 0), 200.0f, false, false, false, 1.0f);
    
    // Simulate knob scroll wheel adjustments
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheel = 1.0f;
    widget.render_knobs(dl, ImVec2(0, 0), 200.0f, false, false, false, 1.0f);
    io.MouseWheel = 0.0f;

    // Simulate Shift fine scroll
    io.MouseWheel = -1.0f;
    io.KeyShift = true;
    widget.render_knobs(dl, ImVec2(0, 0), 200.0f, false, false, false, 1.0f);
    io.MouseWheel = 0.0f;
    io.KeyShift = false;

    // Simulate Double Click reset
    io.MouseDoubleClicked[0] = true;
    widget.render_knobs(dl, ImVec2(0, 0), 200.0f, false, false, false, 1.0f);
    io.MouseDoubleClicked[0] = false;

    // Render non-standard visual body types directly
    ScreenProps props;
    props.engine = &engine;
    props.gui_midi = nullptr;

    auto tuner = std::make_shared<TunerPedal>();
    props.effect = tuner;
    props.index = 0;
    props.type = ScreenType::Tuner;
    ScreenComponent::render(dl, ImVec2(0,0), 200.0f, 1.0f, props);

    auto cab = std::make_shared<CabinetSim>();
    props.effect = cab;
    props.index = 0;
    props.type = ScreenType::Cabinet;
    ScreenComponent::render(dl, ImVec2(0,0), 200.0f, 1.0f, props);

    auto looper = std::make_shared<Looper>();
    props.effect = looper;
    props.index = 0;
    props.type = ScreenType::Looper;
    ScreenComponent::render(dl, ImVec2(0,0), 200.0f, 1.0f, props);

    auto mb_comp = std::make_shared<MultiBandCompressor>();
    props.effect = mb_comp;
    props.index = 0;
    props.type = ScreenType::MultiBandCompressor;
    ScreenComponent::render(dl, ImVec2(0,0), 200.0f, 1.0f, props);

    engine.shutdown();
}
