#include "test_framework.h"
#include "test_fixtures.h"
#include <memory>

#include "gui/pedalboard/pedal_board.h"
#include "gui/pedalboard/pedal_widget.h"
#include "gui/views/gui_midi.h"
#include "gui/commands/command_history.h"
#include "gui/state/gui_graph_state.h"
#include "audio/effects/overdrive.h"
#include "audio/effects/amp_simulator.h"

using namespace Amplitron;
using namespace TestFramework;

namespace Amplitron {
struct TestAccessor {
    static void render_signal_chain(PedalBoard& b) { b.render_signal_chain(); }
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

TEST_F(PresetTest, test_pedal_board_chain_scrolling_and_zooming) {
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

    auto& ui_state = GuiGraphState::get_instance();
    ui_state.zoom = 1.0f;
    ui_state.target_zoom = 1.0f;
    ui_state.scrolling = ImVec2(0, 0);
    ui_state.target_scrolling = ImVec2(0, 0);

    // Initial render
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    TestAccessor::render_signal_chain(board);
    
    // Position the mouse inside the canvas panning hotspot area initially
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(512, 384);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 1. Zoom with Ctrl + mouse wheel (Zoom in)
    // MousePos is stateful (set before advance_frame), MouseWheel is instantaneous (set after advance_frame)
    io.MousePos = ImVec2(512, 384);
    advance_frame();
    io.KeyCtrl = true;
    io.MouseWheel = 1.0f;
    TestAccessor::render_signal_chain(board);

    float zoomed_in_target = ui_state.target_zoom;
    
    // Reset control keys
    io.KeyCtrl = false;
    io.MouseWheel = 0.0f;
    advance_frame();
    TestAccessor::render_signal_chain(board);
    
    ASSERT_GT(zoomed_in_target, 1.0f);

    // 2. Zoom with Ctrl + mouse wheel (Zoom out)
    io.MousePos = ImVec2(512, 384);
    advance_frame();
    io.KeyCtrl = true;
    io.MouseWheel = -1.0f;
    TestAccessor::render_signal_chain(board);
    
    io.KeyCtrl = false;
    io.MouseWheel = 0.0f;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 3. Middle mouse button panning drag
    io.MouseDown[2] = true;
    io.MousePos = ImVec2(500, 380);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MousePos = ImVec2(520, 390);
    advance_frame();
    TestAccessor::render_signal_chain(board);
    
    io.MouseDown[2] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 4. Left-click panning (implicit — no hand tool toggle needed)
    // Reset scrolling offset to prove left-click panning actually moves it
    ui_state.scrolling = ImVec2(0, 0);
    ui_state.target_scrolling = ImVec2(0, 0);
    // Left-click drag on empty canvas should pan without any mode switch
    io.MouseDown[0] = true;
    io.MousePos = ImVec2(500, 380);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MousePos = ImVec2(480, 370);
    advance_frame();
    io.MouseDelta = ImVec2(-20, -10);
    TestAccessor::render_signal_chain(board);
    
    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);
    
    // Scrolling should have changed after the drag
    ASSERT_NE(ui_state.scrolling.x, 0.0f);

    // 5. Scroll without Ctrl (should do scrolling/panning)
    io.MousePos = ImVec2(512, 384);
    advance_frame();
    io.MouseWheel = 2.0f;
    io.MouseWheelH = -1.0f;
    TestAccessor::render_signal_chain(board);
    
