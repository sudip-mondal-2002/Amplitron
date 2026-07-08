#include <imgui_internal.h>

#include <memory>

#include "audio/effects/amp_cab/amp_simulator.h"
#include "audio/effects/distortion/overdrive.h"
#include "gui/commands/command_history.h"
#define private public
#include "gui/pedalboard/pedal_board.h"
#include "gui/pedalboard/pedal_widget.h"
#undef private
#include "gui/views/gui_midi.h"
#include "midi/midi_manager.h"
#include "test_fixtures.h"
#include "test_framework.h"

using namespace Amplitron;
using namespace TestFramework;

static ImGuiID get_item_id(const char *window_substr, const char *item_id_str) {
    ImGuiContext &g = *GImGui;
    ImGuiID popup_id = ImGui::GetID(window_substr);
    char popup_window_name[64];
    snprintf(popup_window_name, sizeof(popup_window_name), "##Popup_%08x", popup_id);

    for (int i = 0; i < g.Windows.Size; i++) {
        if (strstr(g.Windows[i]->Name, window_substr) ||
            strstr(g.Windows[i]->Name, popup_window_name)) {
            return g.Windows[i]->GetID(item_id_str);
        }
    }
    return 0;
}

static void click_item(const char *window_substr, const char *item_id_str) {
    ImGuiID id = get_item_id(window_substr, item_id_str);
    if (id != 0) {
        ImGuiContext &g = *GImGui;
        g.NavActivateId = id;
        g.NavActivateDownId = id;
        g.NavActivatePressedId = id;
    }
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

    // Explicitly call the callbacks by calling add_effect_and_show to exercise
    // that path
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
    TestAccessor::learn_active(midi_manager) = false;
    TestAccessor::current_port(midi_manager) = 0;
    TestAccessor::current_port_name(midi_manager) = "Test MIDI Port";
    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_frame();

    // 3. Render learning state in MIDI popup
    TestAccessor::learn_active(midi_manager) = true;
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

TEST_F(PresetTest, test_pedal_board_menu_extended) {
    ScopedImGuiContext imgui;
    AudioEngine engine;
    engine.initialize();
    CommandHistory history;
    MidiManager midi_manager;
    GuiMidi gui_midi(midi_manager);

    PedalBoard board(engine, history, &gui_midi);

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

    // 1. Test auto-add amp simulator path in render_amp_selector when engine has no effects
    engine.effects().clear();
    board.rebuild_widgets();
    TestAccessor::render_amp_selector(board);
    advance_test_frame();
    // Re-verify that amp was added automatically
    ASSERT_GE(board.find_amp_index(), 0);

    // 2. Click "+ Add Pedal" button programmatically
    click_item("TestWindow", "+ Add Pedal");
    TestAccessor::render_add_pedal_menu(board);
    advance_test_frame();

    // Click every single effect adding option using direct popup open
    const std::vector<std::string> effects_to_add = {"Overdrive",
                                                     "Distortion",
                                                     "Noise Gate",
                                                     "Compressor",
                                                     "MultiBand Compressor",
                                                     "Chorus",
                                                     "Phaser",
                                                     "Flanger",
                                                     "Delay",
                                                     "Reverb",
                                                     "Looper",
                                                     "Wah",
                                                     "Octaver",
                                                     "Pitch Shifter",
                                                     "Equalizer",
                                                     "Cabinet Sim",
                                                     "+ Signal Splitter Node (1 In -> N-Out)",
                                                     "+ Signal Mixer Node (N-In -> 1 Out)"};

    for (const auto &eff_name : effects_to_add) {
        ImGui::OpenPopup("AddPedalPopup");
        TestAccessor::render_add_pedal_menu(board);
        advance_test_frame();

        click_item("AddPedalPopup", eff_name.c_str());

        ImGui::OpenPopup("AddPedalPopup");
        TestAccessor::render_add_pedal_menu(board);
        advance_test_frame();
    }

    // 3. Click Amp button programmatically
    click_item("TestWindow", "Amp: Amp");
    TestAccessor::render_amp_selector(board);
    advance_test_frame();

    // Click every amp model
    const std::vector<std::string> amp_models = {"Clean American", "British Crunch",
                                                 "High Gain Modern", "Jazz Warm"};

    for (const auto &model_name : amp_models) {
        ImGui::OpenPopup("AmpSelectorPopup");
        TestAccessor::render_amp_selector(board);
        advance_test_frame();

        click_item("AmpSelectorPopup", model_name.c_str());

        ImGui::OpenPopup("AmpSelectorPopup");
        TestAccessor::render_amp_selector(board);
        advance_test_frame();
    }

    // 4. Test disconnected MIDI state in MIDI menu by clicking MIDI button programmatically
    TestAccessor::learn_active(midi_manager) = false;
    TestAccessor::current_port(midi_manager) = -1;
    click_item("TestWindow", "MIDI");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    // Setup connected/learning MIDI states for action clicks
    TestAccessor::learn_active(midi_manager) = true;
    TestAccessor::current_port(midi_manager) = 0;
    TestAccessor::current_port_name(midi_manager) = "Virtual Port";

    // Add a dummy mapping so mapping list is non-empty
    MidiMapping mapping;
    mapping.cc_number = 10;
    mapping.midi_channel = 1;
    mapping.target_type = MidiTargetType::EffectParam;
    mapping.effect_name = "Overdrive";
    mapping.param_name = "Drive";
    gui_midi.manager().add_mapping(mapping);

    // Cancel Learn Mode
    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    click_item("MidiMenuPopup", "Cancel Learn Mode");

    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    // Clear All Mappings
    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    click_item("MidiMenuPopup", "Clear All Mappings");

    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    // Save Config
    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    click_item("MidiMenuPopup", "Save Config");

    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    // Load Config
    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    click_item("MidiMenuPopup", "Load Config");

    ImGui::OpenPopup("MidiMenuPopup");
    TestAccessor::render_midi_menu(board);
    advance_test_frame();

    // 5. Clean up window
    ImGui::End();
    engine.shutdown();
}
