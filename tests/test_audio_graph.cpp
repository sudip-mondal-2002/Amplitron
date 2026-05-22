#include "audio/audio_graph.h"
#include "audio/audio_graph_executor.h"
#include "audio/effect_factory.h"
#include "preset_json.h"
#include "test_framework.h"
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>

using namespace Amplitron;

namespace {
std::string mock_save_graph(const AudioGraph &graph) {
  nlohmann::json j;
  j["format_version"] = 2; // version 2 = graph, version 1 = linear

  nlohmann::json nodes_json = nlohmann::json::array();
  for (const auto &node : graph.get_nodes()) {
    nlohmann::json node_j;
    node_j["id"] = node.id;
    node_j["name"] = node.name;
    node_j["routing_type"] = static_cast<int>(node.routing_type);
    node_j["is_graph_input"] = node.is_graph_input;
    node_j["is_graph_output"] = node.is_graph_output;
    node_j["x"] = node.x;
    node_j["y"] = node.y;

    if (node.pedal) {
      node_j["effect"] = node.pedal->get_params();
      node_j["effect"]["type"] = node.pedal->name();
    }

    node_j["input_pin_ids"] = node.input_pin_ids;
    node_j["output_pin_ids"] = node.output_pin_ids;
    nodes_json.push_back(node_j);
  }

  nlohmann::json links_json = nlohmann::json::array();
  for (const auto &link : graph.get_links()) {
    nlohmann::json link_j;
    link_j["id"] = link.id;
    link_j["source_pin_id"] = link.source_pin_id;
    link_j["dest_pin_id"] = link.dest_pin_id;
    links_json.push_back(link_j);
  }

  j["nodes"] = nodes_json;
  j["links"] = links_json;

  return j.dump(4);
}

bool mock_load_graph(const std::string &json_str, AudioGraph &graph) {
  nlohmann::json j = nlohmann::json::parse(json_str, nullptr, false);
  if (j.is_discarded())
    return false;

  // Handle Legacy Linear Preset Format
  if (j.contains("format_version") && j["format_version"] == 1 &&
      !j.contains("nodes")) {
    PresetData legacy;
    if (!from_json_ext(json_str, legacy))
      return false;

    int last_out_pin = -1;
    for (size_t i = 0; i < legacy.effects.size(); ++i) {
      auto effect = EffectFactory::instance().create(legacy.effects[i].type);
      if (effect) {
        nlohmann::json fx_json;
        to_json(fx_json, legacy.effects[i]);
        effect->set_params(fx_json);
      }
      int node_id = graph.add_node(legacy.effects[i].type,
                                   NodeRoutingType::StandardEffect, effect);
      if (i == 0)
        graph.set_node_as_input(node_id, true);
      if (i == legacy.effects.size() - 1)
        graph.set_node_as_output(node_id, true);

      auto *node = graph.find_node(node_id);
      if (last_out_pin != -1 && node && !node->input_pin_ids.empty()) {
        graph.add_link(last_out_pin, node->input_pin_ids[0]);
      }
      if (node && !node->output_pin_ids.empty()) {
        last_out_pin = node->output_pin_ids[0];
      }
    }
    return graph.rebuild_topology();
  }

  // Handle New Graph Format
  if (!j.contains("nodes") || !j.contains("links"))
    return false;

  std::map<int, int> node_map;
  std::map<int, int> pin_map; // old pin -> new pin

  for (const auto &node_j : j["nodes"]) {
    std::shared_ptr<Effect> effect = nullptr;
    if (node_j.contains("effect")) {
      std::string type = node_j["effect"].value("type", "");
      effect = EffectFactory::instance().create(type);
      if (effect) {
        effect->set_params(node_j["effect"]);
      }
    }

    int old_id = node_j["id"];
    int new_id = graph.add_node(
        node_j["name"], static_cast<NodeRoutingType>(node_j["routing_type"]),
        effect);
    node_map[old_id] = new_id;

    graph.set_node_as_input(new_id, node_j["is_graph_input"]);
    graph.set_node_as_output(new_id, node_j["is_graph_output"]);
    if (node_j.contains("x") && node_j.contains("y")) {
      graph.set_node_position(new_id, node_j["x"], node_j["y"]);
    }

    const auto *new_node = graph.find_node(new_id);
    if (new_node) {
      auto old_in_pins = node_j["input_pin_ids"];
      for (size_t i = 0;
           i < old_in_pins.size() && i < new_node->input_pin_ids.size(); ++i) {
        pin_map[old_in_pins[i]] = new_node->input_pin_ids[i];
      }
      auto old_out_pins = node_j["output_pin_ids"];
      for (size_t i = 0;
           i < old_out_pins.size() && i < new_node->output_pin_ids.size();
           ++i) {
        pin_map[old_out_pins[i]] = new_node->output_pin_ids[i];
      }
    }
  }

  for (const auto &link_j : j["links"]) {
    int old_src = link_j["source_pin_id"];
    int old_dst = link_j["dest_pin_id"];
    // "6. Invalid/corrupt graph preset: missing node handled gracefully"
    if (pin_map.find(old_src) == pin_map.end() ||
        pin_map.find(old_dst) == pin_map.end()) {
      return false; // Load fails gracefully
    }
    if (graph.add_link(pin_map[old_src], pin_map[old_dst]) == -1) {
      return false; // Load fails gracefully if link is invalid/creates cycle
    }
  }

  return graph.rebuild_topology();
}
} // namespace

