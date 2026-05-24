#include "audio/audio_graph.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Amplitron {

int AudioGraph::add_node(const std::string &name, NodeRoutingType type,
                         std::shared_ptr<Effect> pedal) {
  DSPNode node;
  node.id = next_id_++; // Uses your unified member counter
  node.name = name;
  node.routing_type = type;
  node.pedal = pedal;

  // Dynamically configure pin structures using the same unified ID pool
  if (type == NodeRoutingType::Mixer || type == NodeRoutingType::MergeSum) {
    node.input_pin_ids.push_back(next_id_++);  // Input Pin Branch A
    node.input_pin_ids.push_back(next_id_++);  // Input Pin Branch B
    node.output_pin_ids.push_back(next_id_++); // 1 Output Pin
  } else if (type == NodeRoutingType::Splitter) {
    node.input_pin_ids.push_back(next_id_++);  // 1 Input Pin
    node.output_pin_ids.push_back(next_id_++); // Output Pin Branch A
    node.output_pin_ids.push_back(next_id_++); // Output Pin Branch B
  } else {
    node.input_pin_ids.push_back(next_id_++);
    node.output_pin_ids.push_back(next_id_++);
  }

  nodes_.push_back(node);

  // Auto-recompile topology order whenever a structural block changes
  rebuild_topology();

  return node.id;
}

int AudioGraph::add_link(int source_pin_id, int dest_pin_id) {
    // Prevent duplicate connections between the exact same pair of pins
    for (const auto& existing_link : links_) {
        if (existing_link.source_pin_id == source_pin_id && existing_link.dest_pin_id == dest_pin_id) {
            printf("add_link failed: duplicate link\n");
            return existing_link.id; 
        }
    }

    // Enforce that each input pin can only have ONE incoming link
    for (const auto& existing_link : links_) {
        if (existing_link.dest_pin_id == dest_pin_id) {
            printf("add_link failed: Input pin %d already in use!\n", dest_pin_id);
            return -1; // Pin already in use!
        }
    }

    // Enforce that ALL pins can only have ONE outgoing link
    int source_node_id = get_node_from_pin(source_pin_id);
    if (source_node_id != -1) {
        const DSPNode *src_node = find_node(source_node_id);
        if (src_node) {
            // Count existing outgoing links from this specific pin
            int out_count = 0;
            for (const auto &existing_link : links_) {
                if (existing_link.source_pin_id == source_pin_id) {
                    printf("add_link failed: Output pin %d already has an outgoing connection!\n", source_pin_id);
                    out_count++;
                }
            }
            if (out_count >= 1) {
                return -1; // Each output pin can only have 1 outgoing connection!
            }
        }
    }

    GraphLink link;
    link.id = next_id_++; // Uses your unified member counter
    link.source_pin_id = source_pin_id;
    link.dest_pin_id = dest_pin_id;
    
    links_.push_back(link);

    // Validate if the new patch wire forms an impossible audio loop feedback cycle
    if (!rebuild_topology()) {
        printf("add_link failed: forms an impossible audio loop feedback cycle!\n");
        // If a feedback loop is detected, pop the dangerous link back off to keep the engine safe
        links_.pop_back();
        rebuild_topology();
        return -1; 
    }

    return link.id;
}

void AudioGraph::set_node_as_input(int node_id, bool is_input) {
  for (auto &node : nodes_) {
    if (node.id == node_id) {
      node.is_graph_input = is_input;
      rebuild_topology();
      break;
    }
  }
}

void AudioGraph::set_node_as_output(int node_id, bool is_output) {
  for (auto &node : nodes_) {
    if (node.id == node_id) {
      node.is_graph_output = is_output;
      rebuild_topology();
      break;
    }
  }
}

void AudioGraph::set_node_position(int node_id, float x, float y) {
  for (auto &node : nodes_) {
    if (node.id == node_id) {
      node.x = x;
      node.y = y;
      break;
    }
  }
}

int AudioGraph::get_node_from_pin(int pin_id) const {
  // Search through all nodes to find which one owns the given Pin ID
  for (const auto &node : nodes_) {
    for (int p : node.input_pin_ids) {
      if (p == pin_id)
        return node.id;
    }
    for (int p : node.output_pin_ids) {
      if (p == pin_id)
        return node.id;
    }
  }
  return -1; // Pin ID not found in any registered node
}

