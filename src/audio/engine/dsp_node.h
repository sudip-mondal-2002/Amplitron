#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Amplitron {

class Effect;

enum class NodeRoutingType { StandardEffect, Splitter, Mixer, MergeSum = Mixer };

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

    std::vector<float> input_gains;  // Gain for each input pin (Mixer only)
};

}  // namespace Amplitron
