#include "audio/engine/audio_engine.h"
#include "gui/commands/command_graph.h"
#include "gui/commands/command_history.h"
#include "gui/state/gui_graph_state.h"
#include "test_framework.h"

using namespace Amplitron;

TEST(AddGraphNodeCommand_executes_and_undoes_correctly) {
    AudioEngine engine;
    CommandHistory history(100);

    auto cmd = std::make_unique<AddGraphNodeCommand>(engine, "TestNode", NodeRoutingType::Splitter,
                                                     nullptr, ImVec2(100, 100));
    history.execute(std::move(cmd));

    auto& graph = engine.graph();
    ASSERT_EQ(1, graph.get_nodes().size());
    ASSERT_EQ(std::string("TestNode"), graph.get_nodes()[0].name);
    int node_id = graph.get_nodes()[0].id;
    ASSERT_TRUE(GuiGraphState::get_instance().node_positions.count(node_id));
    ASSERT_EQ(100.0f, GuiGraphState::get_instance().node_positions[node_id].position.x);

    history.undo();
    ASSERT_EQ(0, graph.get_nodes().size());
    ASSERT_FALSE(GuiGraphState::get_instance().node_positions.count(node_id));

    history.redo();
    ASSERT_EQ(1, graph.get_nodes().size());
    ASSERT_EQ(std::string("TestNode"), graph.get_nodes()[0].name);
    ASSERT_TRUE(GuiGraphState::get_instance().node_positions.count(node_id));
    ASSERT_EQ(100.0f, GuiGraphState::get_instance().node_positions[node_id].position.x);

    // Cleanup singleton
    GuiGraphState::get_instance().node_positions.erase(node_id);
}

TEST(AddGraphNodeCommand_skips_position_tracking_on_zero_coords) {
    AudioEngine engine;
    CommandHistory history(100);

    // Provide exactly (0,0) to trigger auto-placement bypass
    auto cmd = std::make_unique<AddGraphNodeCommand>(
        engine, "AutoNode", NodeRoutingType::StandardEffect, nullptr, ImVec2(0, 0));
    history.execute(std::move(cmd));

    auto& graph = engine.graph();
    ASSERT_EQ(1, graph.get_nodes().size());
    int node_id = graph.get_nodes()[0].id;

    // Position should NOT be inserted into the map for auto-placement
    ASSERT_FALSE(GuiGraphState::get_instance().node_positions.count(node_id));
}

TEST(RemoveGraphNodeCommand_stores_severed_links_and_restores_them_on_undo) {
    AudioEngine engine;
    CommandHistory history(100);
    auto& graph = engine.graph();

    int n1 = graph.add_node("N1", NodeRoutingType::Splitter, nullptr);
    int n2 = graph.add_node("N2", NodeRoutingType::Mixer, nullptr);

    int pin_out = graph.find_node(n1)->output_pin_ids[0];
    int pin_in = graph.find_node(n2)->input_pin_ids[0];

    graph.add_link(pin_out, pin_in);

    ASSERT_EQ(2, graph.get_nodes().size());
    ASSERT_EQ(1, graph.get_links().size());

    auto cmd = std::make_unique<RemoveGraphNodeCommand>(engine, n1, NodeRoutingType::Splitter,
                                                        ImVec2(0, 0));
    history.execute(std::move(cmd));

    ASSERT_EQ(1, graph.get_nodes().size());
    ASSERT_EQ(0, graph.get_links().size());

    history.undo();

    ASSERT_EQ(2, graph.get_nodes().size());
    ASSERT_EQ(1, graph.get_links().size());
    ASSERT_EQ(pin_out, graph.get_links()[0].source_pin_id);
    ASSERT_EQ(pin_in, graph.get_links()[0].dest_pin_id);

    history.redo();
    ASSERT_EQ(1, graph.get_nodes().size());
    ASSERT_EQ(0, graph.get_links().size());
}

TEST(AddGraphLinkCommand_and_RemoveGraphLinkCommand) {
    AudioEngine engine;
    CommandHistory history(100);
    auto& graph = engine.graph();

    int n1 = graph.add_node("N1", NodeRoutingType::Splitter, nullptr);
    int n2 = graph.add_node("N2", NodeRoutingType::Mixer, nullptr);

    int pin_out = graph.find_node(n1)->output_pin_ids[0];
    int pin_in = graph.find_node(n2)->input_pin_ids[0];

    auto add_cmd = std::make_unique<AddGraphLinkCommand>(engine, pin_out, pin_in);
    history.execute(std::move(add_cmd));

    ASSERT_EQ(1, graph.get_links().size());

    history.undo();
    ASSERT_EQ(0, graph.get_links().size());

    history.redo();
    ASSERT_EQ(1, graph.get_links().size());

    GraphLink link = graph.get_links()[0];
    auto rem_cmd = std::make_unique<RemoveGraphLinkCommand>(engine, link);
    history.execute(std::move(rem_cmd));

    ASSERT_EQ(0, graph.get_links().size());

    history.undo();
    ASSERT_EQ(1, graph.get_links().size());

    history.redo();
    ASSERT_EQ(0, graph.get_links().size());
}

