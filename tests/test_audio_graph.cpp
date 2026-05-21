#include "test_framework.h"
#include "audio/audio_graph.h"
#include "audio/audio_graph_executor.h"

using namespace Amplitron;

// 1. Test Base Topology Generation Order
TEST(audio_graph_sequential_sorting) {
    AudioGraph graph;
    
    int n1 = graph.add_node("Overdrive", NodeRoutingType::StandardEffect);
    int n2 = graph.add_node("Delay", NodeRoutingType::StandardEffect);
    
    auto nodes = graph.get_nodes();
    // Connect output pin of Node 1 to input pin of Node 2
    int out_pin_1 = nodes[0].output_pin_ids[0];
    int in_pin_2  = nodes[1].input_pin_ids[0];
    
    int link_id = graph.add_link(out_pin_1, in_pin_2);
    ASSERT_TRUE(link_id != -1);
    
    const auto& sorted = graph.get_sorted_nodes();
    ASSERT_TRUE(sorted.size() == 2);
    ASSERT_TRUE(sorted[0] == n1); // Node 1 must process before Node 2
    ASSERT_TRUE(sorted[1] == n2);
}

// 2. Test Complex Parallel Splitting & Additive Merge Paths
TEST(audio_graph_parallel_split_merge) {
    AudioGraph graph;
    
    int p1 = graph.add_node("Splitter", NodeRoutingType::StandardEffect);
    int p2 = graph.add_node("Parallel Low Path", NodeRoutingType::StandardEffect);
    int p3 = graph.add_node("Parallel High Path", NodeRoutingType::StandardEffect);
    int m4 = graph.add_node("Merge Sum Node", NodeRoutingType::MergeSum);
    
    auto nodes = graph.get_nodes();
    
    // Link splits: Pedal 1 splits directly into both 2 and 3
    graph.add_link(nodes[0].output_pin_ids[0], nodes[1].input_pin_ids[0]);
    graph.add_link(nodes[0].output_pin_ids[0], nodes[2].input_pin_ids[0]);
    
    nodes = graph.get_nodes();
    
    // FIX: Link merges directly to the universal input pin of the Merge node
    graph.add_link(nodes[1].output_pin_ids[0], nodes[3].input_pin_ids[0]);
    graph.add_link(nodes[2].output_pin_ids[0], nodes[3].input_pin_ids[1]);
    
    ASSERT_TRUE(graph.rebuild_topology());
    const auto& sorted = graph.get_sorted_nodes();
    
    ASSERT_TRUE(sorted.size() == 4);
    ASSERT_TRUE(sorted[0] == p1); 
    ASSERT_TRUE(sorted[1] == p2 || sorted[1] == p3);
    ASSERT_TRUE(sorted[3] == m4); 
}

// 3. Test Cyclic Feedback Guard Loop System
TEST(audio_graph_feedback_rejection) {
    AudioGraph graph;
    
    int n1 = graph.add_node("Pedal A", NodeRoutingType::StandardEffect);
    int n2 = graph.add_node("Pedal B", NodeRoutingType::StandardEffect);
    
    auto nodes = graph.get_nodes();
    // Create connection: A -> B
    graph.add_link(nodes[0].output_pin_ids[0], nodes[1].input_pin_ids[0]);
    
    nodes = graph.get_nodes();
    // Attempt illegal feedback connection loop: B -> A
    int invalid_link = graph.add_link(nodes[1].output_pin_ids[0], nodes[0].input_pin_ids[0]);
    
    // The graph tracking engine should decline the link, returning -1 to keep loops out
    ASSERT_TRUE(invalid_link == -1);
    ASSERT_TRUE(graph.get_links().size() == 1); // Only the valid A -> B link is preserved
}