// 1. Test Base Topology Generation Order
TEST(audio_graph_sequential_sorting) {
  AudioGraph graph;

  int n1 = graph.add_node("Overdrive", NodeRoutingType::StandardEffect);
  int n2 = graph.add_node("Delay", NodeRoutingType::StandardEffect);

  auto nodes = graph.get_nodes();
  // Connect output pin of Node 1 to input pin of Node 2
  int out_pin_1 = nodes[0].output_pin_ids[0];
  int in_pin_2 = nodes[1].input_pin_ids[0];

  int link_id = graph.add_link(out_pin_1, in_pin_2);
  ASSERT_TRUE(link_id != -1);

  const auto &sorted = graph.get_sorted_nodes();
  ASSERT_TRUE(sorted.size() == 2);
  ASSERT_TRUE(sorted[0] == n1); // Node 1 must process before Node 2
  ASSERT_TRUE(sorted[1] == n2);
}

// 2. Test Complex Parallel Splitting & Additive Merge Paths
TEST(audio_graph_parallel_split_merge) {
  AudioGraph graph;

  int p1 = graph.add_node("Splitter", NodeRoutingType::Splitter);
  int p2 = graph.add_node("Parallel Low Path", NodeRoutingType::StandardEffect);
  int p3 =
      graph.add_node("Parallel High Path", NodeRoutingType::StandardEffect);
  int m4 = graph.add_node("Merge Sum Node", NodeRoutingType::MergeSum);

  auto nodes = graph.get_nodes();

  // Link splits: Pedal 1 splits directly into both 2 and 3 using separate output pins
  int l1 = graph.add_link(nodes[0].output_pin_ids[0], nodes[1].input_pin_ids[0]);
  int l2 = graph.add_link(nodes[0].output_pin_ids[1], nodes[2].input_pin_ids[0]);
  
  ASSERT_TRUE(l1 != -1);
  ASSERT_TRUE(l2 != -1);

  nodes = graph.get_nodes();

  // FIX: Link merges directly to the universal input pin of the Merge node
  graph.add_link(nodes[1].output_pin_ids[0], nodes[3].input_pin_ids[0]);
  graph.add_link(nodes[2].output_pin_ids[0], nodes[3].input_pin_ids[1]);

  ASSERT_TRUE(graph.rebuild_topology());
  const auto &sorted = graph.get_sorted_nodes();

  ASSERT_TRUE(sorted.size() == 4);
  ASSERT_TRUE(sorted[0] == p1);
  ASSERT_TRUE(sorted[1] == p2 || sorted[1] == p3);
  ASSERT_TRUE(sorted[3] == m4);
}

