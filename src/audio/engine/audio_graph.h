#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Amplitron {

// Forward declaration of your existing base pedal agent interface
class Effect;

enum class NodeRoutingType {
  StandardEffect,
  Splitter,
  Mixer,
  MergeSum = Mixer
};

struct DSPNode {
  int id;
  std::string name;
  NodeRoutingType routing_type;
  std::shared_ptr<Effect> pedal;

  std::vector<int> input_pin_ids;
  std::vector<int> output_pin_ids;

  bool is_graph_input = false;
  bool is_graph_output = false;
  bool is_reachable = true;

  float x = 0.0f;
  float y = 0.0f;
};

struct GraphLink {
  int id;
  int source_pin_id; // Mapping from Output Pin
  int dest_pin_id;   // Mapping to Input Pin
};

class AudioGraph {
public:
  AudioGraph() = default;
  ~AudioGraph() = default;

  // Graph Construction Interface
  int add_node(const std::string &name, NodeRoutingType type,
               std::shared_ptr<Effect> pedal = nullptr, int num_inputs = 0);
  bool remove_node(int node_id);
  int add_link(int source_pin_id, int dest_pin_id);
  bool remove_link(int link_id);

  void set_node_as_input(int node_id, bool is_input);
  void set_node_as_output(int node_id, bool is_output);
  void set_node_position(int node_id, float x, float y);

  // Topological Order & Loop Validation Core
  bool rebuild_topology();

  // For Undo/Redo System (forces cache reload)
  void restore_node(const DSPNode& node);
  void restore_link(const GraphLink& link);

  // Accessors
  const std::vector<int> &get_sorted_nodes() const { return sorted_node_ids_; }
  const std::vector<DSPNode> &get_nodes() const { return nodes_; }
  const std::vector<GraphLink> &get_links() const { return links_; }

  int get_node_from_pin(int pin_id) const;
  const DSPNode *find_node(int node_id) const;

private:
  size_t get_node_index(int node_id) const;

  int next_id_ = 1;
  std::vector<DSPNode> nodes_;
  std::vector<GraphLink> links_;
  std::vector<int> sorted_node_ids_;
};

} // namespace Amplitron