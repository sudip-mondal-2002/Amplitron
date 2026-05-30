#include "test_framework.h"
#include "gui/commands/command_graph.h"
#include "gui/commands/command_history.h"
#include "audio/engine/audio_engine.h"
#include "gui/state/gui_graph_state.h"

using namespace Amplitron;

TEST(AddGraphNodeCommand_executes_and_undoes_correctly) {
    AudioEngine engine;
    CommandHistory history(100);

    auto cmd = std::make_unique<AddGraphNodeCommand>(engine, "TestNode", NodeRoutingType::Splitter, nullptr, ImVec2(100, 100));
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
    auto cmd = std::make_unique<AddGraphNodeCommand>(engine, "AutoNode", NodeRoutingType::StandardEffect, nullptr, ImVec2(0, 0));
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

    auto cmd = std::make_unique<RemoveGraphNodeCommand>(engine, n1, NodeRoutingType::Splitter, ImVec2(0, 0));
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
    state.node_positions[node_id] = { ImVec2(10, 10), false };

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

    auto cmd = std::make_unique<RemoveGraphNodeCommand>(engine, 9999, NodeRoutingType::Splitter, ImVec2(0, 0));
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
    auto cmd1 = std::make_unique<MoveGraphNodeCommand>(8888, ImVec2(0,0), ImVec2(10,10));
    history.execute(std::move(cmd1));
    ASSERT_EQ(0, history.undo_size());

    // 2. Fails on identical position
    int node_id = 7777;
    state.node_positions[node_id] = { ImVec2(50, 50), false };
    auto cmd2 = std::make_unique<MoveGraphNodeCommand>(node_id, ImVec2(50,50), ImVec2(50,50));
    history.execute(std::move(cmd2));
    ASSERT_EQ(0, history.undo_size());

    state.node_positions.erase(node_id);
}