TEST(MoveGraphNodeCommand_executes_and_undoes_correctly) {
    CommandHistory history(100);
    auto& state = GuiGraphState::get_instance();

    int node_id = 999;
    state.node_positions[node_id] = {ImVec2(10, 10), false, ImVec2(0.0f, 0.0f)};

    auto cmd = std::make_unique<MoveGraphNodeCommand>(node_id, ImVec2(10, 10), ImVec2(50, 50));
    history.execute(std::move(cmd));

    ASSERT_EQ(50.0f, state.node_positions[node_id].position.x);
    ASSERT_EQ(50.0f, state.node_positions[node_id].position.y);

    history.undo();
    ASSERT_EQ(10.0f, state.node_positions[node_id].position.x);
    ASSERT_EQ(10.0f, state.node_positions[node_id].position.y);

    history.redo();
    ASSERT_EQ(50.0f, state.node_positions[node_id].position.x);
    ASSERT_EQ(50.0f, state.node_positions[node_id].position.y);

    // Cleanup singleton state
    state.node_positions.erase(node_id);
}

TEST(RemoveGraphNodeCommand_fails_on_missing_node_and_leaves_history_clean) {
    AudioEngine engine;
    CommandHistory history(100);

    auto cmd = std::make_unique<RemoveGraphNodeCommand>(engine, 9999, NodeRoutingType::Splitter,
                                                        ImVec2(0, 0));
    history.execute(std::move(cmd));

    // The command should have returned false and thus NOT been added to history
    ASSERT_EQ(0, history.undo_size());
}

TEST(AddGraphLinkCommand_fails_on_duplicate_link) {
    AudioEngine engine;
    CommandHistory history(100);
    auto& graph = engine.graph();

    int n1 = graph.add_node("N1", NodeRoutingType::Splitter, nullptr);
    int n2 = graph.add_node("N2", NodeRoutingType::Mixer, nullptr);
    int pin_out = graph.find_node(n1)->output_pin_ids[0];
    int pin_in = graph.find_node(n2)->input_pin_ids[0];

    // First link succeeds
    auto cmd1 = std::make_unique<AddGraphLinkCommand>(engine, pin_out, pin_in);
    history.execute(std::move(cmd1));
    ASSERT_EQ(1, history.undo_size());

    // Duplicate link fails
    auto cmd2 = std::make_unique<AddGraphLinkCommand>(engine, pin_out, pin_in);
    history.execute(std::move(cmd2));

    // Should not be added to history
    ASSERT_EQ(1, history.undo_size());
    ASSERT_EQ(1, graph.get_links().size());
}

TEST(RemoveGraphLinkCommand_fails_on_missing_link) {
    AudioEngine engine;
    CommandHistory history(100);

    GraphLink bogus_link;
    bogus_link.id = 9999;
    bogus_link.source_pin_id = 1;
    bogus_link.dest_pin_id = 2;

    auto cmd = std::make_unique<RemoveGraphLinkCommand>(engine, bogus_link);
    history.execute(std::move(cmd));

    // Should not be added to history
    ASSERT_EQ(0, history.undo_size());
}

TEST(MoveGraphNodeCommand_fails_on_missing_node_or_identical_position) {
    CommandHistory history(100);
    auto& state = GuiGraphState::get_instance();

    // 1. Fails on missing node
    auto cmd1 = std::make_unique<MoveGraphNodeCommand>(8888, ImVec2(0, 0), ImVec2(10, 10));
    history.execute(std::move(cmd1));
    ASSERT_EQ(0, history.undo_size());

    // 2. Fails on identical position
    int node_id = 7777;
    state.node_positions[node_id] = {ImVec2(50, 50), false, ImVec2(0.0f, 0.0f)};
    auto cmd2 = std::make_unique<MoveGraphNodeCommand>(node_id, ImVec2(50, 50), ImVec2(50, 50));
    history.execute(std::move(cmd2));
    ASSERT_EQ(0, history.undo_size());

    state.node_positions.erase(node_id);
}

