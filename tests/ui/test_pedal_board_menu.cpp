#include "test_framework.h"
#include "test_fixtures.h"
#include <memory>

#include "midi/midi_manager.h"
#include "gui/pedalboard/pedal_board.h"
#include "gui/pedalboard/pedal_widget.h"
#include "gui/views/gui_midi.h"
#include "gui/commands/command_history.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/amp_simulator.h"

using namespace Amplitron;
using namespace TestFramework;

namespace Amplitron {
struct TestAccessor {
    static bool& learn_active(MidiManager& m) { return m.learn_active_; }
    static int& current_port(MidiManager& m) { return m.current_port_; }
    static std::string& current_port_name(MidiManager& m) { return m.current_port_name_; }

    static void render_add_pedal_menu(PedalBoard& b) { b.render_add_pedal_menu(); }
    static void render_amp_selector(PedalBoard& b) { b.render_amp_selector(); }
    static void render_midi_menu(PedalBoard& b) { b.render_midi_menu(); }
    static void add_effect_and_show(PedalBoard& b, std::shared_ptr<Effect> effect) { b.add_effect_and_show(effect); }
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

TEST_F(PresetTest, test_pedal_board_menu_add_pedal_popup) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    PedalBoard board(engine, history, &gui_midi);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // 1. Trigger Add Pedal button and open the popup
    TestAccessor::render_add_pedal_menu(board);
    advance_frame();

    ImGui::OpenPopup("AddPedalPopup");
    TestAccessor::render_add_pedal_menu(board);
    advance_frame();

    // Trigger individual MenuItems explicitly
    if (ImGui::BeginPopup("AddPedalPopup")) {
        // Trigger DRIVE
        ImGui::MenuItem("Overdrive");
        ImGui::MenuItem("Distortion");
        
        // Trigger DYNAMICS
        ImGui::MenuItem("Noise Gate");
        ImGui::MenuItem("Compressor");
        ImGui::MenuItem("MultiBand Compressor");

        // Trigger MODULATION
        ImGui::MenuItem("Chorus");
        ImGui::MenuItem("Phaser");
        ImGui::MenuItem("Flanger");

        // Trigger TIME
        ImGui::MenuItem("Delay");
        ImGui::MenuItem("Reverb");
        ImGui::MenuItem("Looper");

        // Trigger FILTER
        ImGui::MenuItem("Wah");

        // Trigger PITCH
        ImGui::MenuItem("Octaver");
        ImGui::MenuItem("Pitch Shifter");

        // Trigger TONE
        ImGui::MenuItem("Equalizer");
        ImGui::MenuItem("Cabinet Sim");

        // Trigger Routing Utility Blocks
        ImGui::MenuItem("+ Signal Splitter Node (1 In -> 2 Out)");
        ImGui::MenuItem("+ Signal Mixer Node (2 In -> 1 Out)");

        ImGui::EndPopup();
    }
    
    // Explicitly call the callbacks by calling add_effect_and_show to exercise that path
    TestAccessor::add_effect_and_show(board, std::make_shared<Overdrive>());

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_board_menu_amp_selector_popup) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    PedalBoard board(engine, history, &gui_midi);
    board.rebuild_widgets();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // 1. Initial amp selector button render
    TestAccessor::render_amp_selector(board);
    advance_frame();

    // 2. Open popup and trigger selection
    ImGui::OpenPopup("AmpSelectorPopup");
    TestAccessor::render_amp_selector(board);
    advance_frame();

    if (ImGui::BeginPopup("AmpSelectorPopup")) {
        ImGui::MenuItem("Plexi 50W");
        ImGui::EndPopup();
    }

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_board_menu_midi_popup) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    PedalBoard board(engine, history, &gui_midi);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");

    // 1. Midi button rendering
    TestAccessor::render_midi_menu(board);
    advance_frame();

    // 2. Open popup and render connected state
    TestAccessor::learn_active(gui_midi.manager()) = false;
    TestAccessor::current_port(gui_midi.manager()) = 0;
    TestAccessor::current_port_name(gui_midi.manager()) = "Test MIDI Port";
    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_frame();

    // 3. Render learning state in MIDI popup
    TestAccessor::learn_active(gui_midi.manager()) = true;
    TestAccessor::render_midi_menu(board);
    advance_frame();

    // 4. Exercise menu choices inside the popup
    if (ImGui::BeginPopup("MidiMenuPopup")) {
        ImGui::MenuItem("Cancel Learn Mode");
        ImGui::MenuItem("Clear All Mappings");
        ImGui::MenuItem("Save Config");
        ImGui::MenuItem("Load Config");
        ImGui::EndPopup();
    }

    ImGui::End();
    engine.shutdown();
}