    io.MouseWheel = 0.0f;
    io.MouseWheelH = 0.0f;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 6. Full Screen toggle button
    io.MousePos = ImVec2(1024 - 40, 20); // hover FS button
    advance_frame();
    io.MouseDown[0] = true;
    TestAccessor::render_signal_chain(board);
    
    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, test_implicit_hand_tool) {
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

    auto& ui_state = GuiGraphState::get_instance();
    ui_state.zoom = 1.0f;
    ui_state.target_zoom = 1.0f;
    ui_state.scrolling = ImVec2(0, 0);
    ui_state.target_scrolling = ImVec2(0, 0);

    // Place the Overdrive pedal at a known position
    for (const auto& node : engine.graph().get_nodes()) {
        ui_state.node_positions[node.id] = { ImVec2(100.0f, 100.0f), false };
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    TestAccessor::render_signal_chain(board);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(512, 384);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // ── Test 1: Left-click drag on pedal widget → canvas does NOT pan ──
    // The widget's native_drag_handle captures the click; the canvas
    // InvisibleButton beneath it should NOT receive the drag.
    io.MousePos = ImVec2(110.0f, 110.0f); // Overdrive drag handle area
    io.MouseDown[0] = true;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MousePos = ImVec2(150.0f, 120.0f); // drag moves within widget
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // Scrolling must remain at zero — the widget consumed the drag
    ASSERT_EQ(ui_state.scrolling.x, 0.0f);
    ASSERT_EQ(ui_state.scrolling.y, 0.0f);

    // ── Test 2: Left-click click (no drag) on empty canvas → no pan ──
    // A click without mouse movement should not trigger panning.
    ui_state.scrolling = ImVec2(0, 0);
    ui_state.target_scrolling = ImVec2(0, 0);
    io.MousePos = ImVec2(512, 384);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = true;               // press
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = false;              // release, no movement between frames
    advance_frame();
    TestAccessor::render_signal_chain(board);

    ASSERT_EQ(ui_state.scrolling.x, 0.0f);
    ASSERT_EQ(ui_state.scrolling.y, 0.0f);

    // ── Test 3: Left-click drag on empty canvas → pans (implicit hand) ──
    // Proves the implicit left-click panning works without a hand tool toggle.
    // NOTE: MouseDown and MousePos MUST be on separate frames so that ImGui
    // captures MouseClickedPos at (500, 380); the subsequent mouse movement
    // to (480, 370) exceeds the drag threshold and triggers isMouseDragging.
    ui_state.scrolling = ImVec2(0, 0);
    ui_state.target_scrolling = ImVec2(0, 0);
    io.MousePos = ImVec2(500, 380);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = true;               // press at (500, 380) — click pos captured here
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MousePos = ImVec2(480, 370);       // drag to (480, 370) on next frame
    advance_frame();
    io.MouseDelta = ImVec2(-20, -10);     // simulate movement delta (EndFrame zeroed MousePosPrev)
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    ASSERT_NE(ui_state.scrolling.x, 0.0f);

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_board_chain_nodes_and_wiring) {
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

    auto& ui_state = GuiGraphState::get_instance();
    auto& audio_graph = engine.graph();

    // Set initial positions
    for (const auto& node : audio_graph.get_nodes()) {
        ui_state.node_positions[node.id] = { ImVec2(100.0f, 100.0f), false };
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    TestAccessor::render_signal_chain(board);
    
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(150, 150);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 1. Dragging a standard node
    // Hover over the drag handle area of the first node
    io.MousePos = ImVec2(110.0f, 110.0f);
    io.MouseDown[0] = true;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MousePos = ImVec2(150.0f, 120.0f);
    io.MouseDelta = ImVec2(40.0f, 10.0f);
    advance_frame();
    TestAccessor::render_signal_chain(board);
    
    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 2. Wire spline drafting (dragging from output pin)
    ui_state.active_src_pin_id = 1;
    ui_state.active_src_pin_pos = ImVec2(150.0f, 150.0f);
    io.MousePos = ImVec2(300.0f, 300.0f);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 3. Drop wire spline to complete a connection or release
    advance_frame();
    io.MouseReleased[0] = true;
    TestAccessor::render_signal_chain(board);
    
    io.MouseReleased[0] = false;
    ui_state.active_src_pin_id = -1;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 4. Test bypass active glows and disabled overlays
    od->set_enabled(false);
    advance_frame();
    TestAccessor::render_signal_chain(board);
    
    od->set_enabled(true);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 5. Test removal cross [X] click on standard node
    int deletable_node_id = -1;
    for (const auto& node : audio_graph.get_nodes()) {
        if (node.name != "Input" && node.name != "Amp Sim") {
            deletable_node_id = node.id;
            break;
        }
    }
    if (deletable_node_id != -1) {
        io.MousePos = ImVec2(100.0f + 190.0f - 12.0f, 100.0f + 10.0f); // Approximate cross button location
        advance_frame();
        io.MouseDown[0] = true;
        TestAccessor::render_signal_chain(board);
        
        io.MouseDown[0] = false;
        advance_frame();
        TestAccessor::render_signal_chain(board);
    }

    ImGui::End();
    engine.shutdown();
}

TEST_F(PresetTest, test_pedal_board_chain_extended) {
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

    auto& ui_state = GuiGraphState::get_instance();
    auto& audio_graph = engine.graph();

    // Reset zoom and panning
    ui_state.zoom = 1.0f;
    ui_state.target_zoom = 1.0f;
    ui_state.scrolling = ImVec2(0, 0);
    ui_state.target_scrolling = ImVec2(0, 0);

    // Add splitter and mixer nodes via API
    audio_graph.add_node("Splitter", NodeRoutingType::Splitter);
    audio_graph.add_node("Mixer", NodeRoutingType::Mixer);
    board.rebuild_widgets();

    // Find Splitter and Mixer nodes
    int splitter_id = -1;
    int mixer_id = -1;
    int input_id = -1;
    int amp_id = -1;
    for (const auto& node : audio_graph.get_nodes()) {
        if (node.name == "Splitter") splitter_id = node.id;
        else if (node.name == "Mixer") mixer_id = node.id;
        else if (node.name == "Input") input_id = node.id;
        else if (node.name == "Amp Sim") amp_id = node.id;
    }

    ASSERT_NE(splitter_id, -1);
    ASSERT_NE(mixer_id, -1);

    // Place nodes at known coordinates
    ui_state.node_positions[splitter_id] = { ImVec2(300.0f, 100.0f), false };
    ui_state.node_positions[mixer_id] = { ImVec2(500.0f, 100.0f), false };
    ui_state.node_positions[input_id] = { ImVec2(50.0f, 100.0f), false };
    ui_state.node_positions[amp_id] = { ImVec2(700.0f, 100.0f), false };

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("TestWindow");
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    TestAccessor::render_signal_chain(board);

    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(10, 10);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // 1. Drag the Splitter utility node
    // Click on utility drag handle relative to canvas_pos
    ImVec2 drag_pos(canvas_pos.x + 300.0f + 40.0f, canvas_pos.y + 100.0f + 20.0f);
    io.MousePos = drag_pos;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // MouseDown
    io.MouseDown[0] = true;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // Drag move
    io.MousePos = ImVec2(drag_pos.x + 40.0f, drag_pos.y + 10.0f);
    advance_frame();
    io.MouseDelta = ImVec2(40.0f, 10.0f);
    TestAccessor::render_signal_chain(board);

    // MouseUp
    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // Position of Splitter should have changed
    ASSERT_NE(ui_state.node_positions[splitter_id].position.x, 300.0f);

    // Reset coordinates to known static values
    ui_state.node_positions[splitter_id] = { ImVec2(300.0f, 100.0f), false };

    // 2. Wire spline drafting and manual connection drop
    auto* splitter_node = audio_graph.find_node(splitter_id);
    auto* mixer_node = audio_graph.find_node(mixer_id);
    ASSERT_TRUE(splitter_node != nullptr);
    ASSERT_TRUE(mixer_node != nullptr);

    int splitter_out_pin = splitter_node->output_pin_ids[0];
    int mixer_in_pin = mixer_node->input_pin_ids[0];

    // Compute exact screen positions of pins
    ImVec2 splitter_pin_pos(canvas_pos.x + 300.0f + 130.0f + 2.0f, canvas_pos.y + 100.0f + 39.67f);
    ImVec2 mixer_pin_pos(canvas_pos.x + 500.0f - 2.0f, canvas_pos.y + 100.0f + 39.67f);

    // Set active drafting state
    ui_state.active_src_pin_id = splitter_out_pin;
    ui_state.active_src_pin_pos = splitter_pin_pos;

    // Hover Mixer input pin and release mouse button to connect
    io.MousePos = mixer_pin_pos;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    advance_frame();
    io.MouseReleased[0] = true;
    TestAccessor::render_signal_chain(board);

    advance_frame();
    io.MouseReleased[0] = false;
    ui_state.active_src_pin_id = -1;
    TestAccessor::render_signal_chain(board);

    // The connection should now exist in the graph!
    bool link_found = false;
    for (const auto& link : audio_graph.get_links()) {
        if (link.source_pin_id == splitter_out_pin && link.dest_pin_id == mixer_in_pin) {
            link_found = true;
            break;
        }
    }
    ASSERT_TRUE(link_found);

    // 3. Test link deletion via bezier hover (click on the splitter output pin where connection starts)
    io.MousePos = splitter_pin_pos;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = true;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // The link should be deleted
    link_found = false;
    for (const auto& link : audio_graph.get_links()) {
        if (link.source_pin_id == splitter_out_pin && link.dest_pin_id == mixer_in_pin) {
            link_found = true;
            break;
        }
    }
    ASSERT_FALSE(link_found);

    // 4. Test utility node deletion (Splitter) via close button [X]
    ImVec2 close_pos(canvas_pos.x + 300.0f + 130.0f - 12.0f, canvas_pos.y + 100.0f + 14.0f);
    io.MousePos = close_pos;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = true;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    io.MouseDown[0] = false;
    advance_frame();
    TestAccessor::render_signal_chain(board);

    // The Splitter node should be removed
    ASSERT_TRUE(audio_graph.find_node(splitter_id) == nullptr);

    // 5. Test bypass rails animation (different segments)
    od->set_enabled(false);
    
    io.DeltaTime = 0.1f;
    for (int frame = 0; frame < 15; ++frame) {
        advance_frame();
        TestAccessor::render_signal_chain(board);
    }

    od->set_enabled(true);
    advance_frame();
    TestAccessor::render_signal_chain(board);

    ImGui::End();
    engine.shutdown();
}