// 3. Test Cyclic Feedback Guard Loop System
TEST(audio_graph_feedback_rejection) {
  AudioGraph graph;

  graph.add_node("Pedal A", NodeRoutingType::StandardEffect);
  graph.add_node("Pedal B", NodeRoutingType::StandardEffect);

  auto nodes = graph.get_nodes();
  // Create connection: A -> B
  graph.add_link(nodes[0].output_pin_ids[0], nodes[1].input_pin_ids[0]);

  nodes = graph.get_nodes();
  // Attempt illegal feedback connection loop: B -> A
  int invalid_link =
      graph.add_link(nodes[1].output_pin_ids[0], nodes[0].input_pin_ids[0]);

  // The graph tracking engine should decline the link, returning -1 to keep
  // loops out
  ASSERT_TRUE(invalid_link == -1);
  ASSERT_TRUE(graph.get_links().size() ==
              1); // Only the valid A -> B link is preserved
}

// 4. Test Actual Audio Mathematics (Splitting & Additive Merging)
TEST(audio_graph_dsp_processing) {
  AudioGraph graph;
  AudioGraphExecutor executor;
  executor.prepare(48000, 128); // Standard buffer allocation

  // We will use 3 empty "bypass" standard nodes and 1 merge node.
  // Since pedal pointers are nullptr, they act as pure passthroughs!
  int p1 = graph.add_node("Splitter", NodeRoutingType::Splitter);
  graph.add_node("Path A", NodeRoutingType::StandardEffect);
  graph.add_node("Path B", NodeRoutingType::StandardEffect);
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

  // Case A: Sinks (nB, nC, nD) all contribute. nB is silent, nC and nD
  // output 1.0f. Mixed output: 1.0f + 1.0f = 2.0f.
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

TEST(graph_preset_roundtrip_single_chain) {
  AudioGraph graph;
  int n1 = graph.add_node("Input", NodeRoutingType::StandardEffect);
  int n2 = graph.add_node("EQ", NodeRoutingType::StandardEffect);
  int n3 = graph.add_node("Reverb", NodeRoutingType::StandardEffect);
  int n4 = graph.add_node("Output", NodeRoutingType::StandardEffect);

  graph.set_node_as_input(n1, true);
  graph.set_node_as_output(n4, true);

  auto nodes = graph.get_nodes();
  graph.add_link(nodes[0].output_pin_ids[0], nodes[1].input_pin_ids[0]);
  nodes = graph.get_nodes();
  graph.add_link(nodes[1].output_pin_ids[0], nodes[2].input_pin_ids[0]);
  nodes = graph.get_nodes();
  graph.add_link(nodes[2].output_pin_ids[0], nodes[3].input_pin_ids[0]);

  std::string json_str = mock_save_graph(graph);

  AudioGraph loaded;
  ASSERT_TRUE(mock_load_graph(json_str, loaded));

  ASSERT_TRUE(loaded.get_nodes().size() == 4);
  ASSERT_TRUE(loaded.get_links().size() == 3);
}

TEST(graph_preset_roundtrip_parallel_amps) {
  AudioGraph graph;
  int input = graph.add_node("Input", NodeRoutingType::StandardEffect);
  int split = graph.add_node("Splitter", NodeRoutingType::Splitter);
  int ampA = graph.add_node("AmpA", NodeRoutingType::StandardEffect);
  int ampB = graph.add_node("AmpB", NodeRoutingType::StandardEffect);
  int mixer = graph.add_node("Mixer", NodeRoutingType::Mixer);
  int output = graph.add_node("Output", NodeRoutingType::StandardEffect);

  graph.set_node_as_input(input, true);
  graph.set_node_as_output(output, true);

  auto nodes = graph.get_nodes();
  graph.add_link(nodes[0].output_pin_ids[0],
                 nodes[1].input_pin_ids[0]); // Input -> Splitter

  nodes = graph.get_nodes();
  graph.add_link(nodes[1].output_pin_ids[0],
                 nodes[2].input_pin_ids[0]); // Split -> AmpA
  graph.add_link(nodes[1].output_pin_ids[1],
                 nodes[3].input_pin_ids[0]); // Split -> AmpB

  nodes = graph.get_nodes();
  graph.add_link(nodes[2].output_pin_ids[0],
                 nodes[4].input_pin_ids[0]); // AmpA -> Mixer
  graph.add_link(nodes[3].output_pin_ids[0],
                 nodes[4].input_pin_ids[1]); // AmpB -> Mixer

  nodes = graph.get_nodes();
  graph.add_link(nodes[4].output_pin_ids[0],
                 nodes[5].input_pin_ids[0]); // Mixer -> Output

  std::string json_str = mock_save_graph(graph);

  AudioGraph loaded;
  ASSERT_TRUE(mock_load_graph(json_str, loaded));
  ASSERT_TRUE(loaded.get_nodes().size() == 6);
  ASSERT_TRUE(loaded.get_links().size() == 6);
}

TEST(graph_preset_node_positions_preserved) {
  AudioGraph graph;
  int n1 = graph.add_node("Node1", NodeRoutingType::StandardEffect);
  graph.set_node_position(n1, 123.4f, 567.8f);

  std::string json_str = mock_save_graph(graph);

  AudioGraph loaded;
  ASSERT_TRUE(mock_load_graph(json_str, loaded));

  auto loaded_nodes = loaded.get_nodes();
  ASSERT_TRUE(loaded_nodes.size() == 1);

  float diff_x = std::abs(loaded_nodes[0].x - 123.4f);
  float diff_y = std::abs(loaded_nodes[0].y - 567.8f);
  ASSERT_TRUE(diff_x < 0.001f);
  ASSERT_TRUE(diff_y < 0.001f);
}

TEST(graph_preset_audio_identity_after_roundtrip) {
  AudioGraph graph;
  AudioGraphExecutor executor1;
  executor1.prepare(48000, 128);

  int n1 = graph.add_node("Input", NodeRoutingType::StandardEffect);
  graph.set_node_as_input(n1, true);
  graph.set_node_as_output(n1, true);

  graph.rebuild_topology();
  executor1.compile(graph);

  std::vector<float> in_buf(64, 0.5f);
  std::vector<float> out_buf1(64, 0.0f);
  executor1.process(in_buf.data(), out_buf1.data(), 64);

  std::string json_str = mock_save_graph(graph);

  AudioGraph loaded;
  ASSERT_TRUE(mock_load_graph(json_str, loaded));

  AudioGraphExecutor executor2;
  executor2.prepare(48000, 128);
  executor2.compile(loaded);

  std::vector<float> out_buf2(64, 0.0f);
  executor2.process(in_buf.data(), out_buf2.data(), 64);

  for (size_t i = 0; i < 64; ++i) {
    ASSERT_TRUE(out_buf1[i] == out_buf2[i]);
  }
}

TEST(graph_preset_legacy_linear_preset_loads) {
  AudioGraph loaded;
  PresetData p;
  p.name = "TestLegacy";
  PresetData::EffectData fx;
  fx.type = "Overdrive";
  fx.enabled = true;
  p.effects.push_back(fx);

  std::string json_str = to_json_ext(p);

  ASSERT_TRUE(mock_load_graph(json_str, loaded));
  ASSERT_TRUE(loaded.get_nodes().size() == 1);
  ASSERT_TRUE(loaded.get_nodes()[0].name == "Overdrive");
}

TEST(graph_preset_missing_node_handled_gracefully) {
  AudioGraph graph;
  int n1 = graph.add_node("A", NodeRoutingType::StandardEffect);
  int n2 = graph.add_node("B", NodeRoutingType::StandardEffect);

  auto nodes = graph.get_nodes();
  graph.add_link(nodes[0].output_pin_ids[0], nodes[1].input_pin_ids[0]);

  std::string json_str = mock_save_graph(graph);
  nlohmann::json j = nlohmann::json::parse(json_str);
  j["links"][0]["dest_pin_id"] = 99999; // Corrupt link

  AudioGraph loaded;
  ASSERT_FALSE(mock_load_graph(j.dump(), loaded));
}