TEST(CommandGraph_DynamicPinAndDescription) {
    AudioEngine engine;
    CommandHistory history(100);
    auto& graph = engine.graph();

    // 1. Verify description string for all commands
    AddGraphNodeCommand cmd_add(engine, "Test", NodeRoutingType::Splitter, nullptr, ImVec2(0, 0));
    ASSERT_EQ(std::string("Add Node"), cmd_add.description());

    RemoveGraphNodeCommand cmd_rem(engine, 1, NodeRoutingType::Splitter, ImVec2(0, 0));
    ASSERT_EQ(std::string("Remove Node"), cmd_rem.description());

    AddGraphLinkCommand cmd_add_link(engine, 1, 2);
    ASSERT_EQ(std::string("Add Link"), cmd_add_link.description());

    GraphLink dummy_link;
    dummy_link.id = 10;
    RemoveGraphLinkCommand cmd_rem_link(engine, dummy_link);
    ASSERT_EQ(std::string("Remove Link"), cmd_rem_link.description());

    MoveGraphNodeCommand cmd_move(1, ImVec2(0, 0), ImVec2(10, 10));
    ASSERT_EQ(std::string("Move Node"), cmd_move.description());

    // 2. Setup nodes for Splitter and Mixer tests
    int s1 = graph.add_node("S1", NodeRoutingType::Splitter, nullptr);
    int m1 = graph.add_node("M1", NodeRoutingType::Mixer, nullptr);
    int n1 = graph.add_node("N1", NodeRoutingType::StandardEffect, nullptr);
    int n2 = graph.add_node("N2", NodeRoutingType::StandardEffect, nullptr);
    int n3 = graph.add_node("N3", NodeRoutingType::StandardEffect, nullptr);

    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 2u);
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 2u);

    // 3. Test Splitter auto-adds output pin when all are linked
    // Link 1: S1(outA) -> N1(in)
    auto link1 = std::make_unique<AddGraphLinkCommand>(
        engine, graph.find_node(s1)->output_pin_ids[0], graph.find_node(n1)->input_pin_ids[0]);
    history.execute(std::move(link1));
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 2u);  // still 2

    // Link 2: S1(outB) -> N2(in)
    auto link2 = std::make_unique<AddGraphLinkCommand>(
        engine, graph.find_node(s1)->output_pin_ids[1], graph.find_node(n2)->input_pin_ids[0]);
    history.execute(std::move(link2));
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 3u);  // now 3!

    // Undo Link 2 - should revert Splitter output pins to 2
    history.undo();
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 2u);

    // Redo Link 2
    history.redo();
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 3u);

    // 4. Test Mixer auto-adds input pin when all are linked
    // Link 3: N1(out) -> M1(inA)
    auto link3 = std::make_unique<AddGraphLinkCommand>(
        engine, graph.find_node(n1)->output_pin_ids[0], graph.find_node(m1)->input_pin_ids[0]);
    history.execute(std::move(link3));
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 2u);  // still 2

    // Link 4: N2(out) -> M1(inB)
    auto link4 = std::make_unique<AddGraphLinkCommand>(
        engine, graph.find_node(n2)->output_pin_ids[0], graph.find_node(m1)->input_pin_ids[1]);
    history.execute(std::move(link4));
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 3u);  // now 3!

    // Undo Link 4 - should revert Mixer input pins to 2
    history.undo();
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 2u);

    // Redo Link 4
    history.redo();
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 3u);

    // 5. Test removing link auto-removes empty pin
    // We have 3 output pins on S1 and 3 input pins on M1. Let's link S1(outC) -> N3(in)
    // S1 has 3 output pins, 2 are linked. Occupied count will become 3 after linking outC.
    // So Splitter output pins will grow to 4.
    auto link5 = std::make_unique<AddGraphLinkCommand>(
        engine, graph.find_node(s1)->output_pin_ids[2], graph.find_node(n3)->input_pin_ids[0]);
    history.execute(std::move(link5));
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 4u);

    // Now let's remove that link
    GraphLink active_link = graph.get_links().back();
    auto rem_link = std::make_unique<RemoveGraphLinkCommand>(engine, active_link);
    history.execute(std::move(rem_link));

    // Removing link should auto-remove the empty pin, shrinking Splitter output pins back to 3
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 3u);

    // Undo removing link should restore Splitter output pins to 4
    history.undo();
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 4u);

    // Redo link removal
    history.redo();
    ASSERT_EQ(graph.find_node(s1)->output_pin_ids.size(), 3u);

    // 6. Test RemoveGraphNodeCommand auto-removes empty Mixer pins and restores on Undo
    // First, let's link N3(out) -> M1(inC)
    // M1 has 3 input pins, 2 are linked (N1, N2). Linking N3 will occupy all 3, growing Mixer
    // inputs to 4.
    auto link6 = std::make_unique<AddGraphLinkCommand>(
        engine, graph.find_node(n3)->output_pin_ids[0], graph.find_node(m1)->input_pin_ids[2]);
    history.execute(std::move(link6));
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 4u);

    // Now remove node N3. This should sever link N3(out) -> M1(inC), leaving M1 with an empty pin.
    // Since M1 input count is > 2, it should auto-remove the empty pin, shrinking Mixer inputs back
    // to 3.
    auto rem_node = std::make_unique<RemoveGraphNodeCommand>(
        engine, n3, NodeRoutingType::StandardEffect, ImVec2(0, 0));
    history.execute(std::move(rem_node));

    // Verify node N3 is gone and Mixer inputs shrunk to 3
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 3u);

    // Undo node removal should restore N3, the link N3 -> M1, and Mixer inputs back to 4
    history.undo();
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 4u);

    // Redo node removal
    history.redo();
    ASSERT_EQ(graph.find_node(m1)->input_pin_ids.size(), 3u);
}
