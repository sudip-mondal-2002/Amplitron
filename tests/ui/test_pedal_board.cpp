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
#include "gui/pedal_widget.h"
#include "gui/gui_midi.h"
#include "gui/command_history.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/reverb.h"
#include "audio/effects/amp_simulator.h"
#include "audio/effects/tuner.h"
#include "audio/effects/cabinet_sim.h"
#include "audio/effects/looper.h"
#include "audio/effects/multiband_compressor.h"

#define private public
#include "gui/pedal_board.h"
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