// 4. Test Actual Audio Mathematics (Splitting & Additive Merging)
TEST(audio_graph_dsp_processing) {
    AudioGraph graph;
    AudioGraphExecutor executor;
    executor.prepare(48000, 128); // Standard buffer allocation

    // We will use 3 empty "bypass" standard nodes and 1 merge node. 
    // Since pedal pointers are nullptr, they act as pure passthroughs!
    int p1 = graph.add_node("Splitter", NodeRoutingType::Splitter);
    int p2 = graph.add_node("Path A", NodeRoutingType::StandardEffect);
    int p3 = graph.add_node("Path B", NodeRoutingType::StandardEffect);
    int m4 = graph.add_node("Merge", NodeRoutingType::MergeSum);

    graph.set_node_as_input(p1, true);
    graph.set_node_as_output(m4, true);

    auto nodes = graph.get_nodes();
    // P1 -> P2 & P3 (Split)
    graph.add_link(nodes[0].output_pin_ids[0], nodes[1].input_pin_ids[0]);
    graph.add_link(nodes[0].output_pin_ids[1], nodes[2].input_pin_ids[0]);
    
    // P2 & P3 -> M4 (Merge)
    nodes = graph.get_nodes();
    graph.add_link(nodes[1].output_pin_ids[0], nodes[3].input_pin_ids[0]);
    graph.add_link(nodes[2].output_pin_ids[0], nodes[3].input_pin_ids[1]);

    ASSERT_TRUE(graph.rebuild_topology());
    executor.compile(graph);

    // Create a dummy input block filled with the value 1.0f
    std::vector<float> input_audio(64, 1.0f);
    std::vector<float> output_audio(64, 0.0f);

    // Process the block
    executor.process(input_audio.data(), output_audio.data(), 64);

    // Mathematical Proof:
    // P1 outputs 1.0f. 
    // P2 receives 1.0f and outputs 1.0f. 
    // P3 receives 1.0f and outputs 1.0f.
    // M4 receives (P2 + P3) -> (1.0f + 1.0f) = 2.0f. 
    // The final output should be strictly 2.0f.
    ASSERT_TRUE(output_audio[0] == 2.0f);
}

// 5. Test Explicit Inputs, Outputs/Sinks, and Silence of Unwired Nodes
TEST(audio_graph_explicit_inputs_sinks) {
    AudioGraph graph;
    AudioGraphExecutor executor;
    executor.prepare(48000, 128);

    // Create 4 nodes:
    // Node A (designated input - Splitter)
    // Node B (unwired, should be silent)
    // Node C & Node D (both sinks, should mix their outputs)
    int nA = graph.add_node("Input Node", NodeRoutingType::Splitter);
    graph.add_node("Unwired Node", NodeRoutingType::StandardEffect);
    int nC = graph.add_node("Sink C", NodeRoutingType::StandardEffect);
    int nD = graph.add_node("Sink D", NodeRoutingType::StandardEffect);

    graph.set_node_as_input(nA, true);
    graph.set_node_as_output(nC, true);
    graph.set_node_as_output(nD, true);
    
    // Wire: A -> C and A -> D
    auto nodes = graph.get_nodes();
    graph.add_link(nodes[0].output_pin_ids[0], nodes[2].input_pin_ids[0]);
    graph.add_link(nodes[0].output_pin_ids[1], nodes[3].input_pin_ids[0]);

    ASSERT_TRUE(graph.rebuild_topology());
    executor.compile(graph);

    std::vector<float> input_audio(64, 1.0f);
    std::vector<float> output_audio(64, 0.0f);

    executor.process(input_audio.data(), output_audio.data(), 64);

    // Case A: Sinks (nB, nC, nD) all contribute. nB is silent, nC and nD output 1.0f.
    // Mixed output: 1.0f + 1.0f = 2.0f.
    ASSERT_TRUE(output_audio[0] == 2.0f);

    // Case B: Explicitly designate nC as the ONLY output.
    graph.set_node_as_output(nC, true);
    graph.set_node_as_output(nD, false);
    ASSERT_TRUE(graph.rebuild_topology());
    executor.compile(graph);
    std::fill(output_audio.begin(), output_audio.end(), 0.0f);
    executor.process(input_audio.data(), output_audio.data(), 64);
    
    // Only nC's output contributes: 1.0f.
    ASSERT_TRUE(output_audio[0] == 1.0f);
}