bool AudioGraph::rebuild_topology() {
  // Kahn's algorithm or DFS to topologically sort the nodes based on links.
  // Since your test suite cases are already passing, we can use a basic
  // Kahn's sort dependency tracker to map links to execution order.

  sorted_node_ids_.clear();

  // 1. Forward Reachability BFS
  std::unordered_set<int> forward_reachable;
  std::vector<int> queue;
  for (const auto &node : nodes_) {
    if (node.is_graph_input) {
      queue.push_back(node.id);
      forward_reachable.insert(node.id);
    }
  }
  size_t head = 0;
  while (head < queue.size()) {
    int curr = queue[head++];
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
                           [&](const DSPNode &n) { return n.id == curr; });
    if (it != nodes_.end()) {
      for (int out_pin : it->output_pin_ids) {
        for (const auto &link : links_) {
          if (link.source_pin_id == out_pin) {
            int dest = get_node_from_pin(link.dest_pin_id);
            if (dest != -1 &&
                forward_reachable.find(dest) == forward_reachable.end()) {
              forward_reachable.insert(dest);
              queue.push_back(dest);
            }
          }
        }
      }
    }
  }

  // 2. Backward Reachability BFS
  std::unordered_set<int> backward_reachable;
  queue.clear();
  for (const auto &node : nodes_) {
    if (node.is_graph_output) {
      queue.push_back(node.id);
      backward_reachable.insert(node.id);
    }
  }
  head = 0;
  while (head < queue.size()) {
    int curr = queue[head++];
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
                           [&](const DSPNode &n) { return n.id == curr; });
    if (it != nodes_.end()) {
      for (int in_pin : it->input_pin_ids) {
        for (const auto &link : links_) {
          if (link.dest_pin_id == in_pin) {
            int src = get_node_from_pin(link.source_pin_id);
            if (src != -1 &&
                backward_reachable.find(src) == backward_reachable.end()) {
              backward_reachable.insert(src);
              queue.push_back(src);
            }
          }
        }
      }
    }
  }

  // 3. Update is_reachable for all nodes
  for (auto &node : nodes_) {
    node.is_reachable = (forward_reachable.count(node.id) > 0 &&
                         backward_reachable.count(node.id) > 0);
  }

  std::unordered_map<int, int> in_degree;

  // Initialize in-degree count for all active nodes
  for (const auto &node : nodes_) {
    in_degree[node.id] = 0;
  }

  // Calculate how many incoming cables are hooked up to each node
  for (const auto &link : links_) {
    int dest_node = get_node_from_pin(link.dest_pin_id);
    if (dest_node != -1) {
      in_degree[dest_node]++;
    }
  }

  // Gather all source nodes that have 0 dependencies
  std::vector<int> process_queue;
  for (const auto &node : nodes_) {
    if (in_degree[node.id] == 0) {
      process_queue.push_back(node.id);
    }
  }

  // Topologically extract nodes from the dependency queue
  head = 0;
  while (head < process_queue.size()) {
    int current_node_id = process_queue[head++];
    sorted_node_ids_.push_back(current_node_id);

    // Decrement dependencies for downstream targets linked to this node
    for (const auto &node : nodes_) {
      if (node.id != current_node_id)
        continue;

      for (int out_pin : node.output_pin_ids) {
        for (const auto &link : links_) {
          if (link.source_pin_id == out_pin) {
            int target_node = get_node_from_pin(link.dest_pin_id);
            if (target_node != -1) {
              in_degree[target_node]--;
              if (in_degree[target_node] == 0) {
                process_queue.push_back(target_node);
              }
            }
          }
        }
      }
    }
  }

  // If the sorted list length doesn't match total nodes, an impossible feedback
  // loop exists!
  if (sorted_node_ids_.size() != nodes_.size()) {
    return false; // Rejects connection modifications to protect engine
                  // stability
  }

  return true; // Topology built successfully!
}

bool AudioGraph::remove_node(int node_id) {
  auto it =
      std::find_if(nodes_.begin(), nodes_.end(),
                   [node_id](const DSPNode &n) { return n.id == node_id; });

  if (it != nodes_.end()) {
    // 1. Destroy all cables attached to this node's Input Pins
    for (int pin : it->input_pin_ids) {
      links_.erase(std::remove_if(links_.begin(), links_.end(),
                                  [pin](const GraphLink &l) {
                                    return l.dest_pin_id == pin;
                                  }),
                   links_.end());
    }
    // 2. Destroy all cables attached to this node's Output Pins
    for (int pin : it->output_pin_ids) {
      links_.erase(std::remove_if(links_.begin(), links_.end(),
                                  [pin](const GraphLink &l) {
                                    return l.source_pin_id == pin;
                                  }),
                   links_.end());
    }

    // 3. Erase the node and recompile the audio thread topology
    nodes_.erase(it);
    rebuild_topology();
    return true;
  }
  return false;
}
bool AudioGraph::remove_link(int link_id) {
  auto it =
      std::remove_if(links_.begin(), links_.end(),
                     [link_id](const GraphLink &l) { return l.id == link_id; });
  if (it != links_.end()) {
    links_.erase(it, links_.end());
    rebuild_topology();
    return true;
  }
  return false;
}

const DSPNode *AudioGraph::find_node(int node_id) const {
  for (const auto &node : nodes_) {
    if (node.id == node_id)
      return &node;
  }
  return nullptr;
}

} // namespace Amplitron
